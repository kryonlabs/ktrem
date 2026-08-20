#include "app_context_menu.h"

#include "app_clipboard.h"
#include "app_search.h"

#include "kryon.h"

enum {
    CONTEXT_COPY = 2001,
    CONTEXT_PASTE,
    CONTEXT_PASTE_PRIMARY,
    CONTEXT_FIND,
    CONTEXT_FIND_NEXT,
    CONTEXT_FIND_PREVIOUS,
    CONTEXT_SELECT_ALL
};

void draw_context_menu(State *app, Session *session)
{
    UIMenuItem items[] = {
        {UI_MENU_COMMAND, "Copy", "Ctrl+Shift+C", CONTEXT_COPY, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V", CONTEXT_PASTE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste Primary", NULL, CONTEXT_PASTE_PRIMARY, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Select All", "Ctrl+Shift+A", CONTEXT_SELECT_ALL, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find", "Ctrl+Shift+F", CONTEXT_FIND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Next", "Ctrl+Shift+G", CONTEXT_FIND_NEXT, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Previous", "Ctrl+Shift+B", CONTEXT_FIND_PREVIOUS, 0, 0, NULL, 0}
    };
    int command;

    if(app == NULL || session == NULL || !app->context_menu_open)
        return;
    items[0].disabled = !app->selection.active;
    items[2].disabled = !primary_selection_available();
    command = ContextMenu((UIContextMenu){
        1300,
        app->viewport,
        items,
        (int)(sizeof(items) / sizeof(items[0])),
        &app->context_menu_open,
        &app->context_menu_x,
        &app->context_menu_y
    });
    if(command == CONTEXT_COPY) {
        copy_selection(app);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_PASTE) {
        paste_clipboard_to_session(session);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_PASTE_PRIMARY) {
        paste_primary_to_session(app, session);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_SELECT_ALL) {
        select_all(app);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_FIND) {
        focus_search(app);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_FIND_NEXT) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, 1);
        app->context_menu_open = 0;
    } else if(command == CONTEXT_FIND_PREVIOUS) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, -1);
        app->context_menu_open = 0;
    }
}
