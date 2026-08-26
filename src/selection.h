#ifndef KTREM_SELECTION_H
#define KTREM_SELECTION_H

#include "terminal.h"
#include "terminal_pane.h"

#include <stddef.h>

enum {
    SELECTION_MODE_CHAR = TERMINAL_PANE_SELECTION_CHAR,
    SELECTION_MODE_WORD = TERMINAL_PANE_SELECTION_WORD,
    SELECTION_MODE_LINE = TERMINAL_PANE_SELECTION_LINE
};

typedef TerminalPaneSelection Selection;

void selection_clear(Selection *selection);
void selection_set_range(Selection *selection, int mode, int dragging,
                         int start_row, int start_col, int end_row,
                         int end_col);
void selection_begin_char(Selection *selection, int row, int col);
void selection_select_line(Selection *selection, const TerminalState *terminal,
                           int row);
void selection_select_word(Selection *selection, const TerminalState *terminal,
                           int row, int col);
void selection_select_all(Selection *selection, const TerminalState *terminal);
void selection_update_end(Selection *selection, const TerminalState *terminal,
                          int row, int col);
int selection_contains(const Selection *selection, int row, int col);
int selection_collect_text(const Selection *selection,
                           const TerminalState *terminal, char *buffer,
                           size_t buffer_size);
TerminalPaneClipboardController selection_clipboard_controller(
    Selection *selection, TerminalState *terminal);
int selection_update_primary(const Selection *selection, TerminalState *terminal);
int selection_copy_to_clipboard(const Selection *selection,
                                TerminalState *terminal);
int selection_edge_scroll_delta(float mouse_y, float viewport_y,
                                float viewport_height, float edge_size);
int selection_first_visible_row(int total_rows, int visible_rows,
                                int scroll_offset);
int selection_edge_scroll_row(int first_visible_row, int visible_rows,
                              int scroll_delta);

#endif
