#include "terminal.h"

static const Cell *terminal_screen_cells_const(const TerminalState *terminal)
{
    return terminal != NULL && terminal->alternate_screen ? terminal->alt_cells
                                                          : terminal->main_cells;
}

const Cell *terminal_cell(const TerminalState *terminal, int col, int row)
{
    const Cell *cells = terminal_screen_cells_const(terminal);

    if(terminal == NULL || cells == NULL || col < 0 || row < 0 ||
       col >= terminal->cols || row >= terminal->rows)
        return NULL;
    return cells + row * terminal->cols + col;
}

const Cell *terminal_visible_cell(const TerminalState *terminal, int col,
                                  int visible_row)
{
    int physical;
    int scrollback_count;

    if(terminal == NULL || col < 0 || visible_row < 0 ||
       col >= terminal->cols)
        return NULL;
    scrollback_count = terminal->alternate_screen ? 0 : terminal->scrollback_count;
    if(visible_row < scrollback_count) {
        if(terminal->scrollback == NULL || terminal->scrollback_capacity <= 0)
            return NULL;
        physical = (terminal->scrollback_head - scrollback_count +
                    visible_row + terminal->scrollback_capacity) %
                   terminal->scrollback_capacity;
        return terminal->scrollback + physical * terminal->cols + col;
    }
    return terminal_cell(terminal, col, visible_row - scrollback_count);
}

const char *terminal_hyperlink(const TerminalState *terminal, int hyperlink)
{
    if(terminal == NULL || hyperlink <= 0 ||
       hyperlink > terminal->hyperlink_count)
        return NULL;
    return terminal->hyperlinks[hyperlink - 1];
}

const char *terminal_hyperlink_id(const TerminalState *terminal, int hyperlink)
{
    if(terminal == NULL || hyperlink <= 0 ||
       hyperlink > terminal->hyperlink_count)
        return NULL;
    return terminal->hyperlink_ids[hyperlink - 1];
}

static void copy_terminal_line(const Cell *cells, int cols, int row, char *out,
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
        if(!AppendTerminalPaneUTF8Codepoint(out, out_size, &used, cp))
            break;
        if(cells[row * cols + col].combining != 0 &&
           !AppendTerminalPaneUTF8Codepoint(
               out, out_size, &used, cells[row * cols + col].combining))
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
    copy_terminal_line(terminal_screen_cells_const(terminal), terminal->cols,
                       row, out, out_size);
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
    copy_terminal_line(terminal->scrollback, terminal->cols, physical, out,
                       out_size);
}

int terminal_visible_line_count(const TerminalState *terminal)
{
    if(terminal == NULL)
        return 0;
    if(terminal->alternate_screen)
        return terminal->rows;
    return terminal->scrollback_count + terminal->rows;
}

int terminal_scrollback_rows(const TerminalState *terminal)
{
    return terminal != NULL ? terminal->scrollback_count : 0;
}

void terminal_visible_line(const TerminalState *terminal, int visible_row,
                           char *out, int out_size)
{
    int scrollback_count;

    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(terminal == NULL || visible_row < 0)
        return;
    scrollback_count = terminal->alternate_screen ? 0 : terminal->scrollback_count;
    if(visible_row < scrollback_count) {
        terminal_scrollback_line(terminal, visible_row, out, out_size);
        return;
    }
    terminal_line(terminal, visible_row - scrollback_count, out, out_size);
}

int terminal_visible_line_wrapped(const TerminalState *terminal, int visible_row)
{
    int scrollback_count;
    int physical;
    int row;

    if(terminal == NULL || visible_row < 0)
        return 0;
    scrollback_count = terminal->alternate_screen ? 0 : terminal->scrollback_count;
    if(visible_row < scrollback_count) {
        if(terminal->scrollback_wrapped == NULL ||
           terminal->scrollback_capacity <= 0)
            return 0;
        physical = terminal->scrollback_head - scrollback_count + visible_row;
        while(physical < 0)
            physical += terminal->scrollback_capacity;
        physical %= terminal->scrollback_capacity;
        return terminal->scrollback_wrapped[physical] ? 1 : 0;
    }
    row = visible_row - scrollback_count;
    if(row < 0 || row >= terminal->rows)
        return 0;
    if(terminal->alternate_screen)
        return terminal->alt_wrapped != NULL && terminal->alt_wrapped[row] ? 1 : 0;
    return terminal->main_wrapped != NULL && terminal->main_wrapped[row] ? 1 : 0;
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
