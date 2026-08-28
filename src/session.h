#ifndef SESSION_H
#define SESSION_H

#include "terminal.h"

typedef struct Session {
    TerminalState terminal;
    int scroll_offset;
    int used;
    char cwd[1024];
    char shell[512];
    char command[1024];
    char title[128];
    int title_override;
} Session;

void session_init(Session *session);
int session_open(Session *session, const char *cwd, const char *shell,
                 const char *command, int cols, int rows,
                 int scrollback_limit);
void session_close(Session *session);
const char *session_title(const Session *session);
void session_set_title(Session *session, const char *title);
void session_restore_title(Session *session, const char *title,
                           int title_override);
void session_sync_terminal_metadata(Session *session);
void session_sync_terminal_metadata_with_mode(Session *session,
                                             int dynamic_title_mode);
void session_current_cwd(const Session *session, char *out, int out_size);

#endif
