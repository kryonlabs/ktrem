#ifndef KAPSULE_APP_SEARCH_H
#define KAPSULE_APP_SEARCH_H

#include "app_state.h"

void focus_search(State *app);
int find_scrollback(State *app);
int find_scrollback_direction(State *app, int direction);
void draw_search_prompt(State *app);

#endif
