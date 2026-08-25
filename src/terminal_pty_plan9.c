#ifdef KRYON_NATIVE_PLAN9

#include "terminal.h"

#include "terminal_screen.h"
#include "terminal_sixel.h"

#include "kryon_plan9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAN9_RING_SIZE 65536
#define PLAN9_RING_MASK (PLAN9_RING_SIZE - 1)
#define PLAN9_SESSION_LIMIT 16

typedef struct Plan9TerminalSession {
    TerminalState *terminal;
    int input_fd;
    int output_fd;
    int child_pid;
    int reader_pid;
    volatile int closed;
    volatile int reader_done;
    volatile uint head;
    volatile uint tail;
    uchar ring[PLAN9_RING_SIZE];
} Plan9TerminalSession;

static Plan9TerminalSession plan9_sessions[PLAN9_SESSION_LIMIT];

static Plan9TerminalSession *plan9_find_session(TerminalState *terminal)
{
    int i;

    if(terminal == nil)
        return nil;
    for(i = 0; i < PLAN9_SESSION_LIMIT; i++)
        if(plan9_sessions[i].terminal == terminal)
            return &plan9_sessions[i];
    return nil;
}

static Plan9TerminalSession *plan9_alloc_session(TerminalState *terminal)
{
    int i;

    for(i = 0; i < PLAN9_SESSION_LIMIT; i++) {
        if(plan9_sessions[i].terminal == nil ||
           plan9_sessions[i].reader_done) {
            memset(&plan9_sessions[i], 0, sizeof(plan9_sessions[i]));
            plan9_sessions[i].terminal = terminal;
            plan9_sessions[i].input_fd = -1;
            plan9_sessions[i].output_fd = -1;
            return &plan9_sessions[i];
        }
    }
    return nil;
}

static int plan9_can_exec(const char *path)
{
    int fd;

    if(path == nil || path[0] == '\0')
        return 0;
    fd = open(path, OEXEC);
    if(fd < 0)
        return 0;
    close(fd);
    return 1;
}

static const char *plan9_basename(const char *path)
{
    const char *slash;

    if(path == nil || path[0] == '\0')
        return "rc";
    slash = strrchr(path, '/');
    return slash != nil ? slash + 1 : path;
}

static const char *plan9_default_shell(void)
{
    const char *shell;

    shell = getenv("shell");
    if(shell != nil && shell[0] != '\0')
        return shell;
    shell = getenv("SHELL");
    if(shell != nil && shell[0] != '\0')
        return shell;
    if(plan9_can_exec("/bin/rc"))
        return "/bin/rc";
    if(plan9_can_exec("/bin/sh"))
        return "/bin/sh";
    return "/bin/rc";
}

static void plan9_reader_loop(Plan9TerminalSession *session)
{
    uchar buffer[4096];

    while(session != nil && !session->closed) {
        long n;
        long i;

        n = read(session->output_fd, buffer, sizeof(buffer));
        if(n <= 0)
            break;
        for(i = 0; i < n && !session->closed; i++) {
            uint next;

            next = (session->head + 1) & PLAN9_RING_MASK;
            while(next == session->tail && !session->closed)
                sleep(1);
            if(session->closed)
                break;
            session->ring[session->head] = buffer[i];
            session->head = next;
        }
    }
    session->reader_done = 1;
}

static int plan9_start_reader(Plan9TerminalSession *session)
{
    int pid;

    pid = rfork(RFPROC|RFMEM|RFNOWAIT);
    if(pid < 0)
        return 0;
    if(pid == 0) {
        plan9_reader_loop(session);
        exits(nil);
    }
    session->reader_pid = pid;
    return 1;
}

static void plan9_exec_child(const char *cwd, const char *shell,
                             const char *command)
{
    const char *run_shell;
    const char *name;

    if(cwd != nil && cwd[0] != '\0')
        chdir((char *)cwd);
    putenv("term", "xterm-256color");
    putenv("TERM", "xterm-256color");
    putenv("COLORTERM", "truecolor");
    run_shell = shell;
    if(run_shell == nil || run_shell[0] == '\0')
        run_shell = plan9_default_shell();
    name = plan9_basename(run_shell);
    if(command != nil && command[0] != '\0')
        execl((char *)run_shell, (char *)name, "-c", (char *)command, nil);
    execl((char *)run_shell, (char *)name, "-i", nil);
    exits("exec");
}

int terminal_spawn(TerminalState *terminal, const char *cwd, const char *shell,
                   const char *command, int cols, int rows)
{
    int to_child[2];
    int from_child[2];
    int pid;
    Plan9TerminalSession *session;

    if(terminal == nil)
        return 0;
    terminal_close(terminal);
    terminal_init(terminal);
    if(!terminal_allocate_screen(terminal, cols, rows))
        return 0;
    if(pipe(to_child) < 0)
        return 0;
    if(pipe(from_child) < 0) {
        close(to_child[0]);
        close(to_child[1]);
        return 0;
    }
    session = plan9_alloc_session(terminal);
    if(session == nil) {
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        return 0;
    }
    pid = rfork(RFPROC|RFFDG|RFENVG|RFNOTEG|RFNOWAIT);
    if(pid < 0) {
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        session->terminal = nil;
        return 0;
    }
    if(pid == 0) {
        close(to_child[1]);
        close(from_child[0]);
        dup(to_child[0], 0);
        dup(from_child[1], 1);
        dup(from_child[1], 2);
        close(to_child[0]);
        close(from_child[1]);
        plan9_exec_child(cwd, shell, command);
    }
    close(to_child[0]);
    close(from_child[1]);
    session->input_fd = to_child[1];
    session->output_fd = from_child[0];
    session->child_pid = pid;
    terminal->pid = pid;
    terminal->fd = session->input_fd;
    terminal->running = 1;
    if(!plan9_start_reader(session)) {
        terminal_close(terminal);
        return 0;
    }
    return 1;
}

int terminal_open(TerminalState *terminal, const char *cwd, int cols, int rows)
{
    return terminal_spawn(terminal, cwd, nil, nil, cols, rows);
}

int terminal_write(TerminalState *terminal, const void *data, int size)
{
    Plan9TerminalSession *session;
    long written;

    if(terminal == nil || data == nil || size <= 0)
        return 0;
    session = plan9_find_session(terminal);
    if(session == nil || session->closed || session->input_fd < 0)
        return 0;
    written = write(session->input_fd, data, size);
    return written > 0 ? (int)written : 0;
}

int terminal_write_text(TerminalState *terminal, const char *text)
{
    if(text == nil)
        return 0;
    return terminal_write(terminal, text, strlen(text));
}

int terminal_poll_bytes(TerminalState *terminal)
{
    Plan9TerminalSession *session;
    char buffer[4096];
    int bytes;

    if(terminal == nil)
        return 0;
    session = plan9_find_session(terminal);
    if(session == nil)
        return 0;
    bytes = 0;
    while(session->tail != session->head) {
        int n;

        n = 0;
        while(session->tail != session->head &&
              n < (int)sizeof(buffer)) {
            buffer[n++] = (char)session->ring[session->tail];
            session->tail = (session->tail + 1) & PLAN9_RING_MASK;
        }
        terminal_feed(terminal, buffer, n);
        bytes += n;
    }
    if(session->reader_done && session->tail == session->head) {
        if(session->output_fd >= 0) {
            close(session->output_fd);
            session->output_fd = -1;
        }
        terminal->running = 0;
    }
    return bytes;
}

int terminal_poll(TerminalState *terminal)
{
    if(terminal == nil)
        return 0;
    terminal_poll_bytes(terminal);
    return terminal->running;
}

void terminal_resize(TerminalState *terminal, int cols, int rows)
{
    if(terminal == nil)
        return;
    cols = terminal_clamp_int(cols, 8, MAX_COLS);
    rows = terminal_clamp_int(rows, 4, MAX_ROWS);
    if(cols == terminal->cols && rows == terminal->rows)
        return;
    terminal_allocate_screen(terminal, cols, rows);
}

void terminal_set_scrollback_limit(TerminalState *terminal, int rows)
{
    if(terminal == nil)
        return;
    if(rows < 0)
        rows = 0;
    if(rows > 100000)
        rows = 100000;
    if(rows == 0)
        rows = SCROLLBACK_LIMIT;
    terminal->scrollback_limit = rows;
    if(terminal->cols > 0)
        terminal_allocate_scrollback(terminal);
}

void terminal_close(TerminalState *terminal)
{
    Plan9TerminalSession *session;

    if(terminal == nil)
        return;
    session = plan9_find_session(terminal);
    if(session != nil) {
        session->closed = 1;
        if(session->input_fd >= 0) {
            close(session->input_fd);
            session->input_fd = -1;
        }
        if(session->output_fd >= 0) {
            close(session->output_fd);
            session->output_fd = -1;
        }
        if(session->child_pid > 0)
            postnote(PNPROC, session->child_pid, "hangup");
        if(session->reader_pid > 0)
            postnote(PNPROC, session->reader_pid, "hangup");
        session->terminal = nil;
    }
    free(terminal->main_cells);
    free(terminal->alt_cells);
    free(terminal->scrollback);
    free(terminal->main_wrapped);
    free(terminal->alt_wrapped);
    free(terminal->scrollback_wrapped);
    free(terminal->tab_stops);
    terminal_sixel_clear_images(terminal, -1);
    free(terminal->sixel_images);
    FreeTerminalPaneDCSBuffer(&terminal->dcs);
    terminal_init(terminal);
}

#endif
