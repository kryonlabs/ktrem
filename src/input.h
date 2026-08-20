#ifndef INPUT_H
#define INPUT_H

#include "terminal.h"

int input_mods(void);
int input_send_control_key(TerminalState *terminal, int key, int mods);
void input_send_keyboard(TerminalState *terminal);

#endif
