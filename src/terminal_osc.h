#ifndef KTREM_TERMINAL_OSC_H
#define KTREM_TERMINAL_OSC_H

#include "terminal.h"

int terminal_default_palette_color(int index);
int terminal_send_title_report(TerminalState *terminal, int icon);
void terminal_push_title_targets(TerminalState *terminal, int target);
void terminal_pop_title_targets(TerminalState *terminal, int target);
void terminal_finish_osc(TerminalState *terminal);

#endif
