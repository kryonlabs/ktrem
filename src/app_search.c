#include "app_search.h"

#include "app_sessions.h"

#include "kryon.h"

#include <string.h>

void focus_search(State *app)
{
    if(app == NULL)
        return;
    app->search_visible = 1;
    app->search_focused = 1;
    app->search_cursor = (int)strlen(app->search_text);
}

int find_scrollback_direction(State *app, int direction)
{
    Session *session = active_session(app);
    TerminalState *terminal;
    TerminalPaneSearchController search;

    if(app == NULL || session == NULL || app->search_text[0] == '\0')
        return 0;
    terminal = &session->terminal;
    search = terminal_search_controller(terminal, &app->selection,
                                        app->visible_rows,
                                        app->first_visible_row,
                                        &session->scroll_offset);
    return TerminalPaneSearchFindNext(search, app->search_text, direction,
                                      NULL);
}

int find_scrollback(State *app)
{
    Session *session = active_session(app);
    TerminalState *terminal;
    TerminalPaneSearchController search;

    if(app == NULL || session == NULL || app->search_text[0] == '\0')
        return 0;
    terminal = &session->terminal;
    search = terminal_search_controller(terminal, &app->selection,
                                        app->visible_rows,
                                        app->first_visible_row,
                                        &session->scroll_offset);
    return TerminalPaneSearchFindInitial(search, app->search_text, NULL);
}

void draw_search_prompt(State *app)
{
    int result;
    int ctrl;
    int shift;

    if(app == NULL || !app->search_visible)
        return;
    ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if(IsKeyPressed(KEY_ESCAPE)) {
        app->search_visible = 0;
        app->search_focused = 0;
        return;
    }
    if(ctrl && shift && IsLayoutKeyPressed('g') &&
       app->search_text[0] != '\0') {
        find_scrollback_direction(app, 1);
        app->search_focused = 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('b') &&
       app->search_text[0] != '\0') {
        find_scrollback_direction(app, -1);
        app->search_focused = 1;
    }
    result = PromptDialog((PromptDialogProps){
        "Find",
        app->search_text,
        (int)sizeof(app->search_text),
        &app->search_cursor,
        &app->search_focused,
        "Cancel",
        "Find"
    });

    if(result == 1) {
        app->search_visible = 0;
        app->search_focused = 0;
    } else if(result == 2) {
        if(app->selection.active)
            find_scrollback_direction(app, -1);
        else
            find_scrollback(app);
        app->search_visible = 1;
        app->search_focused = 1;
    }
}
