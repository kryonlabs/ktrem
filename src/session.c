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

static const char *path_basename(const char *path)
{
    const char *end;
    const char *slash;

    if(path == NULL || path[0] == '\0')
        return "terminal";
    end = path + strlen(path);
    while(end > path && end[-1] == '/')
        end--;
    if(end == path)
        return "/";
    slash = end;
    while(slash > path && slash[-1] != '/')
        slash--;
    return slash;
}

static void set_default_title(Session *session)
{
    const char *title;
    int len;

    if(session == NULL)
        return;
    if(session->command[0] != '\0') {
        copy_text(session->title, (int)sizeof(session->title), session->command);
        return;
    }
    title = path_basename(session->cwd);
    len = (int)strlen(title);
    while(len > 1 && title[len - 1] == '/')
        len--;
    if(len <= 0) {
        copy_text(session->title, (int)sizeof(session->title), "terminal");
        return;
    }
    if(len >= (int)sizeof(session->title))
        len = (int)sizeof(session->title) - 1;
    memcpy(session->title, title, (size_t)len);
    session->title[len] = '\0';
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
    copy_text(session->title, (int)sizeof(session->title), title);
    session->title_override = title_override ? 1 : 0;
}

void session_sync_terminal_metadata(Session *session)
{
    if(session == NULL)
        return;
    if(session->terminal.title[0] != '\0' && !session->title_override)
        copy_text(session->title, (int)sizeof(session->title),
                  session->terminal.title);
    if(session->terminal.current_directory[0] != '\0' &&
       strcmp(session->cwd, session->terminal.current_directory) != 0) {
        copy_text(session->cwd, (int)sizeof(session->cwd),
                  session->terminal.current_directory);
        if(!session->title_override && session->terminal.title[0] == '\0')
            set_default_title(session);
    }
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
