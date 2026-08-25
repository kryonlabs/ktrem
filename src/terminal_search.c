#include "terminal.h"

static int terminal_search_line_text(void *userdata, int row, char *out,
                                     int out_size)
{
    const TerminalState *terminal = userdata;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(terminal == NULL || row < 0)
        return 0;
    terminal_visible_line(terminal, row, out, out_size);
    return 1;
}

int terminal_find_visible(const TerminalState *terminal, const char *needle,
                          int start_row, int start_col, int direction,
                          int wrap, TerminalSearchMatch *out)
{
    if(terminal == NULL)
        return 0;
    return TerminalPaneSearchLines(terminal_search_line_text, (void *)terminal,
                                   terminal_visible_line_count(terminal),
                                   needle, start_row, start_col, direction,
                                   wrap, (TerminalPaneSearchMatch *)out);
}

TerminalPaneSearchController terminal_search_controller(
    const TerminalState *terminal, TerminalPaneSelection *selection,
    int visible_rows, int first_visible_row, int *scroll_offset)
{
    return MakeTerminalPaneSearchController(
        terminal_search_line_text, (void *)terminal, selection,
        terminal != NULL ? terminal_visible_line_count(terminal) : 0,
        visible_rows, first_visible_row, scroll_offset);
}
