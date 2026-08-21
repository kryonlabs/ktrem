#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static long long now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static void feed_text(TerminalState *terminal, const char *text, int *bytes)
{
    int len;

    if(text == NULL)
        return;
    len = (int)strlen(text);
    terminal_feed(terminal, text, len);
    if(bytes != NULL)
        *bytes += len;
}

static void emit_result(const char *name, long long start_ns, long long end_ns,
                        int lines, int bytes)
{
    printf("{\"bench\":\"parser_replay\",\"workload\":\"%s\","
           "\"elapsed_ms\":%lld,\"lines\":%d,\"bytes\":%d}\n",
           name, (end_ns - start_ns) / 1000000LL, lines, bytes);
}

static void reset_terminal(TerminalState *terminal)
{
    terminal_close(terminal);
    terminal_init(terminal);
    terminal_resize(terminal, 100, 32);
}

static void bench_startup(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    int bytes = 0;

    reset_terminal(terminal);
    start_ns = now_ns();
    feed_text(terminal, "kapsule benchmark startup\n", &bytes);
    end_ns = now_ns();
    emit_result("startup", start_ns, end_ns, 1, bytes);
}

static void bench_ansi_flood(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[192];
    int bytes = 0;
    int lines = 18000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        int color = 30 + (i % 8);
        int bright = 90 + (i % 8);

        snprintf(line, sizeof(line),
                 "\033[%dm%06d\033[0m \033[%dmcolored terminal throughput "
                 "sample\033[0m abcdefghijklmnopqrstuvwxyz 0123456789\n",
                 color, i, bright);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("ansi_flood", start_ns, end_ns, lines, bytes);
}

static void bench_unicode_table(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[192];
    int bytes = 0;
    int lines = 6000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "│ %06d │ Kryon Λambda │ Kapsule ✓ │ width 測試 │ "
                 "box ──┼── │\n",
                 i);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("unicode_table", start_ns, end_ns, lines, bytes);
}

static void bench_alternate_redraw(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[192];
    int bytes = 0;
    int frames = 180;
    int rows = 28;
    int frame;

    reset_terminal(terminal);
    start_ns = now_ns();
    feed_text(terminal, "\033[?1049h", &bytes);
    for(frame = 1; frame <= frames; frame++) {
        int row;

        feed_text(terminal, "\033[H", &bytes);
        for(row = 1; row <= rows; row++) {
            snprintf(line, sizeof(line),
                     "frame %03d row %02d "
                     "▁▂▃▄▅▆▇█ kryon terminal redraw path █▇▆▅▄▃▂▁\n",
                     frame, row);
            feed_text(terminal, line, &bytes);
        }
    }
    feed_text(terminal, "\033[?1049l", &bytes);
    end_ns = now_ns();
    emit_result("alternate_redraw", start_ns, end_ns, frames * rows, bytes);
}

static void bench_dense_sgr(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[256];
    int bytes = 0;
    int lines = 9000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "\033[1;3%dm%05d\033[0m "
                 "\033[4;38;5;%dmunder\033[0m "
                 "\033[48;5;%dm block \033[0m "
                 "\033[3mitalic\033[0m \033[9mstrike\033[0m "
                 "\033[53mover\033[55m truecolor "
                 "\033[38;2;%d;%d;%dmrgb\033[0m\n",
                 i % 8, i, 16 + (i % 216), 232 + (i % 24), i % 255,
                 (i * 3) % 255, (i * 7) % 255);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("dense_sgr", start_ns, end_ns, lines, bytes);
}

static void bench_wrap_reflow(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[256];
    int bytes = 0;
    int lines = 7000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "wrap-%05d abcdefghijklmnopqrstuvwxyz0123456789 / "
                 "terminal reflow candidate / "
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 / kryon kapsule "
                 "xfce compatibility width sample 測試✓Λ │ ──┼── end\n",
                 i);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("wrap_reflow", start_ns, end_ns, lines, bytes);
}

static void bench_scrollback_flood(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[128];
    int bytes = 0;
    int lines = 50000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "scrollback %05d abcdefghijklmnopqrstuvwxyz 0123456789 "
                 "Kapsule Kryon\n",
                 i);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("scrollback_flood", start_ns, end_ns, lines, bytes);
}

static void bench_cursor_matrix(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char seq[64];
    int bytes = 0;
    int frames = 240;
    int cells = 120;
    int frame;

    reset_terminal(terminal);
    start_ns = now_ns();
    feed_text(terminal, "\033[?1049h\033[2J", &bytes);
    for(frame = 1; frame <= frames; frame++) {
        int cell;

        for(cell = 1; cell <= cells; cell++) {
            int row = 1 + ((cell + frame) % 28);
            int col = 1 + (((cell * 7) + frame) % 84);
            int color = 31 + ((cell + frame) % 7);

            snprintf(seq, sizeof(seq), "\033[%d;%dH\033[%dm%02x\033[0m",
                     row, col, color, cell % 255);
            feed_text(terminal, seq, &bytes);
        }
    }
    feed_text(terminal, "\033[?1049l", &bytes);
    end_ns = now_ns();
    emit_result("cursor_matrix", start_ns, end_ns, frames * cells, bytes);
}

static void bench_paste_burst(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[256];
    int bytes = 0;
    int lines = 12000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "paste-%05d abcdefghijklmnopqrstuvwxyz0123456789 "
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 symbols │┼─✓Λ測試 "
                 "repeated-paste-payload-for-terminal-input-path\n",
                 i);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("paste_burst", start_ns, end_ns, lines, bytes);
}

static void bench_hyperlink_grid(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[384];
    int bytes = 0;
    int lines = 7000;
    int i;

    reset_terminal(terminal);
    start_ns = now_ns();
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "link-%05d \033]8;id=item-%05d;"
                 "https://kapsule.kryonlabs.com/item/%05d\033\\"
                 "Kapsule benchmark link %05d\033]8;;\033\\ "
                 "status=%03d path=/tmp/kapsule/%05d\n",
                 i, i, i, i, i % 200, i);
        feed_text(terminal, line, &bytes);
    }
    end_ns = now_ns();
    emit_result("hyperlink_grid", start_ns, end_ns, lines, bytes);
}

static void fill_search_corpus(TerminalState *terminal, int *bytes)
{
    char line[192];
    int lines = 18000;
    int i;

    for(i = 1; i <= lines; i++) {
        const char *marker;

        if(i % 9 == 0)
            marker = "needle-critical";
        else if(i % 9 == 1)
            marker = "NeedleMixed";
        else
            marker = "background";
        snprintf(line, sizeof(line),
                 "search-%05d %s package=core module=terminal "
                 "result=%05d text=abcdefghijklmnopqrstuvwxyz\n",
                 i, marker, i * 17);
        feed_text(terminal, line, bytes);
    }
}

static void bench_search_corpus(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    int bytes = 0;

    reset_terminal(terminal);
    start_ns = now_ns();
    fill_search_corpus(terminal, &bytes);
    end_ns = now_ns();
    emit_result("search_corpus", start_ns, end_ns, 18000, bytes);
}

static void bench_search_visible(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    TerminalSearchMatch match;
    int bytes = 0;
    int searches = 500;
    int i;

    reset_terminal(terminal);
    fill_search_corpus(terminal, &bytes);
    start_ns = now_ns();
    for(i = 0; i < searches; i++) {
        const char *needle = (i % 2) == 0 ? "needle" : "NeedleMixed";

        terminal_find_visible(terminal, needle, i % 32, 0,
                              (i % 3) == 0 ? -1 : 1, 1, &match);
    }
    end_ns = now_ns();
    emit_result("search_visible", start_ns, end_ns, searches, bytes);
}

static void bench_resize_reflow(TerminalState *terminal)
{
    long long start_ns;
    long long end_ns;
    char line[256];
    int bytes = 0;
    int lines = 5000;
    int cycles = 120;
    int widths[] = {64, 120, 80, 132, 72, 100};
    int i;

    reset_terminal(terminal);
    terminal_set_scrollback_limit(terminal, 20000);
    for(i = 1; i <= lines; i++) {
        snprintf(line, sizeof(line),
                 "resize-%05d abcdefghijklmnopqrstuvwxyz0123456789 / "
                 "reflow benchmark text with mixed width 測試✓Λ │ ──┼── / "
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 end\n",
                 i);
        feed_text(terminal, line, &bytes);
    }
    start_ns = now_ns();
    for(i = 0; i < cycles; i++)
        terminal_resize(terminal,
                        widths[i % (int)(sizeof(widths) / sizeof(widths[0]))],
                        32);
    end_ns = now_ns();
    emit_result("resize_reflow", start_ns, end_ns, cycles, bytes);
}

int main(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    bench_startup(&terminal);
    bench_ansi_flood(&terminal);
    bench_unicode_table(&terminal);
    bench_alternate_redraw(&terminal);
    bench_dense_sgr(&terminal);
    bench_wrap_reflow(&terminal);
    bench_scrollback_flood(&terminal);
    bench_cursor_matrix(&terminal);
    bench_paste_burst(&terminal);
    bench_hyperlink_grid(&terminal);
    bench_search_corpus(&terminal);
    bench_search_visible(&terminal);
    bench_resize_reflow(&terminal);
    terminal_close(&terminal);
    return 0;
}
