#include "selection.h"

#include <limits.h>

static int terminal_line_text(void *userdata, int row, char *out,
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

static int terminal_line_wrapped(void *userdata, int row)
{
    const TerminalState *terminal = userdata;

    if(terminal == NULL || row < 0)
        return 0;
    return terminal_visible_line_wrapped(terminal, row);
}

void selection_clear(Selection *selection)
{
    TerminalPaneSelectionClear(selection);
}

void selection_set_range(Selection *selection, int mode, int dragging,
                         int start_row, int start_col, int end_row,
                         int end_col)
{
    TerminalPaneSelectionSetRange(selection, mode, dragging, start_row,
                                  start_col, end_row, end_col);
}

void selection_begin_char(Selection *selection, int row, int col)
{
    TerminalPaneSelectionBeginChar(selection, row, col);
}

void selection_select_line(Selection *selection, const TerminalState *terminal,
                           int row)
{
    TerminalPaneSelectionSelectLine(selection, terminal_line_text,
                                    (void *)terminal, row);
}

void selection_select_word(Selection *selection, const TerminalState *terminal,
                           int row, int col)
{
    TerminalPaneSelectionSelectWord(selection, terminal_line_text,
                                    (void *)terminal, row, col);
}

void selection_select_all(Selection *selection, const TerminalState *terminal)
{
    TerminalPaneClipboardController controller =
        selection_clipboard_controller(selection, (TerminalState *)terminal);

    (void)TerminalPaneClipboardPerformCommand(
        controller, TERMINAL_PANE_CLIPBOARD_COMMAND_SELECT_ALL, NULL);
}

void selection_update_end(Selection *selection, const TerminalState *terminal,
                          int row, int col)
{
    TerminalPaneSelectionUpdateEnd(selection, terminal_line_text,
                                   (void *)terminal, row, col);
}

int selection_contains(const Selection *selection, int row, int col)
{
    return TerminalPaneSelectionContains(selection, row, col);
}

int selection_collect_text(const Selection *selection,
                           const TerminalState *terminal, char *buffer,
                           size_t buffer_size)
{
    if(buffer_size > (size_t)INT_MAX)
        buffer_size = (size_t)INT_MAX;
    return TerminalPaneSelectionCollectText(
        selection, terminal_line_text, terminal_line_wrapped, (void *)terminal,
        buffer, (int)buffer_size);
}

TerminalPaneClipboardController selection_clipboard_controller(
    Selection *selection, TerminalState *terminal)
{
    TerminalPaneClipboardController controller = {0};

    if(terminal == NULL)
        return controller;
    return MakeTerminalPaneClipboardController(
        terminal_clipboard(terminal), selection, terminal_line_text,
        terminal_line_wrapped, terminal, terminal_visible_line_count(terminal),
        terminal->cols, NULL);
}

int selection_update_primary(const Selection *selection, TerminalState *terminal)
{
    return TerminalPaneClipboardPerformCommand(
        selection_clipboard_controller((Selection *)selection, terminal),
        TERMINAL_PANE_CLIPBOARD_COMMAND_UPDATE_PRIMARY_SELECTION, NULL);
}

int selection_copy_to_clipboard(const Selection *selection,
                                TerminalState *terminal)
{
    return TerminalPaneClipboardPerformCommand(
        selection_clipboard_controller((Selection *)selection, terminal),
        TERMINAL_PANE_CLIPBOARD_COMMAND_COPY_SELECTION, NULL);
}

int selection_edge_scroll_delta(float mouse_y, float viewport_y,
                                float viewport_height, float edge_size)
{
    return TerminalPaneSelectionEdgeScrollDelta(mouse_y, viewport_y,
                                                viewport_height, edge_size);
}

int selection_first_visible_row(int total_rows, int visible_rows,
                                int scroll_offset)
{
    return TerminalPaneSelectionFirstVisibleRow(total_rows, visible_rows,
                                                scroll_offset);
}

int selection_edge_scroll_row(int first_visible_row, int visible_rows,
                              int scroll_delta)
{
    return TerminalPaneSelectionEdgeScrollRow(first_visible_row, visible_rows,
                                              scroll_delta);
}
