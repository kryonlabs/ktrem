#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "terminal.h"

#include "terminal_screen.h"
#include "terminal_sixel.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int wait_for_child_exit(int pid, int timeout_ms)
{
    int elapsed = 0;
    int status;

    if(pid <= 0)
        return 1;
    for(;;) {
        int result = waitpid(pid, &status, WNOHANG);

        if(result > 0 || (result < 0 && errno == ECHILD))
            return 1;
        if(result < 0 && errno == EINTR)
            continue;
        if(result < 0)
            return 1;
        if(elapsed >= timeout_ms)
            return 0;
        usleep(10000);
        elapsed += 10;
    }
}

static void signal_terminal_process_group(int pid, int signal_number)
{
    if(pid <= 0)
        return;
    if(kill(-pid, signal_number) != 0)
        kill(pid, signal_number);
}

static void set_window_size(int fd, int cols, int rows)
{
    struct winsize size;

    memset(&size, 0, sizeof(size));
    size.ws_col = (unsigned short)cols;
    size.ws_row = (unsigned short)rows;
    ioctl(fd, TIOCSWINSZ, &size);
}

static int shell_is_bash(const char *shell)
{
    const char *name;

    if(shell == NULL)
        return 0;
    name = strrchr(shell, '/');
    name = name != NULL ? name + 1 : shell;
    return strcmp(name, "bash") == 0;
}

int terminal_spawn(TerminalState *terminal, const char *cwd, const char *shell,
                   const char *command, int cols, int rows)
{
    int master;
    int pid;

    if(terminal == NULL)
        return 0;
    terminal_close(terminal);
    terminal_init(terminal);
    if(!terminal_allocate_screen(terminal, cols, rows))
        return 0;
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if(master < 0)
        return 0;
    if(grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return 0;
    }
    set_window_size(master, terminal->cols, terminal->rows);
    pid = fork();
    if(pid < 0) {
        close(master);
        return 0;
    }
    if(pid == 0) {
        const char *slave_name = ptsname(master);
        const char *run_shell;
        char slave_path[256];
        int slave;

        if(slave_name == NULL)
            _exit(127);
        snprintf(slave_path, sizeof(slave_path), "%s", slave_name);
        setsid();
        close(master);
        slave = open(slave_path, O_RDWR);
        if(slave < 0)
            _exit(127);
#ifdef TIOCSCTTY
        ioctl(slave, TIOCSCTTY, 0);
#endif
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if(slave > 2)
            close(slave);
        if(cwd != NULL && cwd[0] != '\0' && chdir(cwd) != 0)
            _exit(127);
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        run_shell = shell;
        if(run_shell == NULL || run_shell[0] == '\0')
            run_shell = getenv("SHELL");
        if(run_shell == NULL || run_shell[0] == '\0')
            run_shell = "/bin/sh";
        if(command != NULL && command[0] != '\0') {
            if(shell_is_bash(run_shell))
                execl(run_shell, run_shell, "-i", "-c", command, (char *)NULL);
            else
                execl(run_shell, run_shell, "-lc", command, (char *)NULL);
        }
        else
            execl(run_shell, run_shell, "-i", (char *)NULL);
        _exit(127);
    }
    {
        int flags = fcntl(master, F_GETFL, 0);

        if(flags >= 0)
            fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    terminal->pid = pid;
    terminal->fd = master;
    terminal->running = 1;
    return 1;
}

int terminal_open(TerminalState *terminal, const char *cwd, int cols, int rows)
{
    return terminal_spawn(terminal, cwd, NULL, NULL, cols, rows);
}

int terminal_write(TerminalState *terminal, const void *data, int size)
{
    int written;

    if(terminal == NULL || !terminal->running || terminal->fd < 0 ||
       data == NULL || size <= 0)
        return 0;
    written = (int)write(terminal->fd, data, (size_t)size);
    return written > 0 ? written : 0;
}

int terminal_write_text(TerminalState *terminal, const char *text)
{
    if(text == NULL)
        return 0;
    return terminal_write(terminal, text, (int)strlen(text));
}

int terminal_poll_bytes(TerminalState *terminal)
{
    char buffer[4096];
    int status;
    int closed = 0;
    int bytes = 0;

    if(terminal == NULL || terminal->fd < 0)
        return 0;
    for(;;) {
        int got = (int)read(terminal->fd, buffer, sizeof(buffer));

        if(got < 0) {
            if(errno == EINTR)
                continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if(errno == EIO)
                closed = 1;
            break;
        }
        if(got == 0) {
            closed = 1;
            break;
        }
        terminal_feed(terminal, buffer, got);
        bytes += got;
    }
    if(terminal->running && terminal->pid > 0) {
        if(waitpid(terminal->pid, &status, WNOHANG) > 0)
            terminal->running = 0;
    }
    if(closed && terminal->fd >= 0) {
        close(terminal->fd);
        terminal->fd = -1;
        terminal->running = 0;
    }
    return bytes;
}

int terminal_poll(TerminalState *terminal)
{
    if(terminal == NULL)
        return 0;
    terminal_poll_bytes(terminal);
    return terminal->running;
}

void terminal_resize(TerminalState *terminal, int cols, int rows)
{
    if(terminal == NULL)
        return;
    cols = terminal_clamp_int(cols, 8, MAX_COLS);
    rows = terminal_clamp_int(rows, 4, MAX_ROWS);
    if(cols == terminal->cols && rows == terminal->rows)
        return;
    if(!terminal_allocate_screen(terminal, cols, rows))
        return;
    if(terminal->fd >= 0)
        set_window_size(terminal->fd, terminal->cols, terminal->rows);
}

void terminal_set_scrollback_limit(TerminalState *terminal, int rows)
{
    if(terminal == NULL)
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
    if(terminal == NULL)
        return;
    if(terminal->fd >= 0) {
        close(terminal->fd);
        terminal->fd = -1;
    }
    if(terminal->running && terminal->pid > 0) {
        signal_terminal_process_group(terminal->pid, SIGHUP);
        signal_terminal_process_group(terminal->pid, SIGTERM);
        if(!wait_for_child_exit(terminal->pid, 200)) {
            signal_terminal_process_group(terminal->pid, SIGKILL);
            (void)wait_for_child_exit(terminal->pid, 500);
        }
    } else if(terminal->pid > 0) {
        (void)waitpid(terminal->pid, NULL, WNOHANG);
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
