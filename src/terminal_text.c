#include "terminal_text.h"

int terminal_codepoint_width(unsigned int cp)
{
    if(cp == 0)
        return 0;
    if(cp < 32 || (cp >= 0x7f && cp < 0xa0))
        return 0;
    if((cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
       (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
       (cp >= 0xfe20 && cp <= 0xfe2f))
        return 0;
    if((cp >= 0x1100 && cp <= 0x115f) || cp == 0x2329 || cp == 0x232a ||
       (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
       (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
       (cp >= 0xfe10 && cp <= 0xfe19) || (cp >= 0xfe30 && cp <= 0xfe6f) ||
       (cp >= 0xff00 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
       (cp >= 0x1f300 && cp <= 0x1f64f) ||
       (cp >= 0x1f900 && cp <= 0x1f9ff))
        return 2;
    return 1;
}

unsigned int terminal_translate_charset(const TerminalState *terminal,
                                        unsigned int codepoint)
{
    int charset;

    if(terminal == NULL || codepoint < 0x20 || codepoint > 0x7e)
        return codepoint;
    charset = terminal->active_charset == 1 ? terminal->g1_charset
                                            : terminal->g0_charset;
    if(charset != TERMINAL_CHARSET_DEC_SPECIAL)
        return codepoint;
    switch(codepoint) {
    case '`':
        return 0x25c6;
    case 'a':
        return 0x2592;
    case 'b':
        return 0x2409;
    case 'c':
        return 0x240c;
    case 'd':
        return 0x240d;
    case 'e':
        return 0x240a;
    case 'f':
        return 0x00b0;
    case 'g':
        return 0x00b1;
    case 'h':
        return 0x2424;
    case 'i':
        return 0x240b;
    case 'j':
        return 0x2518;
    case 'k':
        return 0x2510;
    case 'l':
        return 0x250c;
    case 'm':
        return 0x2514;
    case 'n':
        return 0x253c;
    case 'o':
        return 0x23ba;
    case 'p':
        return 0x23bb;
    case 'q':
        return 0x2500;
    case 'r':
        return 0x23bc;
    case 's':
        return 0x23bd;
    case 't':
        return 0x251c;
    case 'u':
        return 0x2524;
    case 'v':
        return 0x2534;
    case 'w':
        return 0x252c;
    case 'x':
        return 0x2502;
    case 'y':
        return 0x2264;
    case 'z':
        return 0x2265;
    case '{':
        return 0x03c0;
    case '|':
        return 0x2260;
    case '}':
        return 0x00a3;
    case '~':
        return 0x00b7;
    default:
        return codepoint;
    }
}

int terminal_charset_from_designator(unsigned int codepoint)
{
    return codepoint == '0' ? TERMINAL_CHARSET_DEC_SPECIAL
                            : TERMINAL_CHARSET_US_ASCII;
}

int terminal_append_utf8(char *out, int out_size, int *used, unsigned int cp)
{
    return AppendTerminalPaneUTF8Codepoint(out, out_size, used, cp);
}
