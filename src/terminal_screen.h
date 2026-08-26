#ifndef KTREM_TERMINAL_SCREEN_H
#define KTREM_TERMINAL_SCREEN_H

#include "terminal.h"

int terminal_clamp_int(int value, int low, int high);
Cell terminal_blank_cell(const TerminalState *terminal);
Cell *terminal_screen_cells(TerminalState *terminal);
unsigned char *terminal_screen_wrapped(TerminalState *terminal);
void terminal_reset_tab_stops(TerminalState *terminal);
void terminal_clear_tab_stops(TerminalState *terminal);
void terminal_clear_cell_block(TerminalState *terminal, Cell *cells);
void terminal_clear_row(TerminalState *terminal, int row);
void terminal_clear_screen(TerminalState *terminal);
void terminal_screen_alignment_test(TerminalState *terminal);
void terminal_clear_alternate_screen(TerminalState *terminal);
void terminal_clear_scrollback(TerminalState *terminal);
int terminal_allocate_scrollback(TerminalState *terminal);
int terminal_allocate_screen(TerminalState *terminal, int cols, int rows);
void terminal_scroll_up(TerminalState *terminal, int top, int bottom,
                        int count);
void terminal_scroll_down(TerminalState *terminal, int top, int bottom,
                          int count);
void terminal_clamp_cursor(TerminalState *terminal);
void terminal_erase_range(TerminalState *terminal, int start, int end);
void terminal_insert_blank_chars(TerminalState *terminal, int count);
void terminal_delete_chars(TerminalState *terminal, int count);
void terminal_erase_chars(TerminalState *terminal, int count);
int terminal_next_tab_stop(const TerminalState *terminal);

#endif
