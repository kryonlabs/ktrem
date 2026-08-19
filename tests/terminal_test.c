#include "session.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int line_has(TerminalState *terminal, const char *needle)
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

static int line_equals(TerminalState *terminal, int row, const char *expected)
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

static int ctrl_c_interrupts_child(void)
{
    TerminalState terminal;
    int i;

    terminal_init(&terminal);
    if(!terminal_spawn(&terminal, NULL, "/bin/sh", "sleep 20", 24, 4)) {
        fprintf(stderr, "spawn failed\n");
        return 0;
    }
    usleep(250000);
    terminal_write_text(&terminal, "\x03");
    for(i = 0; i < 40; i++) {
        terminal_poll(&terminal);
        if(!terminal.running) {
            terminal_close(&terminal);
            return 1;
        }
        usleep(50000);
    }
    terminal_close(&terminal);
    fprintf(stderr, "ctrl-c did not interrupt child\n");
    return 0;
}

static int finish_capture(const char *name, TerminalState *terminal, int read_fd,
                          const char *expected)
{
    char got[256];
    int n;

    terminal_close(terminal);
    n = (int)read(read_fd, got, sizeof(got) - 1);
    close(read_fd);
    if(n < 0)
        n = 0;
    got[n] = '\0';
    if(strcmp(got, expected) != 0) {
        fprintf(stderr, "%s: expected", name);
        for(int i = 0; expected[i] != '\0'; i++)
            fprintf(stderr, " %02x", (unsigned char)expected[i]);
        fprintf(stderr, ", got");
        for(int i = 0; i < n; i++)
            fprintf(stderr, " %02x", (unsigned char)got[i]);
        fprintf(stderr, "\n");
        return 0;
    }
    return 1;
}

static int capture_key_sequence(int key, int mods, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!terminal_send_key(&terminal, key, mods))
        return 0;
    return finish_capture("key sequence", &terminal, fds[0], expected);
}

static int capture_codepoint_sequence(unsigned int codepoint, int mods,
                                      const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "codepoint sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!terminal_send_codepoint(&terminal, codepoint, mods))
        return 0;
    return finish_capture("codepoint sequence", &terminal, fds[0], expected);
}

static int capture_application_cursor_sequence(void)
{
    TerminalState terminal;
    int fds[2];
    char got[32];
    int n;

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "application cursor: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.application_cursor_keys = 1;
    if(!terminal_send_key(&terminal, KEY_UP_CODE, 0))
        return 0;
    if(!terminal_send_key(&terminal, KEY_HOME_CODE, 0))
        return 0;
    if(!terminal_send_key(&terminal, KEY_END_CODE, 0))
        return 0;
    terminal_close(&terminal);
    n = (int)read(fds[0], got, sizeof(got) - 1);
    close(fds[0]);
    if(n < 0)
        n = 0;
    got[n] = '\0';
    if(strcmp(got, "\x1bOA\x1bOH\x1bOF") != 0) {
        fprintf(stderr, "application cursor: expected");
        for(int i = 0; "\x1bOA\x1bOH\x1bOF"[i] != '\0'; i++)
            fprintf(stderr, " %02x",
                    (unsigned char)"\x1bOA\x1bOH\x1bOF"[i]);
        fprintf(stderr, ", got");
        for(int i = 0; i < n; i++)
            fprintf(stderr, " %02x", (unsigned char)got[i]);
        fprintf(stderr, "\n");
        return 0;
    }
    return 1;
}

static int capture_keypad_sequence(int app_mode, char key, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "keypad sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.application_keypad = app_mode;
    if(!terminal_send_keypad(&terminal, key))
        return 0;
    return finish_capture("keypad sequence", &terminal, fds[0], expected);
}

static int capture_bracketed_paste(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "bracketed paste: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.bracketed_paste = 1;
    if(!terminal_send_paste(&terminal, "paste\ntext"))
        return 0;
    return finish_capture("bracketed paste", &terminal, fds[0],
                          "\x1b[200~paste\ntext\x1b[201~");
}

static int capture_mouse_sequence(int sgr, int button, int pressed, int motion,
                                  int mods, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "mouse sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.mouse_mode = motion ? 1002 : 1000;
    terminal.mouse_sgr = sgr;
    if(!terminal_send_mouse(&terminal, button, 4, 2, pressed, motion, mods))
        return 0;
    return finish_capture("mouse sequence", &terminal, fds[0], expected);
}

static int capture_mouse_mode_sequence(const char *mode_sequence, int button,
                                       int pressed, int motion, int mods,
                                       const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "mouse mode sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, mode_sequence, (int)strlen(mode_sequence));
    if(!terminal_send_mouse(&terminal, button, 4, 2, pressed, motion, mods))
        return 0;
    return finish_capture("mouse mode sequence", &terminal, fds[0], expected);
}

static int capture_utf8_mouse_sequence(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "utf8 mouse sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, "\x1b[?1000h\x1b[?1005h",
                  (int)strlen("\x1b[?1000h\x1b[?1005h"));
    if(!terminal_send_mouse(&terminal, TERMINAL_MOUSE_LEFT, 300, 2, 1, 0, 0))
        return 0;
    return finish_capture("utf8 mouse sequence", &terminal, fds[0],
                          "\x1b[M \xc5\x8d#");
}

static int capture_focus_sequence(int focused, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "focus sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.focus_reporting = 1;
    if(!terminal_send_focus(&terminal, focused))
        return 0;
    return finish_capture("focus sequence", &terminal, fds[0], expected);
}

static int capture_osc_color_query(void)
{
    TerminalState terminal;
    int fds[2];
    const char *input = "\x1b]10;#112233\a\x1b]10;?\a"
                        "\x1b]4;2;#445566\a\x1b]4;2;?\a"
                        "\x1b]4;1;?\a"
                        "\x1b]104;2\a\x1b]4;2;?\a"
                        "\x1b]110\a\x1b]10;?\a"
                        "\x1b]111\a\x1b]11;?\a"
                        "\x1b]112\a\x1b]12;?\a";

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "osc color query: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.base_fg = COLOR_TRUE_RGB | 0xaabbcc;
    terminal.base_bg = COLOR_TRUE_RGB | 0x010203;
    terminal.base_cursor_color = COLOR_TRUE_RGB | 0x334455;
    terminal_feed(&terminal, input, (int)strlen(input));
    return finish_capture("osc color query", &terminal, fds[0],
                          "\x1b]10;rgb:1111/2222/3333\a"
                          "\x1b]4;2;rgb:4444/5555/6666\a"
                          "\x1b]4;1;rgb:8080/0000/0000\a"
                          "\x1b]4;2;rgb:0000/8080/0000\a"
                          "\x1b]10;rgb:aaaa/bbbb/cccc\a"
                          "\x1b]11;rgb:0101/0202/0303\a"
                          "\x1b]12;rgb:3333/4444/5555\a");
}

static int c1_controls_parse_csi_and_osc(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x9b" "31mR\x9b" "0mN",
                  (int)strlen("\x9b" "31mR\x9b" "0mN"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 'R' || cell->fg != 1) {
        fprintf(stderr, "8-bit csi sgr failed\n");
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 1, 0);
    if(cell == NULL || cell->codepoint != 'N' ||
       cell->fg != COLOR_DEFAULT) {
        fprintf(stderr, "8-bit csi reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x9d" "2;C1 Title\a",
                  (int)strlen("\x9d" "2;C1 Title\a"));
    if(strcmp(terminal.title, "C1 Title") != 0) {
        fprintf(stderr, "8-bit osc bel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x9d" "10;#112233\x9c",
                  (int)strlen("\x9d" "10;#112233\x9c"));
    if(terminal.default_fg != (COLOR_TRUE_RGB | 0x112233)) {
        fprintf(stderr, "8-bit osc st failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int capture_response_sequence(const char *name, const char *input,
                                     const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "%s: pipe failed\n", name);
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, input, (int)strlen(input));
    return finish_capture(name, &terminal, fds[0], expected);
}

static int resize_reflows_wrapped_text(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, "abcdefghijklmnopq", 17);
    terminal_resize(&terminal, 12, 5);
    if(!line_equals(&terminal, 0, "abcdefghijkl")) {
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 1, "mnopq")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int dec_autowrap_mode_controls_right_margin(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "abcdefghI", 9);
    if(!line_equals(&terminal, 0, "abcdefgh") ||
       !line_equals(&terminal, 1, "I")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "\x1b[?7labcdefghI", 14);
    if(terminal.autowrap || !line_equals(&terminal, 0, "abcdefgI") ||
       !line_equals(&terminal, 1, "")) {
        fprintf(stderr, "autowrap disable failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?7h", 5);
    if(!terminal.autowrap) {
        fprintf(stderr, "autowrap re-enable failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\nabcdefghF", 11);
    if(!line_equals(&terminal, 1, "abcdefgh") ||
       !line_equals(&terminal, 2, "F")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b" "c", 2);
    if(!terminal.autowrap) {
        fprintf(stderr, "autowrap reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int resize_reflows_scrollback(void)
{
    TerminalState terminal;
    const char *text = "abcdefghijkl\r\none\r\ntwo\r\nthree\r\nfour\r\nfive\r\n";
    char line[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 6, 3);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal_scrollback_rows(&terminal) < 3) {
        fprintf(stderr, "scrollback setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_resize(&terminal, 12, 3);
    terminal_scrollback_line(&terminal, 0, line, sizeof(line));
    if(strcmp(line, "abcdefghijkl") != 0) {
        fprintf(stderr, "reflowed scrollback: expected 'abcdefghijkl', got '%s'\n",
                line);
        terminal_close(&terminal);
        return 0;
    }
    terminal_scrollback_line(&terminal, 1, line, sizeof(line));
    if(strcmp(line, "one") != 0) {
        fprintf(stderr, "reflowed scrollback: expected 'one', got '%s'\n",
                line);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int origin_mode_uses_scroll_region(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 5);
    terminal_feed(&terminal, "\x1b[2;4r\x1b[?6h\x1b[Htop",
                  (int)strlen("\x1b[2;4r\x1b[?6h\x1b[Htop"));
    if(!line_equals(&terminal, 1, "top")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[99;1Hbottom",
                  (int)strlen("\x1b[99;1Hbottom"));
    if(!line_equals(&terminal, 3, "bottom")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?6l\x1b[Hhome",
                  (int)strlen("\x1b[?6l\x1b[Hhome"));
    if(!line_equals(&terminal, 0, "home")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int erase_saved_lines_clears_scrollback_only(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 3);
    terminal_feed(&terminal, "one\r\ntwo\r\nthree\r\nfour\r\nfive",
                  (int)strlen("one\r\ntwo\r\nthree\r\nfour\r\nfive"));
    if(terminal_scrollback_rows(&terminal) <= 0 || !line_has(&terminal, "five")) {
        fprintf(stderr, "erase saved lines setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[3J", 4);
    if(terminal_scrollback_rows(&terminal) != 0 || !line_has(&terminal, "five")) {
        fprintf(stderr, "erase saved lines failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int insert_mode_inserts_printable_text(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "abcd\r\x1b[3G\x1b[4hXY\x1b[4lZ",
                  (int)strlen("abcd\r\x1b[3G\x1b[4hXY\x1b[4lZ"));
    if(!line_equals(&terminal, 0, "abXYZd")) {
        terminal_close(&terminal);
        return 0;
    }
    if(terminal.insert_mode) {
        fprintf(stderr, "insert mode reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int wide_characters_use_two_columns(void)
{
    TerminalState terminal;
    const Cell *cell;
    const char *text = "ab\xe4\xbd\xa0""cd";

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal.cursor_col != 6) {
        fprintf(stderr, "wide cursor width failed: %d\n", terminal.cursor_col);
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 3, 0);
    if(cell == NULL || (cell->style & STYLE_WIDE_CONT) == 0) {
        fprintf(stderr, "wide continuation failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 0, "ab\xe4\xbd\xa0""cd")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, "1234567\xe4\xbd\xa0", 10);
    if(!line_equals(&terminal, 0, "1234567") ||
       !line_equals(&terminal, 1, "\xe4\xbd\xa0")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int combining_marks_stay_on_base_cell(void)
{
    TerminalState terminal;
    const Cell *cell;
    const char *text = "e\xcc\x81x";

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal.cursor_col != 2) {
        fprintf(stderr, "combining cursor width failed: %d\n",
                terminal.cursor_col);
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 'e' || cell->combining != 0x0301) {
        fprintf(stderr, "combining mark attach failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 0, text)) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int dec_special_graphics_charset_maps_line_drawing(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b(0lqkx\x1b(Bx",
                  (int)strlen("\x1b(0lqkx\x1b(Bx"));
    if(!line_equals(&terminal, 0, "┌─┐│x")) {
        fprintf(stderr, "dec special graphics g0 failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b)0\x0e""mqj\x0f""mqj",
                  (int)strlen("\x1b)0\x0e""mqj\x0f""mqj"));
    if(!line_equals(&terminal, 0, "└─┘mqj")) {
        fprintf(stderr, "dec special graphics g1 failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b(0l\x1b" "cl",
                  (int)strlen("\x1b(0l\x1b" "cl"));
    if(!line_equals(&terminal, 0, "l")) {
        fprintf(stderr, "dec special graphics reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int save_restore_preserves_rendition_and_charset(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal,
                  "\x1b[31m\x1b(0\x1b" "7\x1b[32m\x1b(BX\x1b" "8l",
                  (int)strlen(
                      "\x1b[31m\x1b(0\x1b" "7\x1b[32m\x1b(BX\x1b" "8l"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x250c || cell->fg != 1) {
        fprintf(stderr, "escape save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[34m\x1b(0\x1b[s\x1b[35m\x1b(BX\x1b[ul",
                  (int)strlen(
                      "\x1b[34m\x1b(0\x1b[s\x1b[35m\x1b(BX\x1b[ul"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x250c || cell->fg != 4) {
        fprintf(stderr, "csi save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[33m\x1b(0\x1b[?1048h\x1b[35m\x1b(B"
                             "\x1b[?1048lq",
                  (int)strlen("\x1b[33m\x1b(0\x1b[?1048h\x1b[35m\x1b(B"
                              "\x1b[?1048lq"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x2500 || cell->fg != 3) {
        fprintf(stderr, "1048 save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int osc_current_directory_updates_session_title(void)
{
    Session session;
    char cwd[1024];

    session_init(&session);
    snprintf(session.cwd, sizeof(session.cwd), "/home/wao");
    session_sync_terminal_metadata(&session);
    terminal_feed(&session.terminal,
                  "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a",
                  (int)strlen(
                      "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a"));
    session_sync_terminal_metadata(&session);
    session_current_cwd(&session, cwd, sizeof(cwd));
    if(strcmp(cwd, "/home/wao/Projects/Kapsule Test") != 0 ||
       strcmp(session_title(&session), "Kapsule Test") != 0) {
        fprintf(stderr, "osc current directory sync failed\n");
        session_close(&session);
        return 0;
    }
    terminal_feed(&session.terminal, "\x1b]2;Editor\a",
                  (int)strlen("\x1b]2;Editor\a"));
    session_sync_terminal_metadata(&session);
    if(strcmp(session_title(&session), "Editor") != 0) {
        fprintf(stderr, "osc title sync failed\n");
        session_close(&session);
        return 0;
    }
    session_set_title(&session, "manual");
    terminal_feed(&session.terminal,
                  "\x1b]2;Ignored\a\x1b]7;file://omega/tmp/project\a",
                  (int)strlen(
                      "\x1b]2;Ignored\a\x1b]7;file://omega/tmp/project\a"));
    session_sync_terminal_metadata(&session);
    if(strcmp(session_title(&session), "manual") != 0) {
        fprintf(stderr, "manual tab title override failed\n");
        session_close(&session);
        return 0;
    }
    session_close(&session);
    return 1;
}

static int alternate_screen_modes_preserve_expected_state(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[2;5H\x1b[?1048h\x1b[4;7H\x1b[?1048lX",
                  (int)strlen("\x1b[2;5H\x1b[?1048h\x1b[4;7H\x1b[?1048lX"));
    if(terminal.alternate_screen || terminal.cursor_row != 1 ||
       terminal.cursor_col != 5 || !line_equals(&terminal, 1, "    X")) {
        fprintf(stderr, "1048 cursor save/restore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[?47hALT\x1b[?47lMAIN\x1b[?47h",
                  (int)strlen("\x1b[?47hALT\x1b[?47lMAIN\x1b[?47h"));
    if(!terminal.alternate_screen || !line_has(&terminal, "ALT")) {
        fprintf(stderr, "47 alternate buffer preservation failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?47l", 6);
    if(terminal.alternate_screen || !line_has(&terminal, "MAIN")) {
        fprintf(stderr, "47 return to main failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "MAIN\x1b[2;6H\x1b[?1049hALT\x1b[?1049lZ",
                  (int)strlen("MAIN\x1b[2;6H\x1b[?1049hALT\x1b[?1049lZ"));
    if(terminal.alternate_screen || terminal.cursor_row != 1 ||
       terminal.cursor_col != 6 || !line_has(&terminal, "MAIN") ||
       !line_equals(&terminal, 1, "     Z")) {
        fprintf(stderr, "1049 alternate screen restore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?1049h", 8);
    if(!terminal.alternate_screen || line_has(&terminal, "ALT")) {
        fprintf(stderr, "1049 alternate clear failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

int main(void)
{
    TerminalState terminal;
    const Cell *cell;
    const char *styled = "hello\r\n\x1b[31mred\x1b[0m\r\n";
    const char *cleared = "\x1b[2Jclear";
    const char *delete_case = "\r\x1b[2Kabcd\x1b[2D\x1b[PZ";
    const char *bracketed = "\x1b[?2004h";
    const char *osc = "\x1b]2;Kapsule\a\x1b]10;#112233\a"
                      "\x1b]11;rgb:4455/6677/8899\x1b\\"
                      "\x1b]12;rgb:aa/bb/cc\a"
                      "\x1b]4;1;#010203;2;rgb:1020/3040/5060\a"
                      "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a"
                      "\x1b]52;c;aGVsbG8=\a";
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
    terminal_feed(&terminal,
                  "\x1b[38:2::17:34:51mF\x1b[48:2::68:85:102mB\x1b[0m",
                  (int)strlen(
                      "\x1b[38:2::17:34:51mF\x1b[48:2::68:85:102mB\x1b[0m"));
    cell = terminal_cell(&terminal, 0, 2);
    if(cell == NULL || cell->fg != (COLOR_TRUE_RGB | 0x112233)) {
        fprintf(stderr, "sgr colon foreground failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 1, 2);
    if(cell == NULL || cell->bg != (COLOR_TRUE_RGB | 0x445566)) {
        fprintf(stderr, "sgr colon background failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[4;58;5;12mI\x1b[58:2::1:2:3mT\x1b[59mR",
                  (int)strlen(
                      "\x1b[4;58;5;12mI\x1b[58:2::1:2:3mT\x1b[59mR"));
    cell = terminal_cell(&terminal, 2, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != 12) {
        fprintf(stderr, "sgr indexed underline color failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 3, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != (COLOR_TRUE_RGB | 0x010203)) {
        fprintf(stderr, "sgr truecolor underline color failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 4, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != COLOR_DEFAULT) {
        fprintf(stderr, "sgr underline color reset failed\n");
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
    terminal_feed(&terminal, osc, (int)strlen(osc));
    if(strcmp(terminal.title, "Kapsule") != 0 ||
       terminal.default_fg != (COLOR_TRUE_RGB | 0x112233) ||
       terminal.default_bg != (COLOR_TRUE_RGB | 0x446688) ||
       terminal.cursor_color != (COLOR_TRUE_RGB | 0xaabbcc) ||
       terminal.palette_overrides[1] != (COLOR_TRUE_RGB | 0x010203) ||
       terminal.palette_overrides[2] != (COLOR_TRUE_RGB | 0x103050) ||
       strcmp(terminal.current_directory,
              "/home/wao/Projects/Kapsule Test") != 0 ||
       !terminal.clipboard_pending ||
       strcmp(terminal.clipboard, "hello") != 0) {
        fprintf(stderr, "osc title/color failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]104;1\a", 8);
    if(terminal.palette_overrides[1] != COLOR_DEFAULT ||
       terminal.palette_overrides[2] != (COLOR_TRUE_RGB | 0x103050)) {
        fprintf(stderr, "osc palette index reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]104\a", 6);
    if(terminal.palette_overrides[2] != COLOR_DEFAULT) {
        fprintf(stderr, "osc palette reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]110\a\x1b]111\a\x1b]112\a", 18);
    if(terminal.default_fg != COLOR_DEFAULT ||
       terminal.default_bg != COLOR_DEFAULT ||
       terminal.cursor_color != COLOR_DEFAULT) {
        fprintf(stderr, "osc default color reset failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[3;1H\x1b]8;id=docs;https://example.test\aDocs"
                  "\x1b]8;;\a plain",
                  (int)strlen("\x1b[3;1H\x1b]8;id=docs;https://example.test\aDocs"
                              "\x1b]8;;\a plain"));
    cell = terminal_cell(&terminal, 0, 2);
    if(cell == NULL || cell->hyperlink <= 0 ||
       strcmp(terminal_hyperlink(&terminal, cell->hyperlink),
              "https://example.test") != 0) {
        fprintf(stderr, "osc hyperlink attach failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 5, 2);
    if(cell == NULL || cell->hyperlink != 0) {
        fprintf(stderr, "osc hyperlink close failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[?1004h", 8);
    if(!terminal.focus_reporting) {
        fprintf(stderr, "focus reporting enable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[?1004l", 8);
    if(terminal.focus_reporting) {
        fprintf(stderr, "focus reporting disable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[3 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_UNDERLINE) {
        fprintf(stderr, "underline cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[5 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BAR) {
        fprintf(stderr, "bar cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[2 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BLOCK) {
        fprintf(stderr, "block cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[0 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_DEFAULT) {
        fprintf(stderr, "default cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b=", 2);
    if(!terminal.application_keypad) {
        fprintf(stderr, "application keypad enable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b>", 2);
    if(terminal.application_keypad) {
        fprintf(stderr, "application keypad disable failed\n");
        return 1;
    }
    terminal_feed(&terminal, alternate, (int)strlen(alternate));
    if(terminal.alternate_screen || !line_has(&terminal, "abZ")) {
        fprintf(stderr, "alternate screen failed\n");
        return 1;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 20, 8);
    terminal_feed(&terminal, "\x1bPq#1;2;100;0;0!3~-$#2;2;0;100;0!2~\x1b\\",
                  (int)strlen("\x1bPq#1;2;100;0;0!3~-$#2;2;0;100;0!2~\x1b\\"));
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel image decode failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 0);

        if(image == NULL || image->width != 3 || image->height != 12 ||
           image->pixels[0] != (COLOR_TRUE_RGB | 0xff0000) ||
           image->pixels[5 * image->width + 2] !=
               (COLOR_TRUE_RGB | 0xff0000) ||
           image->pixels[6 * image->width] !=
               (COLOR_TRUE_RGB | 0x00ff00) ||
           image->pixels[6 * image->width + 2] != COLOR_DEFAULT ||
           terminal.cursor_row != 2) {
            fprintf(stderr, "sixel image pixels failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\x1bP0;2q\"1;1;4;6#3;1;120;50;100?\x1b\\",
                  (int)strlen("\x1bP0;2q\"1;1;4;6#3;1;120;50;100?\x1b\\"));
    if(terminal_sixel_count(&terminal) != 2) {
        fprintf(stderr, "sixel second image failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 1);

        if(image == NULL || image->width != 4 || image->height != 6 ||
           image->pixels[0] != (COLOR_TRUE_RGB | 0x000000)) {
            fprintf(stderr, "sixel raster/background failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\x1b[2J", 4);
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel clear failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1bPq!12\x1b\\",
                  (int)strlen("\x1bPq!12\x1b\\"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel malformed repeat failed\n");
        return 1;
    }
    {
        const unsigned char sixel_8bit[] = {0x90, 'q', '~', 0x9c};

        terminal_feed(&terminal, sixel_8bit, (int)sizeof(sixel_8bit));
    }
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel 8-bit dcs failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[1;1H\x1b[2K",
                  (int)strlen("\x1b[1;1H\x1b[2K"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel erase line failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1bPq~\x1b\\",
                  (int)strlen("\x1bPq~\x1b\\"));
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel erase display setup failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[1;1H\x1b[J",
                  (int)strlen("\x1b[1;1H\x1b[J"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel erase display failed\n");
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
    if(!capture_key_sequence(KEY_TAB_CODE, MOD_SHIFT, "\x1b[Z"))
        return 1;
    if(!capture_codepoint_sequence('a', 0, "a"))
        return 1;
    if(!capture_codepoint_sequence('a', MOD_ALT, "\x1b""a"))
        return 1;
    if(!capture_codepoint_sequence(0x00e9, MOD_ALT, "\x1b\xc3\xa9"))
        return 1;
    if(!capture_key_sequence(KEY_F1_CODE, 0, "\x1bOP"))
        return 1;
    if(!capture_key_sequence(KEY_F5_CODE, 0, "\x1b[15~"))
        return 1;
    if(!capture_key_sequence(KEY_F12_CODE, MOD_CTRL, "\x1b[24;5~"))
        return 1;
    if(!capture_key_sequence(KEY_HOME_CODE, 0, "\x1b[H"))
        return 1;
    if(!capture_key_sequence(KEY_END_CODE, 0, "\x1b[F"))
        return 1;
    if(!capture_key_sequence(KEY_HOME_CODE, MOD_CTRL, "\x1b[1;5H"))
        return 1;
    if(!capture_key_sequence(KEY_END_CODE, MOD_CTRL, "\x1b[1;5F"))
        return 1;
    if(!capture_application_cursor_sequence())
        return 1;
    if(!capture_keypad_sequence(0, '7', "7"))
        return 1;
    if(!capture_keypad_sequence(1, '7', "\x1bOw"))
        return 1;
    if(!capture_keypad_sequence(1, '\r', "\x1bOM"))
        return 1;
    if(!capture_bracketed_paste())
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_LEFT, 1, 0, 0,
                               "\x1b[<0;5;3M"))
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_LEFT, 0, 0, 0,
                               "\x1b[<0;5;3m"))
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_WHEEL_DOWN, 1, 0, MOD_CTRL,
                               "\x1b[<81;5;3M"))
        return 1;
    if(!capture_mouse_sequence(0, TERMINAL_MOUSE_LEFT, 1, 0, 0, "\x1b[M %#"))
        return 1;
    if(!capture_mouse_mode_sequence("\x1b[?9h", TERMINAL_MOUSE_LEFT, 1, 0, 0,
                                    "\x1b[M %#"))
        return 1;
    if(!capture_utf8_mouse_sequence())
        return 1;
    if(!capture_focus_sequence(1, "\x1b[I"))
        return 1;
    if(!capture_focus_sequence(0, "\x1b[O"))
        return 1;
    if(!capture_osc_color_query())
        return 1;
    if(!c1_controls_parse_csi_and_osc())
        return 1;
    if(!capture_response_sequence("primary device attributes", "\x1b[c",
                                  "\x1b[?1;2c"))
        return 1;
    if(!capture_response_sequence("secondary device attributes", "\x1b[>c",
                                  "\x1b[>0;0;0c"))
        return 1;
    if(!capture_response_sequence("device status report", "\x1b[5n",
                                  "\x1b[0n"))
        return 1;
    if(!capture_response_sequence("cursor position report", "abc\x1b[6n",
                                  "\x1b[1;4R"))
        return 1;
    if(!capture_response_sequence("private mode report set", "\x1b[?25$p",
                                  "\x1b[?25;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report reset",
                                  "\x1b[?25l\x1b[?25$p",
                                  "\x1b[?25;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report bracketed paste",
                                  "\x1b[?2004h\x1b[?2004$p",
                                  "\x1b[?2004;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report utf8 mouse",
                                  "\x1b[?1005h\x1b[?1005$p"
                                  "\x1b[?1005l\x1b[?1005$p",
                                  "\x1b[?1005;1$y\x1b[?1005;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report unknown",
                                  "\x1b[?9999$p",
                                  "\x1b[?9999;0$y"))
        return 1;
    if(!capture_response_sequence("insert mode report set",
                                  "\x1b[4h\x1b[4$p",
                                  "\x1b[4;1$y"))
        return 1;
    if(!capture_response_sequence("insert mode report reset",
                                  "\x1b[4l\x1b[4$p",
                                  "\x1b[4;2$y"))
        return 1;
    if(!resize_reflows_wrapped_text())
        return 1;
    if(!dec_autowrap_mode_controls_right_margin())
        return 1;
    if(!resize_reflows_scrollback())
        return 1;
    if(!origin_mode_uses_scroll_region())
        return 1;
    if(!erase_saved_lines_clears_scrollback_only())
        return 1;
    if(!insert_mode_inserts_printable_text())
        return 1;
    if(!wide_characters_use_two_columns())
        return 1;
    if(!combining_marks_stay_on_base_cell())
        return 1;
    if(!dec_special_graphics_charset_maps_line_drawing())
        return 1;
    if(!save_restore_preserves_rendition_and_charset())
        return 1;
    if(!osc_current_directory_updates_session_title())
        return 1;
    if(!alternate_screen_modes_preserve_expected_state())
        return 1;
    if(!ctrl_c_interrupts_child())
        return 1;
    printf("ok terminal\n");
    return 0;
}
