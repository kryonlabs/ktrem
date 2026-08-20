#ifndef KAPSULE_TERMINAL_MODES_H
#define KAPSULE_TERMINAL_MODES_H

#include "terminal.h"

void terminal_reset_style(TerminalState *terminal);
void terminal_reset_charsets(TerminalState *terminal);
void terminal_save_cursor_state(TerminalState *terminal);
void terminal_restore_cursor_state(TerminalState *terminal);
void terminal_soft_reset(TerminalState *terminal);
void terminal_reset_device(TerminalState *terminal);
void terminal_set_private_mode(TerminalState *terminal, int mode, int enabled);

#endif
