#ifndef INPUT_H
#define INPUT_H

#include "terminal.h"

typedef int (*InputWriteFilter)(void *userdata, const char *text, int length);
typedef int (*InputKeyFilter)(void *userdata, int platform_key, int mods);

int input_mods(void);
int input_send_control_key(TerminalState *terminal, int key, int mods);
void input_send_keyboard(TerminalState *terminal);
void input_send_keyboard_filtered(TerminalState *terminal,
                                  InputKeyFilter key_filter,
                                  InputWriteFilter write_filter,
                                  void *userdata);

#endif
