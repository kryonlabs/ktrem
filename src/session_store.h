#ifndef KTREM_SESSION_STORE_H
#define KTREM_SESSION_STORE_H

#include "session.h"

typedef TerminalPaneSessionRecord SessionRecord;

int session_store_state_path(char *path, int path_size);
int session_store_save(const Session *sessions, int session_count, int active);
int session_store_load(SessionRecord *records, int max_records, int *active);

#endif
