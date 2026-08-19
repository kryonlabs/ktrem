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
    STATE_OSC_ESCAPE
};

static int clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static Cell blank_cell(const Terminal *terminal)
{
    Cell cell;

    cell.codepoint = ' ';
    cell.fg = terminal != NULL ? terminal->current_fg : COLOR_DEFAULT;
    cell.bg = terminal != NULL ? terminal->current_bg : COLOR_DEFAULT;
    cell.style = terminal != NULL ? (unsigned char)terminal->current_style : 0;
    return cell;
}

static Cell *screen_cells(Terminal *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

static const Cell *screen_cells_const(const Terminal *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

static void reset_style(Terminal *terminal)
{
    terminal->current_fg = COLOR_DEFAULT;
    terminal->current_bg = COLOR_DEFAULT;
    terminal->current_style = 0;
}

static void reset_tab_stops(Terminal *terminal)
{
    int col;

    if(terminal == NULL || terminal->tab_stops == NULL)
        return;
    memset(terminal->tab_stops, 0, (size_t)terminal->cols);
    for(col = 8; col < terminal->cols; col += 8)
        terminal->tab_stops[col] = 1;
}

void terminal_init(Terminal *terminal)
{
    if(terminal == NULL)
        return;
    memset(terminal, 0, sizeof(*terminal));
    terminal->fd = -1;
    terminal->cursor_visible = 1;
    terminal->scrollback_limit = SCROLLBACK_LIMIT;
    reset_style(terminal);
}

static void clear_cell_block(Terminal *terminal, Cell *cells)
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

static void clear_row(Terminal *terminal, int row)
{
    Cell *cells = screen_cells(terminal);
    Cell blank;
    int col;

    if(terminal == NULL || cells == NULL || row < 0 || row >= terminal->rows)
        return;
    blank = blank_cell(terminal);
    for(col = 0; col < terminal->cols; col++)
        cells[row * terminal->cols + col] = blank;
}

static void clear_screen(Terminal *terminal)
{
    if(terminal == NULL)
        return;
    clear_cell_block(terminal, screen_cells(terminal));
    terminal->cursor_col = 0;
    terminal->cursor_row = 0;
}

static int allocate_scrollback(Terminal *terminal)
{
    Cell *scrollback;
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
    terminal->scrollback = scrollback;
    terminal->scrollback_capacity = limit;
    if(terminal->scrollback_count > terminal->scrollback_capacity)
        terminal->scrollback_count = terminal->scrollback_capacity;
    if(terminal->scrollback_head >= terminal->scrollback_capacity)
        terminal->scrollback_head = 0;
    return 1;
}

static int allocate_screen(Terminal *terminal, int cols, int rows)
{
    Cell *old_main;
    Cell *old_alt;
    unsigned char *old_tabs;
    int old_cols;
    int old_rows;
    int copy_cols;
    int copy_rows;
    int row;
    int col;

    cols = clamp_int(cols, 8, MAX_COLS);
    rows = clamp_int(rows, 4, MAX_ROWS);
    old_main = terminal->main_cells;
    old_alt = terminal->alt_cells;
    old_tabs = terminal->tab_stops;
    old_cols = terminal->cols;
    old_rows = terminal->rows;

    terminal->main_cells = calloc((size_t)(cols * rows), sizeof(Cell));
    terminal->alt_cells = calloc((size_t)(cols * rows), sizeof(Cell));
    terminal->tab_stops = calloc((size_t)cols, 1);
    if(terminal->main_cells == NULL || terminal->alt_cells == NULL ||
       terminal->tab_stops == NULL) {
        free(terminal->main_cells);
        free(terminal->alt_cells);
        free(terminal->tab_stops);
        terminal->main_cells = old_main;
        terminal->alt_cells = old_alt;
        terminal->tab_stops = old_tabs;
        return 0;
    }

    terminal->cols = cols;
    terminal->rows = rows;
    clear_cell_block(terminal, terminal->main_cells);
    clear_cell_block(terminal, terminal->alt_cells);

    copy_cols = old_cols < cols ? old_cols : cols;
    copy_rows = old_rows < rows ? old_rows : rows;
    for(row = 0; row < copy_rows; row++) {
        for(col = 0; col < copy_cols; col++) {
            if(old_main != NULL)
                terminal->main_cells[row * cols + col] =
                    old_main[row * old_cols + col];
            if(old_alt != NULL)
                terminal->alt_cells[row * cols + col] =
                    old_alt[row * old_cols + col];
        }
    }
    free(old_main);
    free(old_alt);
    free(old_tabs);

    terminal->scroll_top = 0;
    terminal->scroll_bottom = rows - 1;
    terminal->cursor_col = clamp_int(terminal->cursor_col, 0, cols - 1);
    terminal->cursor_row = clamp_int(terminal->cursor_row, 0, rows - 1);
    reset_tab_stops(terminal);
    return allocate_scrollback(terminal);
}

static void set_window_size(int fd, int cols, int rows)
{
    struct winsize size;

    memset(&size, 0, sizeof(size));
    size.ws_col = (unsigned short)cols;
    size.ws_row = (unsigned short)rows;
    ioctl(fd, TIOCSWINSZ, &size);
}

static void push_scrollback(Terminal *terminal, const Cell *row)
{
    int index;

    if(terminal == NULL || row == NULL || terminal->alternate_screen ||
       terminal->scrollback == NULL || terminal->scrollback_capacity <= 0)
        return;
    index = terminal->scrollback_head;
    memcpy(terminal->scrollback + index * terminal->cols, row,
           (size_t)terminal->cols * sizeof(Cell));
    terminal->scrollback_head =
        (terminal->scrollback_head + 1) % terminal->scrollback_capacity;
    if(terminal->scrollback_count < terminal->scrollback_capacity)
        terminal->scrollback_count++;
}

static void scroll_up(Terminal *terminal, int top, int bottom, int count)
{
    Cell *cells = screen_cells(terminal);
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
        push_scrollback(terminal, cells + top * terminal->cols);
    if(rows > count) {
        memmove(cells + top * terminal->cols,
                cells + (top + count) * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
    }
    for(i = bottom - count + 1; i <= bottom; i++)
        clear_row(terminal, i);
}

static void scroll_down(Terminal *terminal, int top, int bottom, int count)
{
    Cell *cells = screen_cells(terminal);
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
    if(rows > count) {
        memmove(cells + (top + count) * terminal->cols,
                cells + top * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
    }
    for(i = top; i < top + count; i++)
        clear_row(terminal, i);
}

static void clamp_cursor(Terminal *terminal)
{
    terminal->cursor_col = clamp_int(terminal->cursor_col, 0, terminal->cols - 1);
    terminal->cursor_row = clamp_int(terminal->cursor_row, 0, terminal->rows - 1);
}

static void line_feed(Terminal *terminal)
{
    if(terminal->cursor_row == terminal->scroll_bottom)
        scroll_up(terminal, terminal->scroll_top, terminal->scroll_bottom, 1);
    else
        terminal->cursor_row++;
    clamp_cursor(terminal);
}

static void erase_range(Terminal *terminal, int start, int end)
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
    blank = blank_cell(terminal);
    for(i = start; i < end; i++)
        cells[i] = blank;
}

static void insert_blank_chars(Terminal *terminal, int count)
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

static void delete_chars(Terminal *terminal, int count)
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

static void erase_chars(Terminal *terminal, int count)
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

static int next_tab_stop(const Terminal *terminal)
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

static void put_codepoint(Terminal *terminal, unsigned int codepoint)
{
    Cell *cells = screen_cells(terminal);
    Cell cell;

    if(terminal == NULL || cells == NULL)
        return;
    if(terminal->cursor_col >= terminal->cols) {
        terminal->cursor_col = 0;
        line_feed(terminal);
    }
    clamp_cursor(terminal);
    cell.codepoint = codepoint >= 32 ? codepoint : '?';
    cell.fg = terminal->current_fg;
    cell.bg = terminal->current_bg;
    cell.style = (unsigned char)terminal->current_style;
    cells[terminal->cursor_row * terminal->cols + terminal->cursor_col] = cell;
    terminal->cursor_col++;
}

static int csi_arg(const Terminal *terminal, int index, int fallback)
{
    if(index < 0 || index >= terminal->csi_count ||
       terminal->csi_args[index] == 0)
        return fallback;
    return terminal->csi_args[index];
}

static void apply_style(Terminal *terminal)
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
        else if(arg >= 30 && arg <= 37)
            terminal->current_fg = arg - 30;
        else if(arg >= 40 && arg <= 47)
            terminal->current_bg = arg - 40;
        else if(arg >= 90 && arg <= 97)
            terminal->current_fg = 8 + arg - 90;
        else if(arg >= 100 && arg <= 107)
            terminal->current_bg = 8 + arg - 100;
        else if((arg == 38 || arg == 48) && i + 2 < terminal->csi_count &&
                terminal->csi_args[i + 1] == 5) {
            if(arg == 38)
                terminal->current_fg = terminal->csi_args[i + 2] & 255;
            else
                terminal->current_bg = terminal->csi_args[i + 2] & 255;
            i += 2;
        } else if((arg == 38 || arg == 48) && i + 4 < terminal->csi_count &&
                  terminal->csi_args[i + 1] == 2) {
            int rgb = ((terminal->csi_args[i + 2] & 255) << 16) |
                      ((terminal->csi_args[i + 3] & 255) << 8) |
                      (terminal->csi_args[i + 4] & 255);

            if(arg == 38)
                terminal->current_fg = COLOR_TRUE_RGB | rgb;
            else
                terminal->current_bg = COLOR_TRUE_RGB | rgb;
            i += 4;
        }
    }
}

static void set_private_mode(Terminal *terminal, int mode, int enabled)
{
    if(mode == 25) {
        terminal->cursor_visible = enabled ? 1 : 0;
    } else if(mode == 1) {
        terminal->application_cursor_keys = enabled ? 1 : 0;
    } else if(mode == 1000 || mode == 1002 || mode == 1003) {
        terminal->mouse_mode = enabled ? mode : 0;
    } else if(mode == 1006) {
        terminal->mouse_sgr = enabled ? 1 : 0;
    } else if(mode == 2004) {
        terminal->bracketed_paste = enabled ? 1 : 0;
    } else if(mode == 47 || mode == 1047 || mode == 1049) {
        if(enabled) {
            terminal->saved_col = terminal->cursor_col;
            terminal->saved_row = terminal->cursor_row;
            terminal->alternate_screen = 1;
            clear_screen(terminal);
        } else {
            terminal->alternate_screen = 0;
            if(mode == 1049) {
                terminal->cursor_col = terminal->saved_col;
                terminal->cursor_row = terminal->saved_row;
                clamp_cursor(terminal);
            }
        }
    }
}

static void apply_csi(Terminal *terminal, int final)
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
        terminal->cursor_row = n - 1;
    else if(final == 'H' || final == 'f') {
        terminal->cursor_row = csi_arg(terminal, 0, 1) - 1;
        terminal->cursor_col = csi_arg(terminal, 1, 1) - 1;
    } else if(final == '@') {
        insert_blank_chars(terminal, n);
    } else if(final == 'J') {
        int mode = csi_arg(terminal, 0, 0);

        at = terminal->cursor_row * terminal->cols + terminal->cursor_col;
        if(mode == 2 || mode == 3)
            clear_screen(terminal);
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
            terminal->cursor_row = 0;
        }
    } else if(final == 'S') {
        scroll_up(terminal, terminal->scroll_top, terminal->scroll_bottom, n);
    } else if(final == 'T') {
        scroll_down(terminal, terminal->scroll_top, terminal->scroll_bottom, n);
    } else if(final == 's') {
        terminal->saved_col = terminal->cursor_col;
        terminal->saved_row = terminal->cursor_row;
    } else if(final == 'u') {
        terminal->cursor_col = terminal->saved_col;
        terminal->cursor_row = terminal->saved_row;
    } else if(final == 'g') {
        int mode = csi_arg(terminal, 0, 0);

        if(mode == 0 && terminal->tab_stops != NULL)
            terminal->tab_stops[terminal->cursor_col] = 0;
        else if(mode == 3)
            reset_tab_stops(terminal);
    } else if((final == 'h' || final == 'l') && terminal->csi_private == '?') {
        for(i = 0; i < terminal->csi_count; i++)
            set_private_mode(terminal, terminal->csi_args[i], final == 'h');
    }
    clamp_cursor(terminal);
}

static void start_csi(Terminal *terminal)
{
    int i;

    terminal->parser_state = STATE_CSI;
    terminal->csi_private = 0;
    terminal->csi_intermediate = 0;
    terminal->csi_count = 0;
    for(i = 0; i < MAX_CSI_ARGS; i++)
        terminal->csi_args[i] = 0;
}

static void finish_title(Terminal *terminal)
{
    char *separator;

    terminal->title[sizeof(terminal->title) - 1] = '\0';
    separator = strchr(terminal->title, ';');
    if(separator != NULL &&
       (terminal->title[0] == '0' || terminal->title[0] == '1' ||
        terminal->title[0] == '2')) {
        memmove(terminal->title, separator + 1, strlen(separator + 1) + 1);
    }
}

static void feed_codepoint(Terminal *terminal, unsigned int codepoint)
{
    if(terminal->parser_state == STATE_ESCAPE) {
        if(codepoint == '[') {
            start_csi(terminal);
            return;
        }
        if(codepoint == ']') {
            terminal->parser_state = STATE_OSC;
            terminal->title_len = 0;
            terminal->title[0] = '\0';
            return;
        }
        if(codepoint == '7') {
            terminal->saved_col = terminal->cursor_col;
            terminal->saved_row = terminal->cursor_row;
        } else if(codepoint == '8') {
            terminal->cursor_col = terminal->saved_col;
            terminal->cursor_row = terminal->saved_row;
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
            terminal->application_cursor_keys = 0;
            terminal->bracketed_paste = 0;
            terminal->mouse_mode = 0;
            terminal->mouse_sgr = 0;
            reset_tab_stops(terminal);
        }
        terminal->parser_state = STATE_TEXT;
        clamp_cursor(terminal);
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
            finish_title(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b) {
            terminal->parser_state = STATE_OSC_ESCAPE;
            return;
        }
        if(terminal->title_len < (int)sizeof(terminal->title) - 1) {
            terminal->title[terminal->title_len++] = (char)codepoint;
            terminal->title[terminal->title_len] = '\0';
        }
        return;
    }
    if(terminal->parser_state == STATE_OSC_ESCAPE) {
        if(codepoint == '\\')
            finish_title(terminal);
        terminal->parser_state = STATE_TEXT;
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
    if(codepoint == 7)
        return;
    if(codepoint >= 32)
        put_codepoint(terminal, codepoint);
}

static void feed_byte(Terminal *terminal, unsigned char byte)
{
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

int terminal_spawn(Terminal *terminal, const char *cwd, const char *shell,
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

int terminal_open(Terminal *terminal, const char *cwd, int cols, int rows)
{
    return terminal_spawn(terminal, cwd, NULL, NULL, cols, rows);
}

int terminal_write(Terminal *terminal, const void *data, int size)
{
    int written;

    if(terminal == NULL || !terminal->running || terminal->fd < 0 ||
       data == NULL || size <= 0)
        return 0;
    written = (int)write(terminal->fd, data, (size_t)size);
    return written > 0 ? written : 0;
}

int terminal_write_text(Terminal *terminal, const char *text)
{
    if(text == NULL)
        return 0;
    return terminal_write(terminal, text, (int)strlen(text));
}

int terminal_send_key(Terminal *terminal, int key, int mods)
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
    else if(key == KEY_TAB_CODE)
        plain = "\t";
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
    }

    if(plain != NULL) {
        if((mods & MOD_ALT) != 0) {
            snprintf(seq, sizeof(seq), "\x1b%s", plain);
            return terminal_write_text(terminal, seq);
        }
        return terminal_write_text(terminal, plain);
    }
    if(final != 0) {
        if(modifier == 1 && terminal != NULL && terminal->application_cursor_keys)
            snprintf(seq, sizeof(seq), "\x1bO%c", final);
        else if(modifier == 1)
            snprintf(seq, sizeof(seq), "\x1b[%c", final);
        else
            snprintf(seq, sizeof(seq), "\x1b[1;%d%c", modifier, final);
        return terminal_write_text(terminal, seq);
    }
    return 0;
}

int terminal_send_paste(Terminal *terminal, const char *text)
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

void terminal_feed(Terminal *terminal, const void *data, int size)
{
    const unsigned char *bytes = data;
    int i;

    if(terminal == NULL || bytes == NULL || size <= 0)
        return;
    for(i = 0; i < size; i++)
        feed_byte(terminal, bytes[i]);
}

int terminal_poll(Terminal *terminal)
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

void terminal_resize(Terminal *terminal, int cols, int rows)
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

void terminal_set_scrollback_limit(Terminal *terminal, int rows)
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

void terminal_close(Terminal *terminal)
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
    free(terminal->tab_stops);
    terminal_init(terminal);
}

const Cell *terminal_cell(const Terminal *terminal, int col, int row)
{
    const Cell *cells = screen_cells_const(terminal);

    if(terminal == NULL || cells == NULL || col < 0 || row < 0 ||
       col >= terminal->cols || row >= terminal->rows)
        return NULL;
    return cells + row * terminal->cols + col;
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

        if(cp == 0)
            cp = ' ';
        if(!append_utf8(out, out_size, &used, cp))
            break;
    }
    out[used] = '\0';
}

void terminal_line(const Terminal *terminal, int row, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(terminal == NULL || row < 0 || row >= terminal->rows)
        return;
    copy_line(screen_cells_const(terminal), terminal->cols, row, out, out_size);
}

int terminal_scrollback_rows(const Terminal *terminal)
{
    return terminal != NULL ? terminal->scrollback_count : 0;
}

void terminal_scrollback_line(const Terminal *terminal, int row, char *out,
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

int terminal_visible_line_count(const Terminal *terminal)
{
    if(terminal == NULL)
        return 0;
    return terminal->scrollback_count + terminal->rows;
}

void terminal_visible_line(const Terminal *terminal, int visible_row, char *out,
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
