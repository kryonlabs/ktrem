#include "terminal_screen.h"

#include "terminal_sixel.h"

#include <stdlib.h>
#include <string.h>

int terminal_clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

Cell terminal_blank_cell(const TerminalState *terminal)
{
    Cell cell;

    cell.codepoint = ' ';
    cell.combining = 0;
    cell.fg = terminal != NULL ? terminal->current_fg : COLOR_DEFAULT;
    cell.bg = terminal != NULL ? terminal->current_bg : COLOR_DEFAULT;
    cell.underline =
        terminal != NULL ? terminal->current_underline : COLOR_DEFAULT;
    cell.hyperlink = terminal != NULL ? terminal->current_hyperlink : 0;
    cell.style =
        terminal != NULL ? (unsigned short)terminal->current_style : 0;
    return cell;
}

Cell *terminal_screen_cells(TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

unsigned char *terminal_screen_wrapped(TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_wrapped
                                                          : terminal->main_wrapped;
}

void terminal_reset_tab_stops(TerminalState *terminal)
{
    int col;

    if(terminal == NULL || terminal->tab_stops == NULL)
        return;
    memset(terminal->tab_stops, 0, (size_t)terminal->cols);
    for(col = 8; col < terminal->cols; col += 8)
        terminal->tab_stops[col] = 1;
}

void terminal_clear_tab_stops(TerminalState *terminal)
{
    if(terminal == NULL || terminal->tab_stops == NULL)
        return;
    memset(terminal->tab_stops, 0, (size_t)terminal->cols);
}

void terminal_clear_cell_block(TerminalState *terminal, Cell *cells)
{
    Cell blank;
    int i;
    int count;

    if(terminal == NULL || cells == NULL)
        return;
    blank = terminal_blank_cell(terminal);
    count = terminal->cols * terminal->rows;
    for(i = 0; i < count; i++)
        cells[i] = blank;
}

void terminal_clear_row(TerminalState *terminal, int row)
{
    Cell *cells = terminal_screen_cells(terminal);
    unsigned char *wrapped = terminal_screen_wrapped(terminal);
    Cell blank;
    int col;

    if(terminal == NULL || cells == NULL || row < 0 || row >= terminal->rows)
        return;
    terminal_sixel_clear_range(terminal, row * terminal->cols,
                               (row + 1) * terminal->cols);
    blank = terminal_blank_cell(terminal);
    for(col = 0; col < terminal->cols; col++)
        cells[row * terminal->cols + col] = blank;
    if(wrapped != NULL)
        wrapped[row] = 0;
}

void terminal_clear_screen(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal_clear_cell_block(terminal, terminal_screen_cells(terminal));
    if(terminal_screen_wrapped(terminal) != NULL)
        memset(terminal_screen_wrapped(terminal), 0, (size_t)terminal->rows);
    terminal->cursor_col = 0;
    terminal->cursor_row = 0;
    terminal_sixel_clear_images(terminal, terminal->alternate_screen);
}

void terminal_screen_alignment_test(TerminalState *terminal)
{
    Cell *cells;
    unsigned char *wrapped;
    Cell cell;
    int count;
    int i;

    if(terminal == NULL)
        return;
    cells = terminal_screen_cells(terminal);
    if(cells == NULL)
        return;
    cell = terminal_blank_cell(terminal);
    cell.codepoint = 'E';
    cell.combining = 0;
    cell.style &= ~STYLE_WIDE_CONT;
    count = terminal->cols * terminal->rows;
    for(i = 0; i < count; i++)
        cells[i] = cell;
    wrapped = terminal_screen_wrapped(terminal);
    if(wrapped != NULL)
        memset(wrapped, 0, (size_t)terminal->rows);
    terminal->cursor_col = 0;
    terminal->cursor_row = 0;
    terminal_sixel_clear_images(terminal, terminal->alternate_screen);
}

void terminal_clear_alternate_screen(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal_clear_cell_block(terminal, terminal->alt_cells);
    if(terminal->alt_wrapped != NULL)
        memset(terminal->alt_wrapped, 0, (size_t)terminal->rows);
    terminal_sixel_clear_images(terminal, 1);
}

void terminal_clear_scrollback(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->scrollback_count = 0;
    terminal->scrollback_head = 0;
    if(terminal->scrollback_wrapped != NULL &&
       terminal->scrollback_capacity > 0)
        memset(terminal->scrollback_wrapped, 0,
               (size_t)terminal->scrollback_capacity);
    terminal_sixel_prune_scrollback(terminal);
}

int terminal_allocate_scrollback(TerminalState *terminal)
{
    Cell *scrollback;
    unsigned char *wrapped;
    int limit;

    if(terminal == NULL)
        return 0;
    limit = terminal->scrollback_limit > 0 ? terminal->scrollback_limit
                                           : SCROLLBACK_LIMIT;
    scrollback = realloc(terminal->scrollback,
                         (size_t)(limit * terminal->cols) * sizeof(Cell));
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
        memset(terminal->scrollback_wrapped, 0,
               (size_t)terminal->scrollback_capacity);
    terminal_sixel_prune_scrollback(terminal);
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

static int reflow_cell_blank(const void *cell, void *userdata)
{
    const Cell *terminal_cell = cell;

    (void)userdata;
    return terminal_cell == NULL || terminal_cell->codepoint == ' ';
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

static void terminal_append_scrollback(TerminalState *terminal,
                                       const Cell *row, int wrapped)
{
    int index;

    if(terminal == NULL || row == NULL || terminal->scrollback == NULL ||
       terminal->scrollback_capacity <= 0)
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

static void terminal_push_scrollback(TerminalState *terminal, const Cell *row,
                                     int wrapped)
{
    if(terminal == NULL || terminal->alternate_screen)
        return;
    terminal_append_scrollback(terminal, row, wrapped);
}

static int reflow_main_screen(TerminalState *terminal, const Cell *old_cells,
                              const unsigned char *old_wrapped, int old_cols,
                              int old_rows, int old_cursor_col,
                              int old_cursor_row, int *new_cursor_col,
                              int *new_cursor_row)
{
    Cell *lines;
    unsigned char *wrapped;
    TerminalPaneReflowSpec spec;
    Cell blank;
    int max_lines;
    int cursor_row = -1;
    int cursor_col = -1;
    int out_row = 0;
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
    blank = terminal_blank_cell(terminal);
    memset(&spec, 0, sizeof(spec));
    spec.input_cells = old_cells;
    spec.input_wrapped = old_wrapped;
    spec.input_cols = old_cols;
    spec.input_rows = old_rows;
    spec.output_cells = lines;
    spec.output_wrapped = wrapped;
    spec.output_cols = terminal->cols;
    spec.output_rows = max_lines;
    spec.cell_size = sizeof(Cell);
    spec.blank_cell = &blank;
    spec.is_blank = reflow_cell_blank;
    spec.trim_blank_rows_after_cursor = 1;
    spec.cursor_input_col = old_cursor_col;
    spec.cursor_input_row = old_cursor_row;
    spec.cursor_output_col = &cursor_col;
    spec.cursor_output_row = &cursor_row;
    spec.output_row_count = &out_row;
    if(!TerminalPaneReflowRows(&spec)) {
        free(lines);
        free(wrapped);
        return 0;
    }
    if(out_row <= 0) {
        free(lines);
        free(wrapped);
        return 1;
    }
    copy_rows = out_row < terminal->rows ? out_row : terminal->rows;
    start_row = out_row - copy_rows;
    memcpy(terminal->main_cells, lines + start_row * terminal->cols,
           (size_t)copy_rows * (size_t)terminal->cols * sizeof(Cell));
    memcpy(terminal->main_wrapped, wrapped + start_row, (size_t)copy_rows);
    if(cursor_row >= start_row && cursor_row < start_row + copy_rows) {
        if(new_cursor_row != NULL)
            *new_cursor_row = cursor_row - start_row;
        if(new_cursor_col != NULL)
            *new_cursor_col = cursor_col;
    } else {
        if(new_cursor_row != NULL)
            *new_cursor_row =
                terminal_clamp_int(old_cursor_row, 0, copy_rows - 1);
        if(new_cursor_col != NULL)
            *new_cursor_col = old_cursor_col;
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
        Cell blank = terminal_blank_cell(terminal);

        for(col = 0; col < terminal->cols; col++)
            lines[row * terminal->cols + col] = blank;
    }
    for(index = 0; index < old_count; index++) {
        int physical =
            (old_head - old_count + index + old_capacity) % old_capacity;
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
        terminal_append_scrollback(terminal, lines + row * terminal->cols,
                                   wrapped[row]);
    free(lines);
    free(wrapped);
}

static int reflow_scrollback_and_main(
    TerminalState *terminal, const Cell *old_scrollback,
    const unsigned char *old_scrollback_wrapped, int old_cols,
    int old_scrollback_count, int old_scrollback_head,
    int old_scrollback_capacity, const Cell *old_main,
    const unsigned char *old_main_wrapped, int old_rows, int old_cursor_col,
    int old_cursor_row, int *new_cursor_col, int *new_cursor_row)
{
    Cell *lines;
    unsigned char *wrapped;
    int source_rows;
    int max_lines;
    int out_row = 0;
    int out_col = 0;
    int cursor_row = -1;
    int cursor_col = -1;
    int row;
    int input_rows;
    int start_main;
    int copy_rows;

    if(terminal == NULL || old_scrollback == NULL || old_main == NULL ||
       old_cols <= 0 || old_rows <= 0 || old_scrollback_count <= 0 ||
       old_scrollback_capacity <= 0 || terminal->cols <= 0)
        return 0;
    input_rows = old_rows;
    while(input_rows > old_cursor_row + 1) {
        int soft = old_main_wrapped != NULL && old_main_wrapped[input_rows - 1];

        if(soft || row_text_end(old_main, old_cols, input_rows - 1, 0) > 0)
            break;
        input_rows--;
    }
    source_rows = old_scrollback_count + input_rows;
    max_lines =
        source_rows * ((old_cols + terminal->cols - 1) / terminal->cols + 2);
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
        Cell blank = terminal_blank_cell(terminal);

        for(col = 0; col < terminal->cols; col++)
            lines[row * terminal->cols + col] = blank;
    }
    for(row = 0; row < old_scrollback_count; row++) {
        int physical = (old_scrollback_head - old_scrollback_count + row +
                        old_scrollback_capacity) %
                       old_scrollback_capacity;
        int soft = old_scrollback_wrapped != NULL &&
                   old_scrollback_wrapped[physical];
        int end = row_text_end(old_scrollback, old_cols, physical, soft);
        int col;

        for(col = 0; col < end; col++)
            append_reflow_cell(lines, wrapped, terminal->cols, max_lines,
                               &out_row, &out_col,
                               old_scrollback[physical * old_cols + col]);
        if(!soft)
            finish_reflow_line(&out_row, &out_col, max_lines);
    }
    for(row = 0; row < input_rows; row++) {
        int soft = old_main_wrapped != NULL && old_main_wrapped[row];
        int end = row_text_end(old_main, old_cols, row, soft);
        int col;

        for(col = 0; col < end; col++) {
            if(row == old_cursor_row && col == old_cursor_col) {
                cursor_row = out_row;
                cursor_col = out_col;
            }
            append_reflow_cell(lines, wrapped, terminal->cols, max_lines,
                               &out_row, &out_col,
                               old_main[row * old_cols + col]);
        }
        if(row == old_cursor_row && cursor_row < 0) {
            cursor_row = out_row;
            cursor_col = out_col +
                         (old_cursor_col > end ? old_cursor_col - end : 0);
            while(cursor_col >= terminal->cols) {
                cursor_row++;
                cursor_col -= terminal->cols;
            }
        }
        if(!soft)
            finish_reflow_line(&out_row, &out_col, max_lines);
    }
    if(out_col > 0)
        finish_reflow_line(&out_row, &out_col, max_lines);
    start_main = out_row > terminal->rows ? out_row - terminal->rows : 0;
    for(row = 0; row < start_main; row++)
        terminal_append_scrollback(terminal, lines + row * terminal->cols,
                                   wrapped[row]);
    copy_rows = out_row - start_main;
    if(copy_rows > terminal->rows)
        copy_rows = terminal->rows;
    if(copy_rows > 0) {
        memcpy(terminal->main_cells, lines + start_main * terminal->cols,
               (size_t)copy_rows * (size_t)terminal->cols * sizeof(Cell));
        memcpy(terminal->main_wrapped, wrapped + start_main,
               (size_t)copy_rows);
    }
    if(cursor_row >= start_main && cursor_row < start_main + copy_rows) {
        if(new_cursor_row != NULL)
            *new_cursor_row = cursor_row - start_main;
        if(new_cursor_col != NULL)
            *new_cursor_col = cursor_col;
    } else {
        if(new_cursor_row != NULL)
            *new_cursor_row =
                terminal_clamp_int(old_cursor_row, 0, terminal->rows - 1);
        if(new_cursor_col != NULL)
            *new_cursor_col = old_cursor_col;
    }
    free(lines);
    free(wrapped);
    return 1;
}

int terminal_allocate_screen(TerminalState *terminal, int cols, int rows)
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
    int scrollback_allocated_early = 0;
    int combined_reflowed = 0;
    int reflow_cursor_col;
    int reflow_cursor_row;
    int allocated;
    int row;
    int col;

    if(terminal == NULL)
        return 0;
    cols = terminal_clamp_int(cols, 8, MAX_COLS);
    rows = terminal_clamp_int(rows, 4, MAX_ROWS);
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
    reflow_cursor_col = terminal->alternate_screen ? terminal->saved_col
                                                   : terminal->cursor_col;
    reflow_cursor_row = terminal->alternate_screen ? terminal->saved_row
                                                   : terminal->cursor_row;
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
    terminal_clear_cell_block(terminal, terminal->main_cells);
    terminal_clear_cell_block(terminal, terminal->alt_cells);

    if(scrollback_reflow_needed) {
        terminal->scrollback = NULL;
        terminal->scrollback_wrapped = NULL;
        terminal->scrollback_count = 0;
        terminal->scrollback_head = 0;
        terminal->scrollback_capacity = 0;
        scrollback_allocated_early = terminal_allocate_scrollback(terminal);
        if(scrollback_allocated_early &&
           reflow_scrollback_and_main(
               terminal, old_scrollback, old_scrollback_wrapped, old_cols,
               old_scrollback_count, old_scrollback_head,
               old_scrollback_capacity, old_main, old_main_wrapped, old_rows,
               reflow_cursor_col, reflow_cursor_row, &reflow_cursor_col,
               &reflow_cursor_row)) {
            main_reflowed = 1;
            combined_reflowed = 1;
            scrollback_reflow_needed = 0;
        }
    }

    copy_cols = old_cols < cols ? old_cols : cols;
    copy_rows = old_rows < rows ? old_rows : rows;
    if(old_main != NULL && old_cols > 0 && old_rows > 0 && old_cols != cols &&
       !main_reflowed &&
       reflow_main_screen(terminal, old_main, old_main_wrapped, old_cols,
                          old_rows, reflow_cursor_col, reflow_cursor_row,
                          &reflow_cursor_col, &reflow_cursor_row))
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
    reflow_cursor_col = terminal_clamp_int(reflow_cursor_col, 0, cols - 1);
    reflow_cursor_row = terminal_clamp_int(reflow_cursor_row, 0, rows - 1);
    if(main_reflowed && terminal->alternate_screen) {
        terminal->saved_col = reflow_cursor_col;
        terminal->saved_row = reflow_cursor_row;
    } else if(main_reflowed) {
        terminal->cursor_col = reflow_cursor_col;
        terminal->cursor_row = reflow_cursor_row;
    }
    terminal->cursor_col = terminal_clamp_int(terminal->cursor_col, 0, cols - 1);
    terminal->cursor_row = terminal_clamp_int(terminal->cursor_row, 0, rows - 1);
    if(scrollback_reflow_needed && !scrollback_allocated_early) {
        terminal->scrollback = NULL;
        terminal->scrollback_wrapped = NULL;
        terminal->scrollback_count = 0;
        terminal->scrollback_head = 0;
        terminal->scrollback_capacity = 0;
    }
    terminal_reset_tab_stops(terminal);
    allocated = scrollback_allocated_early ? 1 : terminal_allocate_scrollback(terminal);
    if(allocated && scrollback_reflow_needed) {
        reflow_scrollback(terminal, old_scrollback, old_scrollback_wrapped,
                          old_cols, old_scrollback_count, old_scrollback_head,
                          old_scrollback_capacity);
        free(old_scrollback);
        free(old_scrollback_wrapped);
    } else if(combined_reflowed) {
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

void terminal_scroll_up(TerminalState *terminal, int top, int bottom, int count)
{
    Cell *cells = terminal_screen_cells(terminal);
    unsigned char *wrapped = terminal_screen_wrapped(terminal);
    int rows;
    int i;

    if(terminal == NULL || cells == NULL)
        return;
    top = terminal_clamp_int(top, 0, terminal->rows - 1);
    bottom = terminal_clamp_int(bottom, 0, terminal->rows - 1);
    if(top > bottom)
        return;
    rows = bottom - top + 1;
    count = terminal_clamp_int(count, 1, rows);
    for(i = 0; i < count; i++)
        terminal_push_scrollback(terminal, cells + (top + i) * terminal->cols,
                                 wrapped != NULL ? wrapped[top + i] : 0);
    terminal_sixel_shift(terminal, top, bottom, -count);
    if(rows > count) {
        memmove(cells + top * terminal->cols,
                cells + (top + count) * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
        if(wrapped != NULL)
            memmove(wrapped + top, wrapped + top + count,
                    (size_t)(rows - count));
    }
    for(i = bottom - count + 1; i <= bottom; i++)
        terminal_clear_row(terminal, i);
}

void terminal_scroll_down(TerminalState *terminal, int top, int bottom,
                          int count)
{
    Cell *cells = terminal_screen_cells(terminal);
    unsigned char *wrapped = terminal_screen_wrapped(terminal);
    int rows;
    int i;

    if(terminal == NULL || cells == NULL)
        return;
    top = terminal_clamp_int(top, 0, terminal->rows - 1);
    bottom = terminal_clamp_int(bottom, 0, terminal->rows - 1);
    if(top > bottom)
        return;
    rows = bottom - top + 1;
    count = terminal_clamp_int(count, 1, rows);
    terminal_sixel_shift(terminal, top, bottom, count);
    if(rows > count) {
        memmove(cells + (top + count) * terminal->cols,
                cells + top * terminal->cols,
                (size_t)(rows - count) * terminal->cols * sizeof(Cell));
        if(wrapped != NULL)
            memmove(wrapped + top + count, wrapped + top,
                    (size_t)(rows - count));
    }
    for(i = top; i < top + count; i++)
        terminal_clear_row(terminal, i);
}

void terminal_clamp_cursor(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->cursor_col =
        terminal_clamp_int(terminal->cursor_col, 0, terminal->cols - 1);
    if(terminal->origin_mode)
        terminal->cursor_row = terminal_clamp_int(
            terminal->cursor_row, terminal->scroll_top, terminal->scroll_bottom);
    else
        terminal->cursor_row =
            terminal_clamp_int(terminal->cursor_row, 0, terminal->rows - 1);
}

void terminal_erase_range(TerminalState *terminal, int start, int end)
{
    Cell *cells = terminal_screen_cells(terminal);
    Cell blank;
    int i;
    int limit;

    if(terminal == NULL || cells == NULL)
        return;
    limit = terminal->cols * terminal->rows;
    start = terminal_clamp_int(start, 0, limit);
    end = terminal_clamp_int(end, 0, limit);
    if(start >= end)
        return;
    terminal_sixel_clear_range(terminal, start, end);
    blank = terminal_blank_cell(terminal);
    for(i = start; i < end; i++)
        cells[i] = blank;
}

void terminal_insert_blank_chars(TerminalState *terminal, int count)
{
    Cell *cells = terminal_screen_cells(terminal);
    Cell blank;
    int row_start;
    int available;
    int col;

    if(terminal == NULL || cells == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = terminal_clamp_int(count, 1, available);
    row_start = terminal->cursor_row * terminal->cols;
    if(available > count) {
        memmove(cells + row_start + terminal->cursor_col + count,
                cells + row_start + terminal->cursor_col,
                (size_t)(available - count) * sizeof(Cell));
    }
    blank = terminal_blank_cell(terminal);
    for(col = 0; col < count; col++)
        cells[row_start + terminal->cursor_col + col] = blank;
}

void terminal_delete_chars(TerminalState *terminal, int count)
{
    Cell *cells = terminal_screen_cells(terminal);
    Cell blank;
    int row_start;
    int available;
    int col;

    if(terminal == NULL || cells == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = terminal_clamp_int(count, 1, available);
    row_start = terminal->cursor_row * terminal->cols;
    if(available > count) {
        memmove(cells + row_start + terminal->cursor_col,
                cells + row_start + terminal->cursor_col + count,
                (size_t)(available - count) * sizeof(Cell));
    }
    blank = terminal_blank_cell(terminal);
    for(col = terminal->cols - count; col < terminal->cols; col++)
        cells[row_start + col] = blank;
}

void terminal_erase_chars(TerminalState *terminal, int count)
{
    int available;

    if(terminal == NULL)
        return;
    available = terminal->cols - terminal->cursor_col;
    count = terminal_clamp_int(count, 1, available);
    terminal_erase_range(terminal,
                         terminal->cursor_row * terminal->cols +
                             terminal->cursor_col,
                         terminal->cursor_row * terminal->cols +
                             terminal->cursor_col + count);
}

int terminal_next_tab_stop(const TerminalState *terminal)
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
