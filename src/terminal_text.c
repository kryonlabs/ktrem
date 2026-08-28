#include "terminal_text.h"

static int terminal_codepoint_ambiguous_width(unsigned int cp)
{
    if((cp >= 0x00a1 && cp <= 0x00a1) || (cp >= 0x00a4 && cp <= 0x00a4) ||
       (cp >= 0x00a7 && cp <= 0x00a8) || (cp >= 0x00aa && cp <= 0x00aa) ||
       (cp >= 0x00ad && cp <= 0x00ae) || (cp >= 0x00b0 && cp <= 0x00b4) ||
       (cp >= 0x00b6 && cp <= 0x00ba) || (cp >= 0x00bc && cp <= 0x00bf) ||
       (cp >= 0x00c6 && cp <= 0x00c6) || (cp >= 0x00d0 && cp <= 0x00d0) ||
       (cp >= 0x00d7 && cp <= 0x00d8) || (cp >= 0x00de && cp <= 0x00e1) ||
       (cp >= 0x00e6 && cp <= 0x00e6) || (cp >= 0x00e8 && cp <= 0x00ea) ||
       (cp >= 0x00ec && cp <= 0x00ed) || (cp >= 0x00f0 && cp <= 0x00f0) ||
       (cp >= 0x00f2 && cp <= 0x00f3) || (cp >= 0x00f7 && cp <= 0x00fa) ||
       (cp >= 0x00fc && cp <= 0x00fc) || (cp >= 0x00fe && cp <= 0x00fe) ||
       (cp >= 0x0101 && cp <= 0x0101) || (cp >= 0x0111 && cp <= 0x0111) ||
       (cp >= 0x0113 && cp <= 0x0113) || (cp >= 0x011b && cp <= 0x011b) ||
       (cp >= 0x0126 && cp <= 0x0127) || (cp >= 0x012b && cp <= 0x012b) ||
       (cp >= 0x0131 && cp <= 0x0133) || (cp >= 0x0138 && cp <= 0x0138) ||
       (cp >= 0x013f && cp <= 0x0142) || (cp >= 0x0144 && cp <= 0x0144) ||
       (cp >= 0x0148 && cp <= 0x014b) || (cp >= 0x014d && cp <= 0x014d) ||
       (cp >= 0x0152 && cp <= 0x0153) || (cp >= 0x0166 && cp <= 0x0167) ||
       (cp >= 0x016b && cp <= 0x016b) || (cp >= 0x01ce && cp <= 0x01ce) ||
       (cp >= 0x01d0 && cp <= 0x01d0) || (cp >= 0x01d2 && cp <= 0x01d2) ||
       (cp >= 0x01d4 && cp <= 0x01d4) || (cp >= 0x01d6 && cp <= 0x01d6) ||
       (cp >= 0x01d8 && cp <= 0x01d8) || (cp >= 0x01da && cp <= 0x01da) ||
       (cp >= 0x01dc && cp <= 0x01dc) || (cp >= 0x0251 && cp <= 0x0251) ||
       (cp >= 0x0261 && cp <= 0x0261) || (cp >= 0x02c4 && cp <= 0x02c4) ||
       (cp >= 0x02c7 && cp <= 0x02c7) || (cp >= 0x02c9 && cp <= 0x02cb) ||
       (cp >= 0x02cd && cp <= 0x02cd) || (cp >= 0x02d0 && cp <= 0x02d0) ||
       (cp >= 0x02d8 && cp <= 0x02db) || (cp >= 0x02dd && cp <= 0x02dd) ||
       (cp >= 0x02df && cp <= 0x02df) || (cp >= 0x0391 && cp <= 0x03a1) ||
       (cp >= 0x03a3 && cp <= 0x03a9) || (cp >= 0x03b1 && cp <= 0x03c1) ||
       (cp >= 0x03c3 && cp <= 0x03c9) || (cp >= 0x0401 && cp <= 0x0401) ||
       (cp >= 0x0410 && cp <= 0x044f) || (cp >= 0x0451 && cp <= 0x0451) ||
       (cp >= 0x2010 && cp <= 0x2010) || (cp >= 0x2013 && cp <= 0x2016) ||
       (cp >= 0x2018 && cp <= 0x2019) || (cp >= 0x201c && cp <= 0x201d) ||
       (cp >= 0x2020 && cp <= 0x2022) || (cp >= 0x2024 && cp <= 0x2027) ||
       (cp >= 0x2030 && cp <= 0x2030) || (cp >= 0x2032 && cp <= 0x2033) ||
       (cp >= 0x2035 && cp <= 0x2035) || (cp >= 0x203b && cp <= 0x203b) ||
       (cp >= 0x203e && cp <= 0x203e) || (cp >= 0x2074 && cp <= 0x2074) ||
       (cp >= 0x207f && cp <= 0x207f) || (cp >= 0x2081 && cp <= 0x2084) ||
       (cp >= 0x20ac && cp <= 0x20ac) || (cp >= 0x2103 && cp <= 0x2103) ||
       (cp >= 0x2105 && cp <= 0x2105) || (cp >= 0x2109 && cp <= 0x2109) ||
       (cp >= 0x2113 && cp <= 0x2113) || (cp >= 0x2116 && cp <= 0x2116) ||
       (cp >= 0x2121 && cp <= 0x2122) || (cp >= 0x2126 && cp <= 0x2126) ||
       (cp >= 0x212b && cp <= 0x212b) || (cp >= 0x2153 && cp <= 0x2154) ||
       (cp >= 0x215b && cp <= 0x215e) || (cp >= 0x2160 && cp <= 0x216b) ||
       (cp >= 0x2170 && cp <= 0x2179) || (cp >= 0x2189 && cp <= 0x2189) ||
       (cp >= 0x2190 && cp <= 0x2199) || (cp >= 0x21b8 && cp <= 0x21b9) ||
       (cp >= 0x21d2 && cp <= 0x21d2) || (cp >= 0x21d4 && cp <= 0x21d4) ||
       (cp >= 0x21e7 && cp <= 0x21e7) || (cp >= 0x2200 && cp <= 0x2200) ||
       (cp >= 0x2202 && cp <= 0x2203) || (cp >= 0x2207 && cp <= 0x2208) ||
       (cp >= 0x220b && cp <= 0x220b) || (cp >= 0x220f && cp <= 0x220f) ||
       (cp >= 0x2211 && cp <= 0x2211) || (cp >= 0x2215 && cp <= 0x2215) ||
       (cp >= 0x221a && cp <= 0x221a) || (cp >= 0x221d && cp <= 0x2220) ||
       (cp >= 0x2223 && cp <= 0x2223) || (cp >= 0x2225 && cp <= 0x2225) ||
       (cp >= 0x2227 && cp <= 0x222c) || (cp >= 0x222e && cp <= 0x222e) ||
       (cp >= 0x2234 && cp <= 0x2237) || (cp >= 0x223c && cp <= 0x223d) ||
       (cp >= 0x2248 && cp <= 0x2248) || (cp >= 0x224c && cp <= 0x224c) ||
       (cp >= 0x2252 && cp <= 0x2252) || (cp >= 0x2260 && cp <= 0x2261) ||
       (cp >= 0x2264 && cp <= 0x2267) || (cp >= 0x226a && cp <= 0x226b) ||
       (cp >= 0x226e && cp <= 0x226f) || (cp >= 0x2282 && cp <= 0x2283) ||
       (cp >= 0x2286 && cp <= 0x2287) || (cp >= 0x2295 && cp <= 0x2295) ||
       (cp >= 0x2299 && cp <= 0x2299) || (cp >= 0x22a5 && cp <= 0x22a5) ||
       (cp >= 0x22bf && cp <= 0x22bf) || (cp >= 0x2312 && cp <= 0x2312) ||
       (cp >= 0x2460 && cp <= 0x24e9) || (cp >= 0x24eb && cp <= 0x254b) ||
       (cp >= 0x2550 && cp <= 0x2573) || (cp >= 0x2580 && cp <= 0x258f) ||
       (cp >= 0x2592 && cp <= 0x2595) || (cp >= 0x25a0 && cp <= 0x25a1) ||
       (cp >= 0x25a3 && cp <= 0x25a9) || (cp >= 0x25b2 && cp <= 0x25b3) ||
       (cp >= 0x25b6 && cp <= 0x25b7) || (cp >= 0x25bc && cp <= 0x25bd) ||
       (cp >= 0x25c0 && cp <= 0x25c1) || (cp >= 0x25c6 && cp <= 0x25c8) ||
       (cp >= 0x25cb && cp <= 0x25cb) || (cp >= 0x25ce && cp <= 0x25d1) ||
       (cp >= 0x25e2 && cp <= 0x25e5) || (cp >= 0x25ef && cp <= 0x25ef) ||
       (cp >= 0x2605 && cp <= 0x2606) || (cp >= 0x2609 && cp <= 0x2609) ||
       (cp >= 0x260e && cp <= 0x260f) || (cp >= 0x261c && cp <= 0x261c) ||
       (cp >= 0x261e && cp <= 0x261e) || (cp >= 0x2640 && cp <= 0x2640) ||
       (cp >= 0x2642 && cp <= 0x2642) || (cp >= 0x2660 && cp <= 0x2661) ||
       (cp >= 0x2663 && cp <= 0x2665) || (cp >= 0x2667 && cp <= 0x266a) ||
       (cp >= 0x266c && cp <= 0x266d) || (cp >= 0x266f && cp <= 0x266f) ||
       (cp >= 0x269e && cp <= 0x269f) || (cp >= 0x26bf && cp <= 0x26bf) ||
       (cp >= 0x26c6 && cp <= 0x26cd) || (cp >= 0x26cf && cp <= 0x26d3) ||
       (cp >= 0x26d5 && cp <= 0x26e1) || (cp >= 0x26e3 && cp <= 0x26e3) ||
       (cp >= 0x26e8 && cp <= 0x26e9) || (cp >= 0x26eb && cp <= 0x26f1) ||
       (cp >= 0x26f4 && cp <= 0x26f4) || (cp >= 0x26f6 && cp <= 0x26f9) ||
       (cp >= 0x26fb && cp <= 0x26fc) || (cp >= 0x26fe && cp <= 0x26ff) ||
       (cp >= 0x273d && cp <= 0x273d) || (cp >= 0x2776 && cp <= 0x277f) ||
       (cp >= 0x2b56 && cp <= 0x2b59) || (cp >= 0x3248 && cp <= 0x324f) ||
       (cp >= 0xe000 && cp <= 0xf8ff) || (cp >= 0xfffd && cp <= 0xfffd))
        return 1;
    return 0;
}

int terminal_codepoint_width(unsigned int cp)
{
    return terminal_codepoint_width_for(NULL, cp);
}

int terminal_codepoint_width_for(const TerminalState *terminal, unsigned int cp)
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
    if(terminal != NULL && terminal->ambiguous_width_wide &&
       terminal_codepoint_ambiguous_width(cp))
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
