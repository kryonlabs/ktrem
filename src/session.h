#ifndef SESSION_H
#define SESSION_H

#include "terminal.h"

typedef struct Session {
    Terminal terminal;
    int scroll_offset;
    int used;
    char cwd[1024];
    char shell[512];
    char command[1024];
} Session;

void session_init(Session *session);
int session_open(Session *session, const char *cwd, const char *shell,
                 const char *command, int cols, int rows,
                 int scrollback_limit);
void session_close(Session *session);
const char *session_title(const Session *session);

#endif

