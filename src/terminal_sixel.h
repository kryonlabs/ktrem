#ifndef KAPSULE_TERMINAL_SIXEL_H
#define KAPSULE_TERMINAL_SIXEL_H

#include "terminal.h"

void terminal_sixel_clear_images(TerminalState *terminal,
                                 int alternate_filter);
void terminal_sixel_prune_scrollback(TerminalState *terminal);
void terminal_sixel_clear_range(TerminalState *terminal, int start, int end);
void terminal_sixel_shift(TerminalState *terminal, int top, int bottom,
                          int delta);
int terminal_sixel_finish(TerminalState *terminal, const char *payload);

#endif
