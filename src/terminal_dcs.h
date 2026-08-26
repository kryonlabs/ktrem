#ifndef KTREM_TERMINAL_DCS_H
#define KTREM_TERMINAL_DCS_H

#include "terminal.h"

void terminal_dcs_begin(TerminalState *terminal);
void terminal_dcs_append(TerminalState *terminal, unsigned int codepoint);
int terminal_dcs_finish(TerminalState *terminal);
int terminal_dcs_finish_decrqss(TerminalState *terminal,
                                const char *payload);
int terminal_dcs_finish_xtgettcap(TerminalState *terminal,
                                  const char *payload);

#endif
