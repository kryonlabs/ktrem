#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "terminal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

enum {
    STATE_TEXT,
    STATE_ESCAPE,
    STATE_CSI,
    STATE_OSC,
    STATE_OSC_ESCAPE,
    STATE_DCS,
    STATE_DCS_ESCAPE,
    STATE_CHARSET
};

#define MAX_DCS_BYTES 4194304
#define MAX_SIXEL_DIMENSION 4096

enum {
    CHARSET_US_ASCII,
    CHARSET_DEC_SPECIAL
};

static int append_utf8(char *out, int out_size, int *used, unsigned int cp);
static void clamp_cursor(TerminalState *terminal);

static int clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static Cell blank_cell(const TerminalState *terminal)
{
    Cell cell;

    cell.codepoint = ' ';
    cell.combining = 0;
    cell.fg = terminal != NULL ? terminal->current_fg : COLOR_DEFAULT;
    cell.bg = terminal != NULL ? terminal->current_bg : COLOR_DEFAULT;
    cell.underline =
        terminal != NULL ? terminal->current_underline : COLOR_DEFAULT;
    cell.hyperlink = terminal != NULL ? terminal->current_hyperlink : 0;
    cell.style = terminal != NULL ? (unsigned char)terminal->current_style : 0;
    return cell;
}

static Cell *screen_cells(TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

static const Cell *screen_cells_const(const TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

static unsigned char *screen_wrapped(TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_wrapped
                                                          : terminal->main_wrapped;
}

static void push_scrollback(TerminalState *terminal, const Cell *row, int wrapped);

static void remove_sixel_image(TerminalState *terminal, int index)
{
    SixelImage *image;

    if(terminal == NULL || index < 0 || index >= terminal->sixel_count)
        return;
    image = terminal->sixel_images + index;
    if(image->pixels != NULL) {
        terminal->sixel_total_pixels -= image->width * image->height;
        if(terminal->sixel_total_pixels < 0)
            terminal->sixel_total_pixels = 0;
        free(image->pixels);
    }
    if(index + 1 < terminal->sixel_count)
        memmove(terminal->sixel_images + index,
                terminal->sixel_images + index + 1,
                (size_t)(terminal->sixel_count - index - 1) *
                    sizeof(SixelImage));
    terminal->sixel_count--;
}

static void clear_sixel_images(TerminalState *terminal, int alternate_filter)
{
    int i;

    if(terminal == NULL)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        if(alternate_filter < 0 ||
           terminal->sixel_images[i].alternate_screen == alternate_filter)
            remove_sixel_image(terminal, i);
    }
}

static int sixel_image_intersects_range(const TerminalState *terminal,
                                        const SixelImage *image, int start,
                                        int end)
{
    int rows_used;
    int cols_used;
    int row;

    if(terminal == NULL || image == NULL || terminal->cols <= 0 ||
       image->alternate_screen != terminal->alternate_screen || start >= end)
        return 0;
    rows_used = (image->height + 5) / 6;
    cols_used = (image->width + 5) / 6;
    if(rows_used < 1)
        rows_used = 1;
    if(cols_used < 1)
        cols_used = 1;
    for(row = image->row; row < image->row + rows_used; row++) {
        int row_start;
        int row_end;

        if(row < 0 || row >= terminal->rows)
            continue;
        row_start = row * terminal->cols + image->col;
        row_end = row_start + cols_used;
        if(row_start < row * terminal->cols)
            row_start = row * terminal->cols;
        if(row_end > (row + 1) * terminal->cols)
            row_end = (row + 1) * terminal->cols;
        if(end > row_start && start < row_end)
            return 1;
    }
    return 0;
}

static void clear_sixel_images_in_range(TerminalState *terminal, int start,
                                        int end)
{
    int i;

    if(terminal == NULL)
        return;
    start = clamp_int(start, 0, terminal->cols * terminal->rows);
    end = clamp_int(end, 0, terminal->cols * terminal->rows);
    if(start >= end)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        if(sixel_image_intersects_range(terminal, terminal->sixel_images + i,
                                        start, end))
            remove_sixel_image(terminal, i);
    }
}

static void shift_sixel_images(TerminalState *terminal, int top, int bottom,
                               int delta)
{
    int i;

    if(terminal == NULL || delta == 0)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        SixelImage *image = terminal->sixel_images + i;

        if(image->alternate_screen != terminal->alternate_screen ||
           image->row < top || image->row > bottom)
            continue;
        image->row += delta;
        if(image->row < top || image->row > bottom)
            remove_sixel_image(terminal, i);
    }
}

static void reset_style(TerminalState *terminal)
{
    terminal->current_fg = COLOR_DEFAULT;
    terminal->current_bg = COLOR_DEFAULT;
    terminal->current_underline = COLOR_DEFAULT;
    terminal->current_style = 0;
}

static void reset_charsets(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->g0_charset = CHARSET_US_ASCII;
    terminal->g1_charset = CHARSET_US_ASCII;
    terminal->active_charset = 0;
    terminal->pending_charset = 0;
}

static void save_cursor_state(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->saved_col = terminal->cursor_col;
    terminal->saved_row = terminal->cursor_row;
    terminal->saved_fg = terminal->current_fg;
    terminal->saved_bg = terminal->current_bg;
    terminal->saved_underline = terminal->current_underline;
    terminal->saved_style = terminal->current_style;
    terminal->saved_hyperlink = terminal->current_hyperlink;
    terminal->saved_g0_charset = terminal->g0_charset;
    terminal->saved_g1_charset = terminal->g1_charset;
    terminal->saved_active_charset = terminal->active_charset;
    terminal->saved_origin_mode = terminal->origin_mode;
    terminal->saved_autowrap = terminal->autowrap;
    terminal->saved_insert_mode = terminal->insert_mode;
}

static void restore_cursor_state(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->cursor_col = terminal->saved_col;
    terminal->cursor_row = terminal->saved_row;
    terminal->current_fg = terminal->saved_fg;
    terminal->current_bg = terminal->saved_bg;
    terminal->current_underline = terminal->saved_underline;
    terminal->current_style = terminal->saved_style;
    terminal->current_hyperlink = terminal->saved_hyperlink;
    terminal->g0_charset = terminal->saved_g0_charset;
    terminal->g1_charset = terminal->saved_g1_charset;
    terminal->active_charset = terminal->saved_active_charset;
    terminal->origin_mode = terminal->saved_origin_mode;
    terminal->autowrap = terminal->saved_autowrap;
    terminal->insert_mode = terminal->saved_insert_mode;
    clamp_cursor(terminal);
}

static void reset_tab_stops(TerminalState *terminal)
{
    int col;

    if(terminal == NULL || terminal->tab_stops == NULL)
        return;
    memset(terminal->tab_stops, 0, (size_t)terminal->cols);
    for(col = 8; col < terminal->cols; col += 8)
        terminal->tab_stops[col] = 1;
}

static int codepoint_width(unsigned int cp)
{
    if(cp == 0)
        return 0;
    if(cp < 32 || (cp >= 0x7f && cp < 0xa0))
        return 0;
    if((cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
       (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
       (cp >= 0xfe20 && cp <= 0xfe2f))
        return 0;
    if((cp >= 0x1100 && cp <= 0x115f) || cp == 0x2329 || cp == 0x232a ||
       (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
       (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
       (cp >= 0xfe10 && cp <= 0xfe19) || (cp >= 0xfe30 && cp <= 0xfe6f) ||
       (cp >= 0xff00 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
       (cp >= 0x1f300 && cp <= 0x1f64f) ||
       (cp >= 0x1f900 && cp <= 0x1f9ff))
        return 2;
    return 1;
}

static unsigned int translate_charset(const TerminalState *terminal,
                                      unsigned int codepoint)
{
    int charset;

    if(terminal == NULL || codepoint < 0x20 || codepoint > 0x7e)
        return codepoint;
    charset = terminal->active_charset == 1 ? terminal->g1_charset
                                            : terminal->g0_charset;
    if(charset != CHARSET_DEC_SPECIAL)
        return codepoint;
    switch(codepoint) {
    case '`':
        return 0x25c6;
    case 'a':
        return 0x2592;
    case 'b':
        return 0x2409;
    case 'c':
        return 0x240c;
    case 'd':
        return 0x240d;
    case 'e':
        return 0x240a;
    case 'f':
        return 0x00b0;
    case 'g':
        return 0x00b1;
    case 'h':
        return 0x2424;
    case 'i':
        return 0x240b;
    case 'j':
        return 0x2518;
    case 'k':
        return 0x2510;
    case 'l':
        return 0x250c;
    case 'm':
        return 0x2514;
    case 'n':
        return 0x253c;
    case 'o':
        return 0x23ba;
    case 'p':
        return 0x23bb;
    case 'q':
        return 0x2500;
    case 'r':
        return 0x23bc;
    case 's':
        return 0x23bd;
    case 't':
        return 0x251c;
    case 'u':
        return 0x2524;
    case 'v':
        return 0x2534;
    case 'w':
        return 0x252c;
    case 'x':
        return 0x2502;
    case 'y':
        return 0x2264;
    case 'z':
        return 0x2265;
    case '{':
        return 0x03c0;
    case '|':
        return 0x2260;
    case '}':
        return 0x00a3;
    case '~':
        return 0x00b7;
    default:
        return codepoint;
    }
}

void terminal_init(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    memset(terminal, 0, sizeof(*terminal));
    terminal->fd = -1;
    terminal->cursor_visible = 1;
    terminal->autowrap = 1;
    terminal->insert_mode = 0;
    terminal->scrollback_limit = SCROLLBACK_LIMIT;
    terminal->default_fg = COLOR_DEFAULT;
    terminal->default_bg = COLOR_DEFAULT;
    terminal->cursor_color = COLOR_DEFAULT;
    terminal->base_fg = COLOR_DEFAULT;
    terminal->base_bg = COLOR_DEFAULT;
    terminal->base_cursor_color = COLOR_DEFAULT;
    terminal->cursor_style = TERMINAL_CURSOR_DEFAULT;
    terminal->current_hyperlink = 0;
    terminal->hyperlink_count = 0;
    reset_charsets(terminal);
    for(i = 0; i < 256; i++)
        terminal->palette_overrides[i] = COLOR_DEFAULT;
    reset_style(terminal);
    terminal->saved_fg = terminal->current_fg;
    terminal->saved_bg = terminal->current_bg;
    terminal->saved_underline = terminal->current_underline;
    terminal->saved_style = terminal->current_style;
    terminal->saved_hyperlink = terminal->current_hyperlink;
    terminal->saved_g0_charset = terminal->g0_charset;
    terminal->saved_g1_charset = terminal->g1_charset;
    terminal->saved_active_charset = terminal->active_charset;
    terminal->saved_origin_mode = terminal->origin_mode;
    terminal->saved_autowrap = terminal->autowrap;
    terminal->saved_insert_mode = terminal->insert_mode;
}

static void clear_cell_block(TerminalState *terminal, Cell *cells)
{
    Cell blank;
    int i;
    int count;

    if(terminal == NULL || cells == NULL)
        return;
    blank = blank_cell(terminal);
    count = terminal->cols * terminal->rows;
    for(i = 0; i < count; i++)
        cells[i] = blank;
}

static void clear_row(TerminalState *terminal, int row)
{
    Cell *cells = screen_cells(terminal);
    unsigned char *wrapped = screen_wrapped(terminal);
    Cell blank;
    int col;

    if(terminal == NULL || cells == NULL || row < 0 || row >= terminal->rows)
        return;
    clear_sixel_images_in_range(terminal, row * terminal->cols,
                                (row + 1) * terminal->cols);
    blank = blank_cell(terminal);
    for(col = 0; col < terminal->cols; col++)
        cells[row * terminal->cols + col] = blank;
    if(wrapped != NULL)
        wrapped[row] = 0;
}

static void clear_screen(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    clear_cell_block(terminal, screen_cells(terminal));
    if(screen_wrapped(terminal) != NULL)
        memset(screen_wrapped(terminal), 0, (size_t)terminal->rows);
    terminal->cursor_col = 0;
    terminal->cursor_row = 0;
    clear_sixel_images(terminal, terminal->alternate_screen);
}

static void clear_alternate_screen(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    clear_cell_block(terminal, terminal->alt_cells);
    if(terminal->alt_wrapped != NULL)
        memset(terminal->alt_wrapped, 0, (size_t)terminal->rows);
    clear_sixel_images(terminal, 1);
}

static void clear_scrollback(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->scrollback_count = 0;
    terminal->scrollback_head = 0;
    if(terminal->scrollback_wrapped != NULL &&
       terminal->scrollback_capacity > 0)
        memset(terminal->scrollback_wrapped, 0,
               (size_t)terminal->scrollback_capacity);
}

static int allocate_scrollback(TerminalState *terminal)
{
    Cell *scrollback;
    unsigned char *wrapped;
    int limit;

    if(terminal == NULL)
        return 0;
    limit = terminal->scrollback_limit > 0 ? terminal->scrollback_limit
                                           : SCROLLBACK_LIMIT;
    scrollback = realloc(terminal->scrollback,
                         (size_t)(limit * terminal->cols) *
                             sizeof(Cell));
    if(scrollback == NULL)
        return 0;
    wrapped = realloc(terminal->scrollback_wrapped, (size_t)limit);
    if(wrapped == NULL)
        return 0;
    terminal->scrollback = scrollback;
    terminal->scrollback_wrapped = wrapped;
    terminal->scrollback_capacity = limit;
    if(terminal->scrollback_count > terminal->scrollback_capacity)
        terminal->scrollback_count = terminal->scrollback_capacity;
    if(terminal->scrollback_head >= terminal->scrollback_capacity)
        terminal->scrollback_head = 0;
    if(terminal->scrollback_count == 0)
        memset(terminal->scrollback_wrapped, 0, (size_t)terminal->scrollback_capacity);
    return 1;
}

static int row_text_end(const Cell *cells, int cols, int row, int keep_full)
{
    int end = cols;

    if(cells == NULL || cols <= 0 || row < 0)
        return 0;
    if(keep_full)
        return cols;
    while(end > 0 && cells[row * cols + end - 1].codepoint == ' ')
        end--;
    return end;
}

static void append_reflow_cell(Cell *rows, unsigned char *wrapped, int cols,
                               int max_rows, int *out_row, int *out_col,
                               Cell cell)
{
    if(rows == NULL || wrapped == NULL || out_row == NULL || out_col == NULL ||
       cols <= 0 || max_rows <= 0)
        return;
    if(*out_col >= cols) {
        if(*out_row >= 0 && *out_row < max_rows)
            wrapped[*out_row] = 1;
        (*out_row)++;
        *out_col = 0;
    }
    if(*out_row >= max_rows)
        return;
    rows[*out_row * cols + *out_col] = cell;
    (*out_col)++;
}

static void finish_reflow_line(int *out_row, int *out_col, int max_rows)
{
    if(out_row == NULL || out_col == NULL)
        return;
    if(*out_row < max_rows)
        (*out_row)++;
    *out_col = 0;
}

static int reflow_main_screen(TerminalState *terminal, const Cell *old_cells,
                              const unsigned char *old_wrapped, int old_cols,
                              int old_rows)
{
    Cell *lines;
    unsigned char *wrapped;
    int max_lines;
    int out_row = 0;
    int out_col = 0;
    int row;
    int copy_rows;
    int start_row;

    if(terminal == NULL || terminal->main_cells == NULL || old_cells == NULL ||
       old_cols <= 0 || old_rows <= 0 || terminal->cols <= 0)
        return 0;
    max_lines = old_rows * ((old_cols + terminal->cols - 1) / terminal->cols + 2);
    if(max_lines < terminal->rows)
        max_lines = terminal->rows;
    lines = calloc((size_t)max_lines * (size_t)terminal->cols, sizeof(Cell));
    wrapped = calloc((size_t)max_lines, 1);
    if(lines == NULL || wrapped == NULL) {
        free(lines);
        free(wrapped);
        return 0;
    }
    for(row = 0; row < max_lines; row++) {
        int col;
        Cell blank = blank_cell(terminal);

        for(col = 0; col < terminal->cols; col++)
            lines[row * terminal->cols + col] = blank;
    }
    for(row = 0; row < old_rows; row++) {
        int soft = old_wrapped != NULL && old_wrapped[row];
        int end = row_text_end(old_cells, old_cols, row, soft);
        int col;

        for(col = 0; col < end; col++)
            append_reflow_cell(lines, wrapped, terminal->cols, max_lines,
                               &out_row, &out_col,
                               old_cells[row * old_cols + col]);
        if(!soft)
            finish_reflow_line(&out_row, &out_col, max_lines);
    }
    if(out_col > 0)
        finish_reflow_line(&out_row, &out_col, max_lines);
    if(out_row <= 0) {
        free(lines);
        free(wrapped);
        return 1;
    }
    copy_rows = out_row < terminal->rows ? out_row : terminal->rows;
    start_row = out_row - copy_rows;
    memcpy(terminal->main_cells,
           lines + start_row * terminal->cols,
           (size_t)copy_rows * (size_t)terminal->cols * sizeof(Cell));
    memcpy(terminal->main_wrapped, wrapped + start_row, (size_t)copy_rows);
    terminal->cursor_row = copy_rows - 1;
    terminal->cursor_col = 0;
    if(copy_rows > 0) {
        int end = row_text_end(terminal->main_cells, terminal->cols,
                               copy_rows - 1, 1);

        terminal->cursor_col = clamp_int(end, 0, terminal->cols - 1);
    }
    free(lines);
    free(wrapped);
    return 1;
}

static void reflow_scrollback(TerminalState *terminal, const Cell *old_cells,
                              const unsigned char *old_wrapped, int old_cols,
                              int old_count, int old_head, int old_capacity)
{
    Cell *lines;
    unsigned char *wrapped;
    int max_lines;
    int out_row = 0;
    int out_col = 0;
    int row;
    int index;

    if(terminal == NULL || old_cells == NULL || old_cols <= 0 ||
       old_count <= 0 || old_capacity <= 0 || terminal->cols <= 0)
        return;
    max_lines =
        old_count * ((old_cols + terminal->cols - 1) / terminal->cols + 2);
    if(max_lines < old_count)
        max_lines = old_count;
    lines = calloc((size_t)max_lines * (size_t)terminal->cols, sizeof(Cell));
    wrapped = calloc((size_t)max_lines, 1);
    if(lines == NULL || wrapped == NULL) {
        free(lines);
        free(wrapped);
        return;
    }
    for(row = 0; row < max_lines; row++) {
        int col;
        Cell blank = blank_cell(terminal);

        for(col = 0; col < terminal->cols; col++)
            lines[row * terminal->cols + col] = blank;
    }
    for(index = 0; index < old_count; index++) {
        int physical = (old_head - old_count + index + old_capacity) %
                       old_capacity;
        int soft = old_wrapped != NULL && old_wrapped[physical];
        int end = row_text_end(old_cells, old_cols, physical, soft);
        int col;

        for(col = 0; col < end; col++)
            append_reflow_cell(lines, wrapped, terminal->cols, max_lines,
                               &out_row, &out_col,
                               old_cells[physical * old_cols + col]);
        if(!soft)
            finish_reflow_line(&out_row, &out_col, max_lines);
    }
    if(out_col > 0)
        finish_reflow_line(&out_row, &out_col, max_lines);
    for(row = 0; row < out_row; row++)
        push_scrollback(terminal, lines + row * terminal->cols,
                        wrapped[row]);
    free(lines);
    free(wrapped);
}

static int allocate_screen(TerminalState *terminal, int cols, int rows)
{
    Cell *old_main;
    Cell *old_alt;
    Cell *old_scrollback;
    unsigned char *old_main_wrapped;
    unsigned char *old_alt_wrapped;
    unsigned char *old_scrollback_wrapped;
    unsigned char *old_tabs;
    int old_cols;
    int old_rows;
    int old_scrollback_count;
    int old_scrollback_head;
    int old_scrollback_capacity;
    int copy_cols;
    int copy_rows;
    int main_reflowed = 0;
    int scrollback_reflow_needed;
    int allocated;
    int row;
    int col;

    cols = clamp_int(cols, 8, MAX_COLS);
    rows = clamp_int(rows, 4, MAX_ROWS);
    old_main = terminal->main_cells;
    old_alt = terminal->alt_cells;
    old_scrollback = terminal->scrollback;
    old_main_wrapped = terminal->main_wrapped;
    old_alt_wrapped = terminal->alt_wrapped;
    old_scrollback_wrapped = terminal->scrollback_wrapped;
    old_tabs = terminal->tab_stops;
    old_cols = terminal->cols;
    old_rows = terminal->rows;
    old_scrollback_count = terminal->scrollback_count;
    old_scrollback_head = terminal->scrollback_head;
    old_scrollback_capacity = terminal->scrollback_capacity;
    scrollback_reflow_needed =
        old_cols > 0 && old_cols != cols && old_scrollback != NULL &&
        old_scrollback_count > 0 && old_scrollback_capacity > 0;

    terminal->main_cells = calloc((size_t)(cols * rows), sizeof(Cell));
    terminal->alt_cells = calloc((size_t)(cols * rows), sizeof(Cell));
    terminal->main_wrapped = calloc((size_t)rows, 1);
    terminal->alt_wrapped = calloc((size_t)rows, 1);
    terminal->tab_stops = calloc((size_t)cols, 1);
    if(terminal->main_cells == NULL || terminal->alt_cells == NULL ||
       terminal->main_wrapped == NULL || terminal->alt_wrapped == NULL ||
       terminal->tab_stops == NULL) {
        free(terminal->main_cells);
        free(terminal->alt_cells);
        free(terminal->main_wrapped);
        free(terminal->alt_wrapped);
        free(terminal->tab_stops);
        terminal->main_cells = old_main;
        terminal->alt_cells = old_alt;
        terminal->main_wrapped = old_main_wrapped;
        terminal->alt_wrapped = old_alt_wrapped;
        terminal->tab_stops = old_tabs;
        return 0;
    }

    terminal->cols = cols;
    terminal->rows = rows;
    clear_cell_block(terminal, terminal->main_cells);
    clear_cell_block(terminal, terminal->alt_cells);

    copy_cols = old_cols < cols ? old_cols : cols;
    copy_rows = old_rows < rows ? old_rows : rows;
    if(old_main != NULL && old_cols > 0 && old_rows > 0 && old_cols != cols &&
       !terminal->alternate_screen &&
       reflow_main_screen(terminal, old_main, old_main_wrapped, old_cols,
                          old_rows))
        main_reflowed = 1;
    for(row = 0; row < copy_rows; row++) {
        for(col = 0; col < copy_cols; col++) {
            if(!main_reflowed && old_main != NULL)
                terminal->main_cells[row * cols + col] =
                    old_main[row * old_cols + col];
            if(old_alt != NULL)
                terminal->alt_cells[row * cols + col] =
                    old_alt[row * old_cols + col];
        }
        if(!main_reflowed && old_main_wrapped != NULL)
            terminal->main_wrapped[row] = old_main_wrapped[row];
        if(old_alt_wrapped != NULL)
            terminal->alt_wrapped[row] = old_alt_wrapped[row];
    }
    free(old_main);
    free(old_alt);
    free(old_main_wrapped);
    free(old_alt_wrapped);
    free(old_tabs);

    terminal->scroll_top = 0;
    terminal->scroll_bottom = rows - 1;
    terminal->cursor_col = clamp_int(terminal->cursor_col, 0, cols - 1);
    terminal->cursor_row = clamp_int(terminal->cursor_row, 0, rows - 1);
    if(scrollback_reflow_needed) {
        terminal->scrollback = NULL;
        terminal->scrollback_wrapped = NULL;
        terminal->scrollback_count = 0;
        terminal->scrollback_head = 0;
        terminal->scrollback_capacity = 0;
    }
    reset_tab_stops(terminal);
    allocated = allocate_scrollback(terminal);
    if(allocated && scrollback_reflow_needed) {
        reflow_scrollback(terminal, old_scrollback, old_scrollback_wrapped,
                          old_cols, old_scrollback_count,
                          old_scrollback_head, old_scrollback_capacity);
        free(old_scrollback);
        free(old_scrollback_wrapped);
    } else if(!allocated && scrollback_reflow_needed) {
        terminal->scrollback = old_scrollback;
        terminal->scrollback_wrapped = old_scrollback_wrapped;
        terminal->scrollback_count = old_scrollback_count;
        terminal->scrollback_head = old_scrollback_head;
        terminal->scrollback_capacity = old_scrollback_capacity;
    }
    return allocated;
}

static void set_window_size(int fd, int cols, int rows)
{
    struct winsize size;

    memset(&size, 0, sizeof(size));
    size.ws_col = (unsigned short)cols;
    size.ws_row = (unsigned short)rows;
    ioctl(fd, TIOCSWINSZ, &size);
}

static void push_scrollback(TerminalState *terminal, const Cell *row, int wrapped)
{
    int index;

    if(terminal == NULL || row == NULL || terminal->alternate_screen ||
       terminal->scrollback == NULL || terminal->scrollback_capacity <= 0)
        return;
    index = terminal->scrollback_head;
    memcpy(terminal->scrollback + index * terminal->cols, row,
           (size_t)terminal->cols * sizeof(Cell));
    if(terminal->scrollback_wrapped != NULL)
        terminal->scrollback_wrapped[index] = wrapped ? 1 : 0;
    terminal->scrollback_head =
        (terminal->scrollback_head + 1) % terminal->scrollback_capacity;
    if(terminal->scrollback_count < terminal->scrollback_capacity)
        terminal->scrollback_count++;
}

static void scroll_up(TerminalState *terminal, int top, int bottom, int count)
{
    Cell *cells = screen_cells(terminal);
    unsigned char *wrapped = screen_wrapped(terminal);
    int rows;
    int i;

    if(terminal == NULL || cells == NULL)
        return;
    top = clamp_int(top, 0, terminal->rows - 1);
    bottom = clamp_int(bottom, 0, terminal->rows - 1);
    if(top > bottom)
        return;
    rows = bottom - top + 1;
    count = clamp_int(count, 1, rows);
    for(i = 0; i < count; i++)
        push_scrollback(terminal, cells + (top + i) * terminal->cols,
                        wrapped != NULL ? wrapped[top + i] : 0);
    shift_sixel_images(terminal, top, bottom, -count);
    if(rows > count) {
        memmove(cells + top * terminal->cols,
                cells + (top + count) * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
        if(wrapped != NULL)
            memmove(wrapped + top, wrapped + top + count,
                    (size_t)(rows - count));
    }
    for(i = bottom - count + 1; i <= bottom; i++)
        clear_row(terminal, i);
}

static void scroll_down(TerminalState *terminal, int top, int bottom, int count)
{
    Cell *cells = screen_cells(terminal);
    unsigned char *wrapped = screen_wrapped(terminal);
    int rows;
    int i;

    if(terminal == NULL || cells == NULL)
        return;
    top = clamp_int(top, 0, terminal->rows - 1);
    bottom = clamp_int(bottom, 0, terminal->rows - 1);
    if(top > bottom)
        return;
    rows = bottom - top + 1;
    count = clamp_int(count, 1, rows);
    shift_sixel_images(terminal, top, bottom, count);
    if(rows > count) {
        memmove(cells + (top + count) * terminal->cols,
                cells + top * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
        if(wrapped != NULL)
            memmove(wrapped + top + count, wrapped + top,
                    (size_t)(rows - count));
    }
    for(i = top; i < top + count; i++)
        clear_row(terminal, i);
}

static void clamp_cursor(TerminalState *terminal)
{
    terminal->cursor_col = clamp_int(terminal->cursor_col, 0, terminal->cols - 1);
    if(terminal->origin_mode)
        terminal->cursor_row = clamp_int(terminal->cursor_row,
                                         terminal->scroll_top,
                                         terminal->scroll_bottom);
    else
        terminal->cursor_row = clamp_int(terminal->cursor_row, 0,
                                         terminal->rows - 1);
}

static void line_feed(TerminalState *terminal)
{
    if(terminal->cursor_row == terminal->scroll_bottom)
        scroll_up(terminal, terminal->scroll_top, terminal->scroll_bottom, 1);
    else
        terminal->cursor_row++;
    clamp_cursor(terminal);
}

static void erase_range(TerminalState *terminal, int start, int end)
{
    Cell *cells = screen_cells(terminal);
    Cell blank;
    int i;
    int limit;

    if(terminal == NULL || cells == NULL)
        return;
    limit = terminal->cols * terminal->rows;
    start = clamp_int(start, 0, limit);
    end = clamp_int(end, 0, limit);
    if(start >= end)
        return;
    clear_sixel_images_in_range(terminal, start, end);
    blank = blank_cell(terminal);
    for(i = start; i < end; i++)
        cells[i] = blank;
}

static void insert_blank_chars(TerminalState *terminal, int count)
{
    Cell *cells = screen_cells(terminal);
    Cell blank;
    int row_start;
    int available;
    int col;

    if(terminal == NULL || cells == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = clamp_int(count, 1, available);
    row_start = terminal->cursor_row * terminal->cols;
    if(available > count) {
        memmove(cells + row_start + terminal->cursor_col + count,
                cells + row_start + terminal->cursor_col,
                (size_t)(available - count) * sizeof(Cell));
    }
    blank = blank_cell(terminal);
    for(col = 0; col < count; col++)
        cells[row_start + terminal->cursor_col + col] = blank;
}

static void delete_chars(TerminalState *terminal, int count)
{
    Cell *cells = screen_cells(terminal);
    Cell blank;
    int row_start;
    int available;
    int col;

    if(terminal == NULL || cells == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = clamp_int(count, 1, available);
    row_start = terminal->cursor_row * terminal->cols;
    if(available > count) {
        memmove(cells + row_start + terminal->cursor_col,
                cells + row_start + terminal->cursor_col + count,
                (size_t)(available - count) * sizeof(Cell));
    }
    blank = blank_cell(terminal);
    for(col = terminal->cols - count; col < terminal->cols; col++)
        cells[row_start + col] = blank;
}

static void erase_chars(TerminalState *terminal, int count)
{
    int available;

    if(terminal == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = clamp_int(count, 1, available);
    erase_range(terminal, terminal->cursor_row * terminal->cols +
                              terminal->cursor_col,
                terminal->cursor_row * terminal->cols +
                    terminal->cursor_col + count);
}

static int next_tab_stop(const TerminalState *terminal)
{
    int col;

    if(terminal == NULL)
        return 0;
    for(col = terminal->cursor_col + 1; col < terminal->cols; col++) {
        if(terminal->tab_stops != NULL && terminal->tab_stops[col])
            return col;
    }
    return terminal->cols - 1;
}

static void put_codepoint(TerminalState *terminal, unsigned int codepoint)
{
    Cell *cells = screen_cells(terminal);
    Cell cell;
    int width;
    int target_col;
    int target_row;

    if(terminal == NULL || cells == NULL)
        return;
    codepoint = translate_charset(terminal, codepoint);
    width = codepoint_width(codepoint);
    if(width <= 0) {
        if(codepoint < 0x300)
            return;
        target_col = terminal->cursor_col - 1;
        target_row = terminal->cursor_row;
        if(target_col < 0) {
            target_row--;
            target_col = terminal->cols - 1;
        }
        if(target_row < 0 || target_col < 0)
            return;
        cell = cells[target_row * terminal->cols + target_col];
        if((cell.style & STYLE_WIDE_CONT) != 0 && target_col > 0)
            target_col--;
        if(cells[target_row * terminal->cols + target_col].codepoint != 0 &&
           (cells[target_row * terminal->cols + target_col].style &
            STYLE_WIDE_CONT) == 0)
            cells[target_row * terminal->cols + target_col].combining =
                codepoint;
        return;
    }
    if(terminal->cursor_col >= terminal->cols ||
       terminal->cursor_col + width > terminal->cols) {
        unsigned char *wrapped = screen_wrapped(terminal);

        if(terminal->autowrap) {
            if(wrapped != NULL && terminal->cursor_row >= 0 &&
               terminal->cursor_row < terminal->rows)
                wrapped[terminal->cursor_row] = 1;
            terminal->cursor_col = 0;
            line_feed(terminal);
        } else {
            terminal->cursor_col = terminal->cols - width;
            if(terminal->cursor_col < 0)
                terminal->cursor_col = 0;
        }
    }
    clamp_cursor(terminal);
    if(terminal->insert_mode)
        insert_blank_chars(terminal, width);
    cell.codepoint = codepoint >= 32 ? codepoint : '?';
    cell.combining = 0;
    cell.fg = terminal->current_fg;
    cell.bg = terminal->current_bg;
    cell.underline = terminal->current_underline;
    cell.hyperlink = terminal->current_hyperlink;
    cell.style = (unsigned char)terminal->current_style;
    cells[terminal->cursor_row * terminal->cols + terminal->cursor_col] = cell;
    if(width == 2 && terminal->cursor_col + 1 < terminal->cols) {
        Cell cont = blank_cell(terminal);

        cont.codepoint = 0;
        cont.style = (unsigned char)(cont.style | STYLE_WIDE_CONT);
        cells[terminal->cursor_row * terminal->cols + terminal->cursor_col + 1] =
            cont;
    }
    terminal->cursor_col += width;
}

static int csi_arg(const TerminalState *terminal, int index, int fallback)
{
    if(index < 0 || index >= terminal->csi_count ||
       terminal->csi_args[index] == 0)
        return fallback;
    return terminal->csi_args[index];
}

static int cursor_row_from_arg(const TerminalState *terminal, int index,
                               int fallback)
{
    int row;

    if(terminal == NULL)
        return 0;
    row = csi_arg(terminal, index, fallback) - 1;
    if(terminal->origin_mode)
        row += terminal->scroll_top;
    return row;
}

static void apply_style(TerminalState *terminal)
{
    int i;

    if(terminal->csi_count == 0) {
        reset_style(terminal);
        return;
    }
    for(i = 0; i < terminal->csi_count; i++) {
        int arg = terminal->csi_args[i];

        if(arg == 0)
            reset_style(terminal);
        else if(arg == 1)
            terminal->current_style |= STYLE_BOLD;
        else if(arg == 3)
            terminal->current_style |= STYLE_ITALIC;
        else if(arg == 4)
            terminal->current_style |= STYLE_UNDERLINE;
        else if(arg == 7)
            terminal->current_style |= STYLE_INVERSE;
        else if(arg == 9)
            terminal->current_style |= STYLE_STRIKE;
        else if(arg == 22)
            terminal->current_style &= ~STYLE_BOLD;
        else if(arg == 23)
            terminal->current_style &= ~STYLE_ITALIC;
        else if(arg == 24)
            terminal->current_style &= ~STYLE_UNDERLINE;
        else if(arg == 27)
            terminal->current_style &= ~STYLE_INVERSE;
        else if(arg == 29)
            terminal->current_style &= ~STYLE_STRIKE;
        else if(arg == 39)
            terminal->current_fg = COLOR_DEFAULT;
        else if(arg == 49)
            terminal->current_bg = COLOR_DEFAULT;
        else if(arg == 59)
            terminal->current_underline = COLOR_DEFAULT;
        else if(arg >= 30 && arg <= 37)
            terminal->current_fg = arg - 30;
        else if(arg >= 40 && arg <= 47)
            terminal->current_bg = arg - 40;
        else if(arg >= 90 && arg <= 97)
            terminal->current_fg = 8 + arg - 90;
        else if(arg >= 100 && arg <= 107)
            terminal->current_bg = 8 + arg - 100;
        else if((arg == 38 || arg == 48 || arg == 58) &&
                i + 2 < terminal->csi_count &&
                terminal->csi_args[i + 1] == 5) {
            if(arg == 38)
                terminal->current_fg = terminal->csi_args[i + 2] & 255;
            else if(arg == 48)
                terminal->current_bg = terminal->csi_args[i + 2] & 255;
            else
                terminal->current_underline = terminal->csi_args[i + 2] & 255;
            i += 2;
        } else if((arg == 38 || arg == 48 || arg == 58) &&
                  i + 4 < terminal->csi_count &&
                  terminal->csi_args[i + 1] == 2) {
            int offset = (i + 5 < terminal->csi_count &&
                          terminal->csi_args[i + 2] == 0)
                             ? 3
                             : 2;
            int rgb = ((terminal->csi_args[i + offset] & 255) << 16) |
                      ((terminal->csi_args[i + offset + 1] & 255) << 8) |
                      (terminal->csi_args[i + offset + 2] & 255);

            if(arg == 38)
                terminal->current_fg = COLOR_TRUE_RGB | rgb;
            else if(arg == 48)
                terminal->current_bg = COLOR_TRUE_RGB | rgb;
            else
                terminal->current_underline = COLOR_TRUE_RGB | rgb;
            i += offset + 2;
        }
    }
}

static void set_private_mode(TerminalState *terminal, int mode, int enabled)
{
    if(mode == 25) {
        terminal->cursor_visible = enabled ? 1 : 0;
    } else if(mode == 6) {
        terminal->origin_mode = enabled ? 1 : 0;
        terminal->cursor_col = 0;
        terminal->cursor_row = terminal->origin_mode ? terminal->scroll_top : 0;
    } else if(mode == 7) {
        terminal->autowrap = enabled ? 1 : 0;
    } else if(mode == 1) {
        terminal->application_cursor_keys = enabled ? 1 : 0;
    } else if(mode == 9 || mode == 1000 || mode == 1002 || mode == 1003) {
        terminal->mouse_mode = enabled ? mode : 0;
    } else if(mode == 1004) {
        terminal->focus_reporting = enabled ? 1 : 0;
    } else if(mode == 1005) {
        terminal->mouse_utf8 = enabled ? 1 : 0;
    } else if(mode == 1006) {
        terminal->mouse_sgr = enabled ? 1 : 0;
    } else if(mode == 2004) {
        terminal->bracketed_paste = enabled ? 1 : 0;
    } else if(mode == 1048) {
        if(enabled) {
            save_cursor_state(terminal);
        } else {
            restore_cursor_state(terminal);
        }
    } else if(mode == 47) {
        if(enabled) {
            terminal->alternate_screen = 1;
        } else {
            terminal->alternate_screen = 0;
        }
    } else if(mode == 1047) {
        if(enabled) {
            terminal->alternate_screen = 1;
            clear_screen(terminal);
        } else {
            clear_alternate_screen(terminal);
            terminal->alternate_screen = 0;
        }
    } else if(mode == 1049) {
        if(enabled) {
            save_cursor_state(terminal);
            terminal->alternate_screen = 1;
            clear_screen(terminal);
        } else {
            clear_alternate_screen(terminal);
            terminal->alternate_screen = 0;
            restore_cursor_state(terminal);
        }
    }
}

static void set_mode(TerminalState *terminal, int mode, int enabled)
{
    if(terminal == NULL)
        return;
    if(mode == 4)
        terminal->insert_mode = enabled ? 1 : 0;
}

static int mode_report_status(const TerminalState *terminal, int mode)
{
    if(terminal == NULL)
        return 0;
    if(mode == 4)
        return terminal->insert_mode ? 1 : 2;
    return 0;
}

static int private_mode_report_status(const TerminalState *terminal, int mode)
{
    if(terminal == NULL)
        return 0;
    if(mode == 25)
        return terminal->cursor_visible ? 1 : 2;
    if(mode == 6)
        return terminal->origin_mode ? 1 : 2;
    if(mode == 7)
        return terminal->autowrap ? 1 : 2;
    if(mode == 1)
        return terminal->application_cursor_keys ? 1 : 2;
    if(mode == 9 || mode == 1000 || mode == 1002 || mode == 1003)
        return terminal->mouse_mode == mode ? 1 : 2;
    if(mode == 1004)
        return terminal->focus_reporting ? 1 : 2;
    if(mode == 1005)
        return terminal->mouse_utf8 ? 1 : 2;
    if(mode == 1006)
        return terminal->mouse_sgr ? 1 : 2;
    if(mode == 2004)
        return terminal->bracketed_paste ? 1 : 2;
    if(mode == 47 || mode == 1047 || mode == 1049)
        return terminal->alternate_screen ? 1 : 2;
    return 0;
}

static void send_private_mode_report(TerminalState *terminal, int mode)
{
    char response[32];

    snprintf(response, sizeof(response), "\x1b[?%d;%d$y", mode,
             private_mode_report_status(terminal, mode));
    terminal_write_text(terminal, response);
}

static void send_mode_report(TerminalState *terminal, int mode)
{
    char response[32];

    snprintf(response, sizeof(response), "\x1b[%d;%d$y", mode,
             mode_report_status(terminal, mode));
    terminal_write_text(terminal, response);
}

static void apply_csi(TerminalState *terminal, int final)
{
    int n = csi_arg(terminal, 0, 1);
    int at;
    int i;

    if(final == 'A')
        terminal->cursor_row -= n;
    else if(final == 'B')
        terminal->cursor_row += n;
    else if(final == 'C')
        terminal->cursor_col += n;
    else if(final == 'D')
        terminal->cursor_col -= n;
    else if(final == 'E') {
        terminal->cursor_row += n;
        terminal->cursor_col = 0;
    } else if(final == 'F') {
        terminal->cursor_row -= n;
        terminal->cursor_col = 0;
    }
    else if(final == 'G')
        terminal->cursor_col = n - 1;
    else if(final == 'd')
        terminal->cursor_row = cursor_row_from_arg(terminal, 0, 1);
    else if(final == 'H' || final == 'f') {
        terminal->cursor_row = cursor_row_from_arg(terminal, 0, 1);
        terminal->cursor_col = csi_arg(terminal, 1, 1) - 1;
    } else if(final == '@') {
        insert_blank_chars(terminal, n);
    } else if(final == 'J') {
        int mode = csi_arg(terminal, 0, 0);

        at = terminal->cursor_row * terminal->cols + terminal->cursor_col;
        if(mode == 2)
            clear_screen(terminal);
        else if(mode == 3)
            clear_scrollback(terminal);
        else if(mode == 0)
            erase_range(terminal, at, terminal->cols * terminal->rows);
        else if(mode == 1)
            erase_range(terminal, 0, at + 1);
    } else if(final == 'K') {
        int mode = csi_arg(terminal, 0, 0);
        int start = terminal->cursor_row * terminal->cols + terminal->cursor_col;
        int end = (terminal->cursor_row + 1) * terminal->cols;

        if(mode == 1) {
            start = terminal->cursor_row * terminal->cols;
            end = terminal->cursor_row * terminal->cols + terminal->cursor_col + 1;
        } else if(mode == 2) {
            start = terminal->cursor_row * terminal->cols;
        }
        erase_range(terminal, start, end);
    } else if(final == 'L') {
        scroll_down(terminal, terminal->cursor_row, terminal->scroll_bottom, n);
    } else if(final == 'M') {
        scroll_up(terminal, terminal->cursor_row, terminal->scroll_bottom, n);
    } else if(final == 'P') {
        delete_chars(terminal, n);
    } else if(final == 'X') {
        erase_chars(terminal, n);
    } else if(final == 'I') {
        for(i = 0; i < n; i++)
            terminal->cursor_col = next_tab_stop(terminal);
    } else if(final == 'Z') {
        int moved = 0;
        int col;

        for(col = terminal->cursor_col - 1; col >= 0 && moved < n; col--) {
            if(terminal->tab_stops != NULL && terminal->tab_stops[col]) {
                terminal->cursor_col = col;
                moved++;
            }
        }
        if(moved < n)
            terminal->cursor_col = 0;
    } else if(final == 'm') {
        apply_style(terminal);
    } else if(final == 'r') {
        int top = csi_arg(terminal, 0, 1) - 1;
        int bottom = csi_arg(terminal, 1, terminal->rows) - 1;

        if(top < bottom) {
            terminal->scroll_top = clamp_int(top, 0, terminal->rows - 1);
            terminal->scroll_bottom = clamp_int(bottom, 0, terminal->rows - 1);
            terminal->cursor_col = 0;
            terminal->cursor_row =
                terminal->origin_mode ? terminal->scroll_top : 0;
        }
    } else if(final == 'S') {
        scroll_up(terminal, terminal->scroll_top, terminal->scroll_bottom, n);
    } else if(final == 'T') {
        scroll_down(terminal, terminal->scroll_top, terminal->scroll_bottom, n);
    } else if(final == 's') {
        save_cursor_state(terminal);
    } else if(final == 'u') {
        restore_cursor_state(terminal);
    } else if(final == 'g') {
        int mode = csi_arg(terminal, 0, 0);

        if(mode == 0 && terminal->tab_stops != NULL)
            terminal->tab_stops[terminal->cursor_col] = 0;
        else if(mode == 3)
            reset_tab_stops(terminal);
    } else if(final == 'q' && terminal->csi_intermediate == ' ') {
        int style = csi_arg(terminal, 0, 0);

        if(style <= 1)
            terminal->cursor_style = TERMINAL_CURSOR_DEFAULT;
        else if(style == 2)
            terminal->cursor_style = TERMINAL_CURSOR_BLOCK;
        else if(style == 3 || style == 4)
            terminal->cursor_style = TERMINAL_CURSOR_UNDERLINE;
        else if(style == 5 || style == 6)
            terminal->cursor_style = TERMINAL_CURSOR_BAR;
    } else if((final == 'h' || final == 'l') && terminal->csi_private == '?') {
        for(i = 0; i < terminal->csi_count; i++)
            set_private_mode(terminal, terminal->csi_args[i], final == 'h');
    } else if(final == 'h' || final == 'l') {
        for(i = 0; i < terminal->csi_count; i++)
            set_mode(terminal, terminal->csi_args[i], final == 'h');
    } else if(final == 'p' && terminal->csi_private == '?' &&
              terminal->csi_intermediate == '$') {
        send_private_mode_report(terminal, n);
    } else if(final == 'p' && terminal->csi_intermediate == '$') {
        send_mode_report(terminal, n);
    } else if(final == 'c') {
        if(terminal->csi_private == '>')
            terminal_write_text(terminal, "\x1b[>0;0;0c");
        else
            terminal_write_text(terminal, "\x1b[?1;2c");
    } else if(final == 'n') {
        if(n == 5) {
            terminal_write_text(terminal, "\x1b[0n");
        } else if(n == 6) {
            char response[32];

            snprintf(response, sizeof(response), "\x1b[%d;%dR",
                     terminal->cursor_row + 1, terminal->cursor_col + 1);
            terminal_write_text(terminal, response);
        }
    }
    clamp_cursor(terminal);
}

static void start_csi(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_CSI;
    terminal->csi_private = 0;
    terminal->csi_intermediate = 0;
    terminal->csi_count = 0;
    for(i = 0; i < MAX_CSI_ARGS; i++)
        terminal->csi_args[i] = 0;
}

static void start_osc(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_OSC;
    terminal->osc_len = 0;
    terminal->osc[0] = '\0';
}

static void start_dcs(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_DCS;
    terminal->dcs_len = 0;
    terminal->dcs_ignored = 0;
    if(terminal->dcs != NULL)
        terminal->dcs[0] = '\0';
}

static int hex_value(int ch)
{
    if(ch >= '0' && ch <= '9')
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    if(ch >= 'A' && ch <= 'F')
        return 10 + ch - 'A';
    return -1;
}

static int parse_hex_byte(const char *text, int *used)
{
    int hi;
    int lo;

    if(text == NULL || text[0] == '\0')
        return -1;
    hi = hex_value((unsigned char)text[0]);
    if(hi < 0)
        return -1;
    lo = hex_value((unsigned char)text[1]);
    if(lo < 0) {
        if(used != NULL)
            *used = 1;
        return (hi << 4) | hi;
    }
    if(used != NULL)
        *used = 2;
    return (hi << 4) | lo;
}

static int parse_osc_color(const char *text)
{
    int r;
    int g;
    int b;
    int used;

    if(text == NULL)
        return COLOR_DEFAULT;
    if(text[0] == '#') {
        r = parse_hex_byte(text + 1, NULL);
        g = parse_hex_byte(text + 3, NULL);
        b = parse_hex_byte(text + 5, NULL);
        if(r >= 0 && g >= 0 && b >= 0)
            return COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
        return COLOR_DEFAULT;
    }
    if(strncmp(text, "rgb:", 4) == 0 || strncmp(text, "rgba:", 5) == 0) {
        const char *cursor = strchr(text, ':');

        if(cursor == NULL)
            return COLOR_DEFAULT;
        cursor++;
        r = parse_hex_byte(cursor, &used);
        cursor = strchr(cursor, '/');
        if(cursor == NULL)
            return COLOR_DEFAULT;
        cursor++;
        g = parse_hex_byte(cursor, &used);
        cursor = strchr(cursor, '/');
        if(cursor == NULL)
            return COLOR_DEFAULT;
        cursor++;
        b = parse_hex_byte(cursor, &used);
        if(r >= 0 && g >= 0 && b >= 0)
            return COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
    }
    return COLOR_DEFAULT;
}

typedef struct SixelBuilder {
    int *pixels;
    int width;
    int height;
    int max_x;
    int max_y;
    int raster_width;
    int raster_height;
    int x;
    int y;
    int color_index;
    int palette[256];
    int background;
    int transparent;
    int failed;
} SixelBuilder;

static int sixel_default_color(int index)
{
    static const int ansi[16] = {
        0x000000, 0x800000, 0x008000, 0x808000,
        0x000080, 0x800080, 0x008080, 0xc0c0c0,
        0x808080, 0xff0000, 0x00ff00, 0xffff00,
        0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
    };

    if(index < 0)
        index = 0;
    if(index < 16)
        return COLOR_TRUE_RGB | ansi[index];
    if(index >= 16 && index < 232) {
        int n = index - 16;
        int r = n / 36;
        int g = (n / 6) % 6;
        int b = n % 6;

        r = r == 0 ? 0 : 55 + r * 40;
        g = g == 0 ? 0 : 55 + g * 40;
        b = b == 0 ? 0 : 55 + b * 40;
        return COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
    }
    if(index < 256) {
        int level = 8 + (index - 232) * 10;

        return COLOR_TRUE_RGB | (level << 16) | (level << 8) | level;
    }
    return COLOR_TRUE_RGB;
}

static void init_sixel_builder(SixelBuilder *builder, int transparent,
                               int background)
{
    int i;

    memset(builder, 0, sizeof(*builder));
    builder->max_x = -1;
    builder->max_y = -1;
    builder->color_index = 0;
    builder->background = transparent ? COLOR_DEFAULT : background;
    builder->transparent = transparent ? 1 : 0;
    for(i = 0; i < 256; i++)
        builder->palette[i] = sixel_default_color(i);
}

static int ensure_sixel_canvas(SixelBuilder *builder, int width, int height)
{
    int new_width;
    int new_height;
    int *pixels;
    int row;

    if(builder == NULL || width <= 0 || height <= 0 || builder->failed)
        return 0;
    if(width > MAX_SIXEL_DIMENSION || height > MAX_SIXEL_DIMENSION ||
       (size_t)width * (size_t)height > MAX_SIXEL_IMAGE_PIXELS) {
        builder->failed = 1;
        return 0;
    }
    if(width <= builder->width && height <= builder->height)
        return 1;
    new_width = builder->width > 0 ? builder->width : 1;
    new_height = builder->height > 0 ? builder->height : 1;
    while(new_width < width)
        new_width *= 2;
    while(new_height < height)
        new_height *= 2;
    if(new_width > MAX_SIXEL_DIMENSION)
        new_width = width;
    if(new_height > MAX_SIXEL_DIMENSION)
        new_height = height;
    if((size_t)new_width * (size_t)new_height > MAX_SIXEL_IMAGE_PIXELS) {
        new_width = width;
        new_height = height;
    }
    if((size_t)new_width * (size_t)new_height > MAX_SIXEL_IMAGE_PIXELS) {
        builder->failed = 1;
        return 0;
    }
    pixels = malloc((size_t)new_width * (size_t)new_height * sizeof(int));
    if(pixels == NULL) {
        builder->failed = 1;
        return 0;
    }
    for(row = 0; row < new_height; row++) {
        int col;

        for(col = 0; col < new_width; col++)
            pixels[row * new_width + col] = builder->background;
    }
    for(row = 0; row < builder->height; row++) {
        memcpy(pixels + row * new_width,
               builder->pixels + row * builder->width,
               (size_t)builder->width * sizeof(int));
    }
    free(builder->pixels);
    builder->pixels = pixels;
    builder->width = new_width;
    builder->height = new_height;
    return 1;
}

static int read_sixel_number(const char **cursor)
{
    int value = 0;
    int seen = 0;

    if(cursor == NULL || *cursor == NULL)
        return -1;
    while(**cursor >= '0' && **cursor <= '9') {
        value = value * 10 + (**cursor - '0');
        (*cursor)++;
        seen = 1;
    }
    return seen ? value : -1;
}

static int read_sixel_param(const char **cursor, int fallback)
{
    int value;

    value = read_sixel_number(cursor);
    if(**cursor == ';')
        (*cursor)++;
    return value >= 0 ? value : fallback;
}

static int percent_to_byte(int value)
{
    value = clamp_int(value, 0, 100);
    return value * 255 / 100;
}

static int hls_to_rgb(int hue, int lightness, int saturation)
{
    int c;
    int x;
    int m;
    int segment;
    int remainder;
    int r = 0;
    int g = 0;
    int b = 0;

    hue %= 360;
    if(hue < 0)
        hue += 360;
    lightness = clamp_int(lightness, 0, 100);
    saturation = clamp_int(saturation, 0, 100);
    c = (100 - abs(2 * lightness - 100)) * saturation / 100;
    segment = hue / 60;
    remainder = hue % 120;
    x = c * (60 - abs(remainder - 60)) / 60;
    m = lightness - c / 2;
    if(segment == 0) {
        r = c;
        g = x;
    } else if(segment == 1) {
        r = x;
        g = c;
    } else if(segment == 2) {
        g = c;
        b = x;
    } else if(segment == 3) {
        g = x;
        b = c;
    } else if(segment == 4) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    r = percent_to_byte(r + m);
    g = percent_to_byte(g + m);
    b = percent_to_byte(b + m);
    return COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
}

static void put_sixel_column(SixelBuilder *builder, int bits)
{
    int bit;

    if(builder == NULL || builder->failed)
        return;
    if(!ensure_sixel_canvas(builder, builder->x + 1, builder->y + 6))
        return;
    for(bit = 0; bit < 6; bit++) {
        if((bits & (1 << bit)) != 0) {
            int y = builder->y + bit;

            builder->pixels[y * builder->width + builder->x] =
                builder->palette[builder->color_index];
            if(builder->x > builder->max_x)
                builder->max_x = builder->x;
            if(y > builder->max_y)
                builder->max_y = y;
        }
    }
    if(builder->x > builder->max_x)
        builder->max_x = builder->x;
    if(builder->y + 5 > builder->max_y)
        builder->max_y = builder->y + 5;
    builder->x++;
}

static int add_sixel_image(TerminalState *terminal, const int *pixels,
                           int stride, int width, int height)
{
    SixelImage *images;
    int *copy;
    int row;

    if(terminal == NULL || pixels == NULL || width <= 0 || height <= 0 ||
       stride < width)
        return 0;
    if((size_t)width * (size_t)height > MAX_SIXEL_IMAGE_PIXELS)
        return 0;
    while(terminal->sixel_count >= MAX_SIXEL_IMAGES ||
          terminal->sixel_total_pixels + width * height >
              MAX_SIXEL_TOTAL_PIXELS)
        remove_sixel_image(terminal, 0);
    if(terminal->sixel_count >= terminal->sixel_capacity) {
        int capacity = terminal->sixel_capacity > 0
                           ? terminal->sixel_capacity * 2
                           : 8;

        if(capacity > MAX_SIXEL_IMAGES)
            capacity = MAX_SIXEL_IMAGES;
        images = realloc(terminal->sixel_images,
                         (size_t)capacity * sizeof(SixelImage));
        if(images == NULL)
            return 0;
        terminal->sixel_images = images;
        terminal->sixel_capacity = capacity;
    }
    copy = malloc((size_t)width * (size_t)height * sizeof(int));
    if(copy == NULL)
        return 0;
    for(row = 0; row < height; row++) {
        memcpy(copy + row * width, pixels + row * stride,
               (size_t)width * sizeof(int));
    }
    terminal->sixel_images[terminal->sixel_count].col = terminal->cursor_col;
    terminal->sixel_images[terminal->sixel_count].row = terminal->cursor_row;
    terminal->sixel_images[terminal->sixel_count].alternate_screen =
        terminal->alternate_screen;
    terminal->sixel_images[terminal->sixel_count].width = width;
    terminal->sixel_images[terminal->sixel_count].height = height;
    terminal->sixel_images[terminal->sixel_count].pixels = copy;
    terminal->sixel_count++;
    terminal->sixel_total_pixels += width * height;
    return 1;
}

static void finish_sixel(TerminalState *terminal, const char *payload)
{
    SixelBuilder builder;
    const char *cursor;
    const char *data;
    int transparent = 1;
    int background = COLOR_TRUE_RGB;
    int width;
    int height;
    int rows_used;

    if(terminal == NULL || payload == NULL)
        return;
    data = strchr(payload, 'q');
    if(data == NULL)
        return;
    cursor = payload;
    if(cursor < data) {
        int param_index = 0;

        while(cursor < data) {
            int value = read_sixel_number(&cursor);

            if(param_index == 1 && value == 2)
                transparent = 0;
            if(*cursor == ';') {
                cursor++;
                param_index++;
            } else if(cursor < data) {
                cursor++;
            }
        }
    }
    init_sixel_builder(&builder, transparent, background);
    cursor = data + 1;
    while(*cursor != '\0' && !builder.failed) {
        int ch = (unsigned char)*cursor++;

        if(ch >= '?' && ch <= '~') {
            put_sixel_column(&builder, ch - '?');
        } else if(ch == '!') {
            int repeat = read_sixel_number(&cursor);
            int repeated;

            if(repeat <= 0)
                repeat = 1;
            if(*cursor == '\0')
                break;
            repeated = (unsigned char)*cursor++;
            if(repeated >= '?' && repeated <= '~') {
                int i;

                repeat = clamp_int(repeat, 1, MAX_SIXEL_DIMENSION);
                for(i = 0; i < repeat && !builder.failed; i++)
                    put_sixel_column(&builder, repeated - '?');
            }
        } else if(ch == '$') {
            builder.x = 0;
        } else if(ch == '-') {
            builder.x = 0;
            builder.y += 6;
            if(builder.y > MAX_SIXEL_DIMENSION)
                builder.failed = 1;
        } else if(ch == '#') {
            int index = read_sixel_number(&cursor);

            if(index >= 0 && index < 256) {
                builder.color_index = index;
                if(*cursor == ';') {
                    int mode;
                    int a;
                    int b;
                    int c;

                    cursor++;
                    mode = read_sixel_param(&cursor, 0);
                    a = read_sixel_param(&cursor, 0);
                    b = read_sixel_param(&cursor, 0);
                    c = read_sixel_param(&cursor, 0);
                    if(mode == 2) {
                        builder.palette[index] =
                            COLOR_TRUE_RGB |
                            (percent_to_byte(a) << 16) |
                            (percent_to_byte(b) << 8) |
                            percent_to_byte(c);
                    } else if(mode == 1) {
                        builder.palette[index] = hls_to_rgb(a, b, c);
                    }
                }
            }
        } else if(ch == '"') {
            int pan = read_sixel_param(&cursor, 0);
            int pad = read_sixel_param(&cursor, 0);
            int raster_width = read_sixel_param(&cursor, 0);
            int raster_height = read_sixel_param(&cursor, 0);

            (void)pan;
            (void)pad;
            if(raster_width > 0 && raster_height > 0) {
                builder.raster_width = raster_width;
                builder.raster_height = raster_height;
                ensure_sixel_canvas(&builder, raster_width, raster_height);
            }
        }
    }
    width = builder.max_x + 1;
    height = builder.max_y + 1;
    if(builder.raster_width > width)
        width = builder.raster_width;
    if(builder.raster_height > height)
        height = builder.raster_height;
    if(!builder.failed && width > 0 && height > 0 &&
       add_sixel_image(terminal, builder.pixels, builder.width, width, height)) {
        rows_used = (height + 5) / 6;
        if(rows_used < 1)
            rows_used = 1;
        terminal->cursor_col = 0;
        while(rows_used-- > 0)
            line_feed(terminal);
        clamp_cursor(terminal);
    }
    free(builder.pixels);
}

static void finish_dcs(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    if(!terminal->dcs_ignored && terminal->dcs != NULL) {
        terminal->dcs[terminal->dcs_len] = '\0';
        if(strchr(terminal->dcs, 'q') != NULL)
            finish_sixel(terminal, terminal->dcs);
    }
    terminal->dcs_len = 0;
    terminal->dcs_ignored = 0;
}

static void append_dcs(TerminalState *terminal, unsigned int codepoint)
{
    char bytes[8];
    int used = 0;
    int i;

    if(terminal == NULL || terminal->dcs_ignored)
        return;
    if(!append_utf8(bytes, (int)sizeof(bytes), &used, codepoint)) {
        terminal->dcs_ignored = 1;
        return;
    }
    if(terminal->dcs_capacity <= terminal->dcs_len + used + 1) {
        int capacity = terminal->dcs_capacity > 0 ? terminal->dcs_capacity * 2
                                                  : 1024;
        char *dcs;

        while(capacity <= terminal->dcs_len + used + 1)
            capacity *= 2;
        if(capacity > MAX_DCS_BYTES) {
            terminal->dcs_ignored = 1;
            return;
        }
        dcs = realloc(terminal->dcs, (size_t)capacity);
        if(dcs == NULL) {
            terminal->dcs_ignored = 1;
            return;
        }
        terminal->dcs = dcs;
        terminal->dcs_capacity = capacity;
    }
    for(i = 0; i < used; i++)
        terminal->dcs[terminal->dcs_len++] = bytes[i];
    terminal->dcs[terminal->dcs_len] = '\0';
}

static int base64_value(int ch)
{
    if(ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if(ch >= 'a' && ch <= 'z')
        return 26 + ch - 'a';
    if(ch >= '0' && ch <= '9')
        return 52 + ch - '0';
    if(ch == '+')
        return 62;
    if(ch == '/')
        return 63;
    return -1;
}

static int decode_base64(char *out, int out_size, const char *text)
{
    int value = 0;
    int bits = 0;
    int used = 0;

    if(out == NULL || out_size <= 0 || text == NULL)
        return 0;
    out[0] = '\0';
    while(*text != '\0') {
        int ch = (unsigned char)*text++;
        int v;

        if(ch == '=')
            break;
        if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            continue;
        v = base64_value(ch);
        if(v < 0)
            return 0;
        value = (value << 6) | v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            if(used >= out_size - 1)
                break;
            out[used++] = (char)((value >> bits) & 0xff);
        }
    }
    out[used] = '\0';
    return used;
}

static void decode_file_uri_path(char *out, int out_size, const char *uri)
{
    const char *path;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(uri == NULL || strncmp(uri, "file://", 7) != 0)
        return;
    path = strchr(uri + 7, '/');
    if(path == NULL)
        return;
    while(*path != '\0' && used < out_size - 1) {
        if(*path == '%' && hex_value((unsigned char)path[1]) >= 0 &&
           hex_value((unsigned char)path[2]) >= 0) {
            out[used++] = (char)((hex_value((unsigned char)path[1]) << 4) |
                                 hex_value((unsigned char)path[2]));
            path += 3;
        } else {
            out[used++] = *path++;
        }
    }
    out[used] = '\0';
}

static int find_or_add_hyperlink(TerminalState *terminal, const char *url)
{
    int i;

    if(terminal == NULL || url == NULL || url[0] == '\0')
        return 0;
    for(i = 0; i < terminal->hyperlink_count; i++) {
        if(strcmp(terminal->hyperlinks[i], url) == 0)
            return i + 1;
    }
    if(terminal->hyperlink_count >= MAX_HYPERLINKS)
        return terminal->hyperlink_count;
    snprintf(terminal->hyperlinks[terminal->hyperlink_count],
             sizeof(terminal->hyperlinks[terminal->hyperlink_count]), "%s",
             url);
    terminal->hyperlink_count++;
    return terminal->hyperlink_count;
}

static int send_osc_color_response(TerminalState *terminal, int code, int color)
{
    char response[64];
    int rgb;
    int r;
    int g;
    int b;

    if(terminal == NULL || (color & COLOR_TRUE_RGB) == 0)
        return 0;
    rgb = color & 0xffffff;
    r = (rgb >> 16) & 255;
    g = (rgb >> 8) & 255;
    b = rgb & 255;
    snprintf(response, sizeof(response),
             "\x1b]%d;rgb:%02x%02x/%02x%02x/%02x%02x\a",
             code, r, r, g, g, b, b);
    return terminal_write_text(terminal, response);
}

static int send_osc_palette_response(TerminalState *terminal, int index,
                                     int color)
{
    char response[80];
    int rgb;
    int r;
    int g;
    int b;

    if(terminal == NULL || index < 0 || index >= 256 ||
       (color & COLOR_TRUE_RGB) == 0)
        return 0;
    rgb = color & 0xffffff;
    r = (rgb >> 16) & 255;
    g = (rgb >> 8) & 255;
    b = rgb & 255;
    snprintf(response, sizeof(response),
             "\x1b]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\a",
             index, r, r, g, g, b, b);
    return terminal_write_text(terminal, response);
}

static void finish_osc(TerminalState *terminal)
{
    char buffer[sizeof(terminal->osc)];
    char *separator;
    char *payload;
    int code;
    int color;

    if(terminal == NULL)
        return;
    terminal->osc[sizeof(terminal->osc) - 1] = '\0';
    snprintf(buffer, sizeof(buffer), "%s", terminal->osc);
    separator = strchr(buffer, ';');
    if(separator != NULL) {
        *separator = '\0';
        payload = separator + 1;
    } else {
        payload = "";
    }
    code = atoi(buffer);
    if(code == 0 || code == 1 || code == 2) {
        snprintf(terminal->title, sizeof(terminal->title), "%s",
                 payload);
        return;
    }
    if(code == 7) {
        decode_file_uri_path(terminal->current_directory,
                             (int)sizeof(terminal->current_directory),
                             payload);
        return;
    }
    if(code == 10 || code == 11 || code == 12) {
        if(payload[0] == '?') {
            if(code == 10) {
                color = terminal->default_fg != COLOR_DEFAULT
                            ? terminal->default_fg
                            : terminal->base_fg;
                send_osc_color_response(terminal, code, color);
            } else if(code == 11) {
                color = terminal->default_bg != COLOR_DEFAULT
                            ? terminal->default_bg
                            : terminal->base_bg;
                send_osc_color_response(terminal, code, color);
            } else {
                color = terminal->cursor_color != COLOR_DEFAULT
                            ? terminal->cursor_color
                            : terminal->base_cursor_color;
                send_osc_color_response(terminal, code, color);
            }
            return;
        }
        color = parse_osc_color(payload);
        if(color == COLOR_DEFAULT)
            return;
        if(code == 10)
            terminal->default_fg = color;
        else if(code == 11)
            terminal->default_bg = color;
        else
            terminal->cursor_color = color;
        return;
    }
    if(code == 110 || code == 111 || code == 112) {
        if(code == 110)
            terminal->default_fg = COLOR_DEFAULT;
        else if(code == 111)
            terminal->default_bg = COLOR_DEFAULT;
        else
            terminal->cursor_color = COLOR_DEFAULT;
        return;
    }
    if(code == 4) {
        char *cursor = payload;

        while(cursor != NULL && cursor[0] != '\0') {
            char *next;
            char *color_text;
            int index = atoi(cursor);

            next = strchr(cursor, ';');
            if(next == NULL)
                return;
            color_text = next + 1;
            next = strchr(color_text, ';');
            if(next != NULL)
                *next = '\0';
            if(index >= 0 && index < 256 && color_text[0] == '?') {
                int value = terminal->palette_overrides[index];

                if(value == COLOR_DEFAULT)
                    value = sixel_default_color(index);
                send_osc_palette_response(terminal, index, value);
            } else if(index >= 0 && index < 256) {
                color = parse_osc_color(color_text);
                if(color != COLOR_DEFAULT)
                    terminal->palette_overrides[index] = color;
            }
            cursor = next != NULL ? next + 1 : NULL;
        }
        return;
    }
    if(code == 104) {
        char *cursor = payload;

        if(cursor[0] == '\0') {
            int i;

            for(i = 0; i < 256; i++)
                terminal->palette_overrides[i] = COLOR_DEFAULT;
            return;
        }
        while(cursor != NULL && cursor[0] != '\0') {
            char *next = strchr(cursor, ';');
            int index;

            if(next != NULL)
                *next = '\0';
            index = atoi(cursor);
            if(index >= 0 && index < 256)
                terminal->palette_overrides[index] = COLOR_DEFAULT;
            cursor = next != NULL ? next + 1 : NULL;
        }
        return;
    }
    if(code == 8) {
        char *url = strchr(payload, ';');

        if(url == NULL)
            return;
        url++;
        terminal->current_hyperlink = find_or_add_hyperlink(terminal, url);
        return;
    }
    if(code == 52) {
        char *clipboard_payload = strchr(payload, ';');

        if(clipboard_payload == NULL || clipboard_payload[1] == '?' ||
           clipboard_payload[1] == '\0')
            return;
        if(decode_base64(terminal->clipboard,
                         (int)sizeof(terminal->clipboard),
                         clipboard_payload + 1) > 0)
            terminal->clipboard_pending = 1;
    }
}

static void feed_codepoint(TerminalState *terminal, unsigned int codepoint)
{
    if(terminal->parser_state == STATE_ESCAPE) {
        if(codepoint == '[') {
            start_csi(terminal);
            return;
        }
        if(codepoint == ']') {
            start_osc(terminal);
            return;
        }
        if(codepoint == 'P') {
            start_dcs(terminal);
            return;
        }
        if(codepoint == '(' || codepoint == ')' || codepoint == '*' ||
           codepoint == '+') {
            terminal->pending_charset = codepoint == ')' ? 1 : 0;
            terminal->parser_state = STATE_CHARSET;
            return;
        }
        if(codepoint == '7') {
            save_cursor_state(terminal);
        } else if(codepoint == '8') {
            restore_cursor_state(terminal);
        } else if(codepoint == 'D') {
            line_feed(terminal);
        } else if(codepoint == 'H') {
            if(terminal->tab_stops != NULL)
                terminal->tab_stops[terminal->cursor_col] = 1;
        } else if(codepoint == 'M') {
            if(terminal->cursor_row == terminal->scroll_top)
                scroll_down(terminal, terminal->scroll_top,
                            terminal->scroll_bottom, 1);
            else
                terminal->cursor_row--;
        } else if(codepoint == 'c') {
            clear_screen(terminal);
            reset_style(terminal);
            terminal->scroll_top = 0;
            terminal->scroll_bottom = terminal->rows - 1;
            terminal->cursor_visible = 1;
            terminal->autowrap = 1;
            terminal->application_cursor_keys = 0;
            terminal->application_keypad = 0;
            terminal->bracketed_paste = 0;
            terminal->insert_mode = 0;
            terminal->mouse_mode = 0;
            terminal->mouse_utf8 = 0;
            terminal->mouse_sgr = 0;
            terminal->focus_reporting = 0;
            terminal->origin_mode = 0;
            terminal->current_hyperlink = 0;
            reset_charsets(terminal);
            clear_sixel_images(terminal, -1);
            reset_tab_stops(terminal);
            save_cursor_state(terminal);
        } else if(codepoint == '=') {
            terminal->application_keypad = 1;
        } else if(codepoint == '>') {
            terminal->application_keypad = 0;
        }
        terminal->parser_state = STATE_TEXT;
        clamp_cursor(terminal);
        return;
    }
    if(terminal->parser_state == STATE_CHARSET) {
        if(terminal->pending_charset == 1)
            terminal->g1_charset =
                codepoint == '0' ? CHARSET_DEC_SPECIAL : CHARSET_US_ASCII;
        else
            terminal->g0_charset =
                codepoint == '0' ? CHARSET_DEC_SPECIAL : CHARSET_US_ASCII;
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->parser_state == STATE_CSI) {
        if(codepoint == '?' || codepoint == '>') {
            terminal->csi_private = (int)codepoint;
            return;
        }
        if(codepoint >= 0x20 && codepoint <= 0x2f) {
            terminal->csi_intermediate = (int)codepoint;
            return;
        }
        if(codepoint >= '0' && codepoint <= '9') {
            if(terminal->csi_count == 0)
                terminal->csi_count = 1;
            terminal->csi_args[terminal->csi_count - 1] =
                terminal->csi_args[terminal->csi_count - 1] * 10 +
                (int)(codepoint - '0');
            return;
        }
        if(codepoint == ';' || codepoint == ':') {
            if(terminal->csi_count < MAX_CSI_ARGS) {
                if(terminal->csi_count == 0)
                    terminal->csi_count = 1;
                terminal->csi_args[terminal->csi_count] = 0;
                terminal->csi_count++;
            }
            return;
        }
        terminal->parser_state = STATE_TEXT;
        if(codepoint >= '@' && codepoint <= '~')
            apply_csi(terminal, (int)codepoint);
        return;
    }
    if(terminal->parser_state == STATE_OSC) {
        if(codepoint == 7) {
            finish_osc(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b) {
            terminal->parser_state = STATE_OSC_ESCAPE;
            return;
        }
        if(terminal->osc_len < (int)sizeof(terminal->osc) - 1) {
            terminal->osc[terminal->osc_len++] = (char)codepoint;
            terminal->osc[terminal->osc_len] = '\0';
        }
        return;
    }
    if(terminal->parser_state == STATE_OSC_ESCAPE) {
        if(codepoint == '\\')
            finish_osc(terminal);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->parser_state == STATE_DCS) {
        if(codepoint == 0x9c || codepoint == 7) {
            finish_dcs(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b) {
            terminal->parser_state = STATE_DCS_ESCAPE;
            return;
        }
        append_dcs(terminal, codepoint);
        return;
    }
    if(terminal->parser_state == STATE_DCS_ESCAPE) {
        if(codepoint == '\\') {
            finish_dcs(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        append_dcs(terminal, 0x1b);
        append_dcs(terminal, codepoint);
        terminal->parser_state = STATE_DCS;
        return;
    }

    if(codepoint == 0x1b) {
        terminal->parser_state = STATE_ESCAPE;
        return;
    }
    if(codepoint == '\r') {
        terminal->cursor_col = 0;
        return;
    }
    if(codepoint == '\n') {
        line_feed(terminal);
        return;
    }
    if(codepoint == '\b') {
        if(terminal->cursor_col > 0)
            terminal->cursor_col--;
        return;
    }
    if(codepoint == '\t') {
        terminal->cursor_col = next_tab_stop(terminal);
        clamp_cursor(terminal);
        return;
    }
    if(codepoint == 0x0e) {
        terminal->active_charset = 1;
        return;
    }
    if(codepoint == 0x0f) {
        terminal->active_charset = 0;
        return;
    }
    if(codepoint == 7)
        return;
    if(codepoint >= 32)
        put_codepoint(terminal, codepoint);
}

static void feed_byte(TerminalState *terminal, unsigned char byte)
{
    if(byte == 0x9b) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_csi(terminal);
        return;
    }
    if(byte == 0x9d) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_osc(terminal);
        return;
    }
    if(byte == 0x90) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_dcs(terminal);
        return;
    }
    if(byte == 0x9c) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        if(terminal->parser_state == STATE_DCS ||
           terminal->parser_state == STATE_DCS_ESCAPE)
            finish_dcs(terminal);
        else if(terminal->parser_state == STATE_OSC ||
                terminal->parser_state == STATE_OSC_ESCAPE)
            finish_osc(terminal);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->utf8_remaining > 0) {
        if((byte & 0xc0) == 0x80) {
            terminal->utf8_codepoint =
                (terminal->utf8_codepoint << 6) | (byte & 0x3f);
            terminal->utf8_remaining--;
            if(terminal->utf8_remaining == 0) {
                feed_codepoint(terminal,
                               (unsigned int)terminal->utf8_codepoint);
                terminal->utf8_codepoint = 0;
            }
            return;
        }
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        feed_codepoint(terminal, '?');
    }
    if(byte < 0x80) {
        feed_codepoint(terminal, byte);
    } else if((byte & 0xe0) == 0xc0) {
        terminal->utf8_codepoint = byte & 0x1f;
        terminal->utf8_remaining = 1;
    } else if((byte & 0xf0) == 0xe0) {
        terminal->utf8_codepoint = byte & 0x0f;
        terminal->utf8_remaining = 2;
    } else if((byte & 0xf8) == 0xf0) {
        terminal->utf8_codepoint = byte & 0x07;
        terminal->utf8_remaining = 3;
    } else {
        feed_codepoint(terminal, '?');
    }
}

int terminal_spawn(TerminalState *terminal, const char *cwd, const char *shell,
                   const char *command, int cols, int rows)
{
    int master;
    int pid;

    if(terminal == NULL)
        return 0;
    terminal_close(terminal);
    terminal_init(terminal);
    if(!allocate_screen(terminal, cols, rows))
        return 0;
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if(master < 0)
        return 0;
    if(grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return 0;
    }
    set_window_size(master, terminal->cols, terminal->rows);
    pid = fork();
    if(pid < 0) {
        close(master);
        return 0;
    }
    if(pid == 0) {
        const char *slave_name = ptsname(master);
        const char *run_shell;
        char slave_path[256];
        int slave;

        if(slave_name == NULL)
            _exit(127);
        snprintf(slave_path, sizeof(slave_path), "%s", slave_name);
        setsid();
        close(master);
        slave = open(slave_path, O_RDWR);
        if(slave < 0)
            _exit(127);
#ifdef TIOCSCTTY
        ioctl(slave, TIOCSCTTY, 0);
#endif
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if(slave > 2)
            close(slave);
        if(cwd != NULL && cwd[0] != '\0' && chdir(cwd) != 0)
            _exit(127);
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        run_shell = shell;
        if(run_shell == NULL || run_shell[0] == '\0')
            run_shell = getenv("SHELL");
        if(run_shell == NULL || run_shell[0] == '\0')
            run_shell = "/bin/sh";
        if(command != NULL && command[0] != '\0')
            execl(run_shell, run_shell, "-lc", command, (char *)NULL);
        else
            execl(run_shell, run_shell, "-i", (char *)NULL);
        _exit(127);
    }
    {
        int flags = fcntl(master, F_GETFL, 0);

        if(flags >= 0)
            fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    terminal->pid = pid;
    terminal->fd = master;
    terminal->running = 1;
    return 1;
}

int terminal_open(TerminalState *terminal, const char *cwd, int cols, int rows)
{
    return terminal_spawn(terminal, cwd, NULL, NULL, cols, rows);
}

int terminal_write(TerminalState *terminal, const void *data, int size)
{
    int written;

    if(terminal == NULL || !terminal->running || terminal->fd < 0 ||
       data == NULL || size <= 0)
        return 0;
    written = (int)write(terminal->fd, data, (size_t)size);
    return written > 0 ? written : 0;
}

int terminal_write_text(TerminalState *terminal, const char *text)
{
    if(text == NULL)
        return 0;
    return terminal_write(terminal, text, (int)strlen(text));
}

int terminal_send_codepoint(TerminalState *terminal, unsigned int codepoint,
                            int mods)
{
    char text[8];
    int len = 0;

    if(terminal == NULL || codepoint == 0)
        return 0;
    if((mods & MOD_ALT) != 0)
        text[len++] = '\x1b';
    if(!append_utf8(text, (int)sizeof(text), &len, codepoint))
        return 0;
    return terminal_write(terminal, text, len);
}

int terminal_send_key(TerminalState *terminal, int key, int mods)
{
    char seq[32];
    const char *plain = NULL;
    char final = 0;
    int modifier = 1;

    if(mods & MOD_SHIFT)
        modifier += 1;
    if(mods & MOD_ALT)
        modifier += 2;
    if(mods & MOD_CTRL)
        modifier += 4;

    if(key == KEY_ENTER_CODE)
        plain = "\r";
    else if(key == KEY_BACKSPACE_CODE)
        plain = "\x7f";
    else if(key == KEY_TAB_CODE) {
        if((mods & MOD_SHIFT) != 0) {
            if((mods & MOD_ALT) != 0)
                snprintf(seq, sizeof(seq), "\x1b\x1b[Z");
            else
                snprintf(seq, sizeof(seq), "\x1b[Z");
            return terminal_write_text(terminal, seq);
        }
        plain = "\t";
    }
    else if(key == KEY_ESCAPE_CODE)
        plain = "\x1b";
    else if(key == KEY_UP_CODE)
        final = 'A';
    else if(key == KEY_DOWN_CODE)
        final = 'B';
    else if(key == KEY_RIGHT_CODE)
        final = 'C';
    else if(key == KEY_LEFT_CODE)
        final = 'D';
    else if(key == KEY_HOME_CODE)
        final = 'H';
    else if(key == KEY_END_CODE)
        final = 'F';
    else if(key == KEY_INSERT_CODE || key == KEY_DELETE_CODE ||
            key == KEY_PAGE_UP_CODE || key == KEY_PAGE_DOWN_CODE) {
        int code = 2;

        if(key == KEY_DELETE_CODE)
            code = 3;
        else if(key == KEY_PAGE_UP_CODE)
            code = 5;
        else if(key == KEY_PAGE_DOWN_CODE)
            code = 6;
        if(modifier == 1)
            snprintf(seq, sizeof(seq), "\x1b[%d~", code);
        else
            snprintf(seq, sizeof(seq), "\x1b[%d;%d~", code, modifier);
        return terminal_write_text(terminal, seq);
    } else if(key >= KEY_F1_CODE && key <= KEY_F12_CODE) {
        static const int function_codes[] = {
            11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24
        };
        int index = key - KEY_F1_CODE;
        int code = function_codes[index];

        if(index < 4 && modifier == 1)
            snprintf(seq, sizeof(seq), "\x1bO%c", 'P' + index);
        else if(index < 4)
            snprintf(seq, sizeof(seq), "\x1b[1;%d%c", modifier, 'P' + index);
        else if(modifier == 1)
            snprintf(seq, sizeof(seq), "\x1b[%d~", code);
        else
            snprintf(seq, sizeof(seq), "\x1b[%d;%d~", code, modifier);
        return terminal_write_text(terminal, seq);
    }

    if(plain != NULL) {
        if((mods & MOD_ALT) != 0) {
            snprintf(seq, sizeof(seq), "\x1b%s", plain);
            return terminal_write_text(terminal, seq);
        }
        return terminal_write_text(terminal, plain);
    }
    if(final != 0) {
        if(modifier == 1 && terminal != NULL &&
           terminal->application_cursor_keys &&
           (final == 'A' || final == 'B' || final == 'C' || final == 'D' ||
            final == 'H' || final == 'F'))
            snprintf(seq, sizeof(seq), "\x1bO%c", final);
        else if(modifier == 1)
            snprintf(seq, sizeof(seq), "\x1b[%c", final);
        else
            snprintf(seq, sizeof(seq), "\x1b[1;%d%c", modifier, final);
        return terminal_write_text(terminal, seq);
    }
    return 0;
}

int terminal_send_keypad(TerminalState *terminal, char key)
{
    char seq[4];
    char app = 0;

    if(terminal == NULL)
        return 0;
    if(!terminal->application_keypad) {
        if(key == '\r')
            return terminal_write_text(terminal, "\r");
        seq[0] = key;
        return terminal_write(terminal, seq, 1);
    }
    if(key >= '0' && key <= '9')
        app = (char)('p' + key - '0');
    else if(key == '.')
        app = 'n';
    else if(key == '/')
        app = 'o';
    else if(key == '*')
        app = 'j';
    else if(key == '-')
        app = 'm';
    else if(key == '+')
        app = 'k';
    else if(key == '=')
        app = 'X';
    else if(key == '\r')
        app = 'M';
    if(app == 0)
        return 0;
    seq[0] = '\x1b';
    seq[1] = 'O';
    seq[2] = app;
    return terminal_write(terminal, seq, 3);
}

int terminal_send_mouse(TerminalState *terminal, int button, int col, int row,
                        int pressed, int motion, int mods)
{
    char seq[64];
    int cb;

    if(terminal == NULL || terminal->mouse_mode == 0 || col < 0 || row < 0)
        return 0;
    if(motion && (terminal->mouse_mode == 9 || terminal->mouse_mode == 1000))
        return 0;

    cb = button;
    if(cb < TERMINAL_MOUSE_LEFT)
        cb = TERMINAL_MOUSE_LEFT;
    if(cb > TERMINAL_MOUSE_WHEEL_DOWN)
        cb = TERMINAL_MOUSE_RELEASE;
    if(!pressed && !motion && !terminal->mouse_sgr)
        cb = TERMINAL_MOUSE_RELEASE;
    if(motion)
        cb += 32;
    if(mods & MOD_SHIFT)
        cb += 4;
    if(mods & MOD_ALT)
        cb += 8;
    if(mods & MOD_CTRL)
        cb += 16;

    if(terminal->mouse_sgr) {
        snprintf(seq, sizeof(seq), "\x1b[<%d;%d;%d%c", cb, col + 1, row + 1,
                 pressed || motion ? 'M' : 'm');
        return terminal_write_text(terminal, seq);
    }

    if(terminal->mouse_utf8) {
        int used = 3;

        seq[0] = '\x1b';
        seq[1] = '[';
        seq[2] = 'M';
        if(!append_utf8(seq, (int)sizeof(seq), &used, (unsigned int)(32 + cb)) ||
           !append_utf8(seq, (int)sizeof(seq), &used, (unsigned int)(33 + col)) ||
           !append_utf8(seq, (int)sizeof(seq), &used, (unsigned int)(33 + row)))
            return 0;
        return terminal_write(terminal, seq, used);
    }

    if(col > 222 || row > 222)
        return 0;
    seq[0] = '\x1b';
    seq[1] = '[';
    seq[2] = 'M';
    seq[3] = (char)(32 + cb);
    seq[4] = (char)(33 + col);
    seq[5] = (char)(33 + row);
    return terminal_write(terminal, seq, 6);
}

int terminal_send_focus(TerminalState *terminal, int focused)
{
    if(terminal == NULL || !terminal->focus_reporting)
        return 0;
    return terminal_write_text(terminal, focused ? "\x1b[I" : "\x1b[O");
}

int terminal_send_paste(TerminalState *terminal, const char *text)
{
    int written = 0;

    if(terminal == NULL || text == NULL || text[0] == '\0')
        return 0;
    if(terminal->bracketed_paste)
        written += terminal_write_text(terminal, "\x1b[200~");
    written += terminal_write_text(terminal, text);
    if(terminal->bracketed_paste)
        written += terminal_write_text(terminal, "\x1b[201~");
    return written;
}

void terminal_feed(TerminalState *terminal, const void *data, int size)
{
    const unsigned char *bytes = data;
    int i;

    if(terminal == NULL || bytes == NULL || size <= 0)
        return;
    for(i = 0; i < size; i++)
        feed_byte(terminal, bytes[i]);
}

int terminal_poll(TerminalState *terminal)
{
    char buffer[4096];
    int status;

    if(terminal == NULL || terminal->fd < 0)
        return 0;
    for(;;) {
        int got = (int)read(terminal->fd, buffer, sizeof(buffer));

        if(got < 0) {
            if(errno == EINTR)
                continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        if(got == 0)
            break;
        terminal_feed(terminal, buffer, got);
    }
    if(terminal->running && terminal->pid > 0) {
        if(waitpid(terminal->pid, &status, WNOHANG) > 0)
            terminal->running = 0;
    }
    return terminal->running;
}

void terminal_resize(TerminalState *terminal, int cols, int rows)
{
    if(terminal == NULL)
        return;
    cols = clamp_int(cols, 8, MAX_COLS);
    rows = clamp_int(rows, 4, MAX_ROWS);
    if(cols == terminal->cols && rows == terminal->rows)
        return;
    if(!allocate_screen(terminal, cols, rows))
        return;
    if(terminal->fd >= 0)
        set_window_size(terminal->fd, terminal->cols, terminal->rows);
}

void terminal_set_scrollback_limit(TerminalState *terminal, int rows)
{
    if(terminal == NULL)
        return;
    if(rows < 0)
        rows = 0;
    if(rows > 100000)
        rows = 100000;
    if(rows == 0)
        rows = SCROLLBACK_LIMIT;
    terminal->scrollback_limit = rows;
    if(terminal->cols > 0)
        allocate_scrollback(terminal);
}

void terminal_close(TerminalState *terminal)
{
    int status;

    if(terminal == NULL)
        return;
    if(terminal->running && terminal->pid > 0) {
        kill(terminal->pid, SIGHUP);
        kill(terminal->pid, SIGTERM);
        while(waitpid(terminal->pid, &status, 0) < 0 && errno == EINTR)
            ;
    }
    if(terminal->fd >= 0)
        close(terminal->fd);
    free(terminal->main_cells);
    free(terminal->alt_cells);
    free(terminal->scrollback);
    free(terminal->main_wrapped);
    free(terminal->alt_wrapped);
    free(terminal->scrollback_wrapped);
    free(terminal->tab_stops);
    clear_sixel_images(terminal, -1);
    free(terminal->sixel_images);
    free(terminal->dcs);
    terminal_init(terminal);
}

const Cell *terminal_cell(const TerminalState *terminal, int col, int row)
{
    const Cell *cells = screen_cells_const(terminal);

    if(terminal == NULL || cells == NULL || col < 0 || row < 0 ||
       col >= terminal->cols || row >= terminal->rows)
        return NULL;
    return cells + row * terminal->cols + col;
}

const Cell *terminal_visible_cell(const TerminalState *terminal, int col,
                                  int visible_row)
{
    int physical;

    if(terminal == NULL || col < 0 || visible_row < 0 ||
       col >= terminal->cols)
        return NULL;
    if(visible_row < terminal->scrollback_count) {
        if(terminal->scrollback == NULL || terminal->scrollback_capacity <= 0)
            return NULL;
        physical = (terminal->scrollback_head - terminal->scrollback_count +
                    visible_row + terminal->scrollback_capacity) %
                   terminal->scrollback_capacity;
        return terminal->scrollback + physical * terminal->cols + col;
    }
    return terminal_cell(terminal, col, visible_row - terminal->scrollback_count);
}

const char *terminal_hyperlink(const TerminalState *terminal, int hyperlink)
{
    if(terminal == NULL || hyperlink <= 0 ||
       hyperlink > terminal->hyperlink_count)
        return NULL;
    return terminal->hyperlinks[hyperlink - 1];
}

static int append_utf8(char *out, int out_size, int *used, unsigned int cp)
{
    if(cp < 0x80) {
        if(*used + 1 >= out_size)
            return 0;
        out[(*used)++] = (char)cp;
    } else if(cp < 0x800) {
        if(*used + 2 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xc0 | (cp >> 6));
        out[(*used)++] = (char)(0x80 | (cp & 0x3f));
    } else if(cp < 0x10000) {
        if(*used + 3 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xe0 | (cp >> 12));
        out[(*used)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (cp & 0x3f));
    } else {
        if(*used + 4 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xf0 | (cp >> 18));
        out[(*used)++] = (char)(0x80 | ((cp >> 12) & 0x3f));
        out[(*used)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (cp & 0x3f));
    }
    out[*used] = '\0';
    return 1;
}

static void copy_line(const Cell *cells, int cols, int row, char *out,
                      int out_size)
{
    int end;
    int col;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(cells == NULL || row < 0)
        return;
    end = cols;
    while(end > 0 && cells[row * cols + end - 1].codepoint == ' ')
        end--;
    for(col = 0; col < end; col++) {
        unsigned int cp = cells[row * cols + col].codepoint;

        if((cells[row * cols + col].style & STYLE_WIDE_CONT) != 0)
            continue;
        if(cp == 0)
            cp = ' ';
        if(!append_utf8(out, out_size, &used, cp))
            break;
        if(cells[row * cols + col].combining != 0 &&
           !append_utf8(out, out_size, &used,
                        cells[row * cols + col].combining))
            break;
    }
    out[used] = '\0';
}

void terminal_line(const TerminalState *terminal, int row, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(terminal == NULL || row < 0 || row >= terminal->rows)
        return;
    copy_line(screen_cells_const(terminal), terminal->cols, row, out, out_size);
}

int terminal_scrollback_rows(const TerminalState *terminal)
{
    return terminal != NULL ? terminal->scrollback_count : 0;
}

void terminal_scrollback_line(const TerminalState *terminal, int row, char *out,
                              int out_size)
{
    int physical;

    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(terminal == NULL || terminal->scrollback == NULL || row < 0 ||
       row >= terminal->scrollback_count)
        return;
    physical = terminal->scrollback_head - terminal->scrollback_count + row;
    while(physical < 0)
        physical += terminal->scrollback_capacity;
    physical %= terminal->scrollback_capacity;
    copy_line(terminal->scrollback, terminal->cols, physical, out, out_size);
}

int terminal_visible_line_count(const TerminalState *terminal)
{
    if(terminal == NULL)
        return 0;
    return terminal->scrollback_count + terminal->rows;
}

void terminal_visible_line(const TerminalState *terminal, int visible_row, char *out,
                           int out_size)
{
    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(terminal == NULL || visible_row < 0)
        return;
    if(visible_row < terminal->scrollback_count) {
        terminal_scrollback_line(terminal, visible_row, out, out_size);
        return;
    }
    terminal_line(terminal, visible_row - terminal->scrollback_count, out,
                  out_size);
}

int terminal_sixel_count(const TerminalState *terminal)
{
    return terminal != NULL ? terminal->sixel_count : 0;
}

const SixelImage *terminal_sixel_image(const TerminalState *terminal, int index)
{
    if(terminal == NULL || index < 0 || index >= terminal->sixel_count)
        return NULL;
    return terminal->sixel_images + index;
}
