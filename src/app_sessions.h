#ifndef KAPSULE_APP_SESSIONS_H
#define KAPSULE_APP_SESSIONS_H

#include "app_state.h"

Session *active_session(State *app);
void set_active_session(State *app, int index);
void seed_theme_defaults_to_terminal(State *app, TerminalState *terminal);
void open_session(State *app, const char *command);
int open_launch_sessions(State *app);
void close_session(State *app, int index);
void save_sessions(State *app);
int restore_sessions(State *app);

#endif
