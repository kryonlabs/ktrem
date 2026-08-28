#ifndef KTREM_TERMINAL_TEXT_H
#define KTREM_TERMINAL_TEXT_H

#include "terminal.h"

enum {
    TERMINAL_CHARSET_US_ASCII,
    TERMINAL_CHARSET_DEC_SPECIAL
};

int terminal_codepoint_width(unsigned int codepoint);
int terminal_codepoint_width_for(const TerminalState *terminal,
                                 unsigned int codepoint);
unsigned int terminal_translate_charset(const TerminalState *terminal,
                                        unsigned int codepoint);
int terminal_charset_from_designator(unsigned int codepoint);
int terminal_append_utf8(char *out, int out_size, int *used,
                         unsigned int codepoint);

#endif
