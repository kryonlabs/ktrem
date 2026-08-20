#include "session.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void copy_text(char *dst, int dst_size, const char *src)
{
    size_t len;

    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    len = strlen(src);
    if(len >= (size_t)dst_size)
        len = (size_t)dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void set_short_title(Session *session, const char *text,
                            const char *fallback)
{
    if(session == NULL)
        return;
    (void)FormatTerminalPaneSessionTitle(
        session->title, (int)sizeof(session->title), text, fallback);
}

static void set_default_title(Session *session)
{
    if(session == NULL)
        return;
    if(session->command[0] != '\0') {
        set_short_title(session, session->command, "terminal");
        return;
    }
    set_short_title(session, session->cwd, "terminal");
}

void session_init(Session *session)
{
    if(session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    terminal_init(&session->terminal);
}

int session_open(Session *session, const char *cwd, const char *shell,
                 const char *command, int cols, int rows,
                 int scrollback_limit)
{
    if(session == NULL)
        return 0;
    session_close(session);
    session_init(session);
    copy_text(session->cwd, (int)sizeof(session->cwd), cwd);
    copy_text(session->shell, (int)sizeof(session->shell), shell);
    copy_text(session->command, (int)sizeof(session->command), command);
    set_default_title(session);
    terminal_set_scrollback_limit(&session->terminal, scrollback_limit);
    if(!terminal_spawn(&session->terminal, session->cwd, session->shell,
                       session->command, cols, rows)) {
        terminal_feed(&session->terminal, "Could not start shell\r\n", 23);
        session->used = 1;
        return 0;
    }
    session->used = 1;
    return 1;
}

void session_close(Session *session)
{
    if(session == NULL)
        return;
    terminal_close(&session->terminal);
    session->used = 0;
    session->scroll_offset = 0;
}

const char *session_title(const Session *session)
{
    if(session == NULL)
        return "";
    if(session->title[0] != '\0')
        return session->title;
    return "terminal";
}

void session_set_title(Session *session, const char *title)
{
    if(session == NULL)
        return;
    copy_text(session->title, (int)sizeof(session->title),
              title != NULL && title[0] != '\0' ? title : "terminal");
    session->title_override = 1;
}

void session_restore_title(Session *session, const char *title,
                           int title_override)
{
    if(session == NULL || title == NULL || title[0] == '\0')
        return;
    session->title_override = title_override ? 1 : 0;
    if(session->title_override)
        copy_text(session->title, (int)sizeof(session->title), title);
    else
        set_short_title(session, title, "terminal");
}

void session_sync_terminal_metadata(Session *session)
{
    if(session == NULL)
        return;
    if(session->terminal.current_directory[0] != '\0' &&
       strcmp(session->cwd, session->terminal.current_directory) != 0) {
        copy_text(session->cwd, (int)sizeof(session->cwd),
                  session->terminal.current_directory);
        if(!session->title_override && session->terminal.title[0] == '\0')
            set_default_title(session);
    }
    if(session->terminal.title[0] != '\0' && !session->title_override)
        set_short_title(session, session->terminal.title, "terminal");
}

void session_current_cwd(const Session *session, char *out, int out_size)
{
    char link_path[64];
    ssize_t len;

    if(out == NULL || out_size <= 0)
        return;
    out[0] = '\0';
    if(session == NULL)
        return;
    if(session->terminal.current_directory[0] != '\0') {
        copy_text(out, out_size, session->terminal.current_directory);
        return;
    }
    if(session->terminal.pid > 0) {
        snprintf(link_path, sizeof(link_path), "/proc/%d/cwd",
                 session->terminal.pid);
        len = readlink(link_path, out, (size_t)out_size - 1);
        if(len > 0) {
            out[len] = '\0';
            return;
        }
    }
    copy_text(out, out_size, session->cwd);
}
