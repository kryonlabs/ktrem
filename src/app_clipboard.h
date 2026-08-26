#ifndef KTREM_APP_CLIPBOARD_H
#define KTREM_APP_CLIPBOARD_H

#include "app_state.h"

#include <stddef.h>

int collect_selection_text(State *app, char *buffer, size_t buffer_size);
int primary_selection_available(void);
void update_primary_selection(State *app);
void copy_selection(State *app);
void paste_text_to_session(Session *session, const char *text);
void paste_clipboard_to_session(Session *session);
void paste_primary_to_session(State *app, Session *session);
void paste_primary_or_clipboard_to_session(State *app, Session *session);
void sync_host_clipboard_to_terminal(Session *session);
void flush_terminal_clipboard_to_host(Session *session);
void select_all(State *app);

#endif
