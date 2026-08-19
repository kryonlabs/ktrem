#include "terminal.h"

#include <stdio.h>
#include <string.h>

static int line_has(Terminal *terminal, const char *needle)
{
    char line[256];
    int row;

    for(row = 0; row < terminal->rows; row++) {
        terminal_line(terminal, row, line, sizeof(line));
        if(strstr(line, needle) != NULL)
            return 1;
    }
    return 0;
}

static int line_equals(Terminal *terminal, int row, const char *expected)
{
    char line[256];

    terminal_line(terminal, row, line, sizeof(line));
    if(strcmp(line, expected) != 0) {
        fprintf(stderr, "row %d: expected '%s', got '%s'\n", row, expected,
                line);
        return 0;
    }
    return 1;
}

int main(void)
{
    Terminal terminal;
    const Cell *cell;
    const char *styled = "hello\r\n\x1b[31mred\x1b[0m\r\n";
    const char *cleared = "\x1b[2Jclear";
    const char *delete_case = "\r\x1b[2Kabcd\x1b[2D\x1b[PZ";
    const char *bracketed = "\x1b[?2004h";
    const char *alternate = "\x1b[?1049hALT\x1b[?1049l";
    const char *scroll = "one\r\ntwo\r\nthree\r\nfour\r\nfive\r\n";

    terminal_init(&terminal);
    terminal_resize(&terminal, 40, 8);
    terminal_feed(&terminal, styled, (int)strlen(styled));
    if(!line_has(&terminal, "hello") || !line_has(&terminal, "red")) {
        fprintf(stderr, "basic feed failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 0, 1);
    if(cell == NULL || cell->fg != 1) {
        fprintf(stderr, "sgr color failed\n");
        return 1;
    }
    terminal_feed(&terminal, cleared, (int)strlen(cleared));
    if(!line_has(&terminal, "clear")) {
        fprintf(stderr, "clear screen failed\n");
        return 1;
    }
    terminal_feed(&terminal, delete_case, (int)strlen(delete_case));
    if(!line_equals(&terminal, 0, "abZ")) {
        fprintf(stderr, "delete char failed\n");
        return 1;
    }
    terminal_feed(&terminal, bracketed, (int)strlen(bracketed));
    if(!terminal.bracketed_paste) {
        fprintf(stderr, "bracketed paste mode failed\n");
        return 1;
    }
    terminal_feed(&terminal, alternate, (int)strlen(alternate));
    if(terminal.alternate_screen || !line_has(&terminal, "abZ")) {
        fprintf(stderr, "alternate screen failed\n");
        return 1;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, scroll, (int)strlen(scroll));
    if(terminal_scrollback_rows(&terminal) <= 0) {
        fprintf(stderr, "scrollback failed\n");
        return 1;
    }
    terminal_close(&terminal);
    printf("ok terminal\n");
    return 0;
}
