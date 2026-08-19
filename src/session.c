#include "session.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *dst, int dst_size, const char *src)
{
    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, (size_t)dst_size, "%s", src);
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
    if(session->terminal.title[0] != '\0')
        return session->terminal.title;
    if(session->command[0] != '\0')
        return session->command;
    return "shell";
}

