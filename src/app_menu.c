#include "app_menu.h"

#include "app_commands.h"

#include "kryon.h"

void draw_app_menu_bar(State *app, Rectangle bounds)
{
    static const UIMenuItem file_items[] = {
        {UI_MENU_COMMAND, "New Tab", "Ctrl+Shift+T",
         APP_COMMAND_NEW_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Close Tab", "Ctrl+Shift+W",
         APP_COMMAND_CLOSE_TAB, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Quit", "Ctrl+Shift+Q",
         APP_COMMAND_QUIT, 0, 0, NULL, 0}
    };
    static const UIMenuItem edit_items[] = {
        {UI_MENU_COMMAND, "Copy", "Ctrl+Shift+C",
         APP_COMMAND_COPY, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V",
         APP_COMMAND_PASTE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Select All", "Ctrl+Shift+A",
         APP_COMMAND_SELECT_ALL, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find", "Ctrl+Shift+F",
         APP_COMMAND_FIND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Next", "Ctrl+Shift+G",
         APP_COMMAND_FIND_NEXT, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Previous", "Ctrl+Shift+B",
         APP_COMMAND_FIND_PREVIOUS, 0, 0, NULL, 0}
    };
    static const UIMenuItem view_items[] = {
        {UI_MENU_COMMAND, "Zoom In", "Ctrl++",
         APP_COMMAND_FONT_INCREASE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Zoom Out", "Ctrl+-",
         APP_COMMAND_FONT_DECREASE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Normal Size", "Ctrl+0",
         APP_COMMAND_FONT_RESET, 0, 0, NULL, 0}
    };
    static const UIMenuItem cursor_items[] = {
        {UI_MENU_COMMAND, "Block", NULL,
         APP_COMMAND_CURSOR_BLOCK, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Underline", NULL,
         APP_COMMAND_CURSOR_UNDERLINE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Bar", NULL,
         APP_COMMAND_CURSOR_BAR, 0, 0, NULL, 0}
    };
    static const UIMenuItem terminal_items[] = {
        {UI_MENU_COMMAND, "New Terminal", "Ctrl+Shift+T",
         APP_COMMAND_NEW_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V",
         APP_COMMAND_PASTE, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Shell...", NULL,
         APP_COMMAND_PROFILE_SHELL, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Working Directory...", NULL,
         APP_COMMAND_PROFILE_CWD, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Terminal Font...", NULL,
         APP_COMMAND_PROFILE_FONT_FILE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Font Size...", NULL,
         APP_COMMAND_PROFILE_FONT_SIZE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Scrollback...", NULL,
         APP_COMMAND_PROFILE_SCROLLBACK, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Foreground Color...", NULL,
         APP_COMMAND_PROFILE_FOREGROUND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Background Color...", NULL,
         APP_COMMAND_PROFILE_BACKGROUND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Cursor Color...", NULL,
         APP_COMMAND_PROFILE_CURSOR_COLOR, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Selection Foreground...", NULL,
         APP_COMMAND_PROFILE_SELECTION_FOREGROUND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Selection Background...", NULL,
         APP_COMMAND_PROFILE_SELECTION_BACKGROUND, 0, 0, NULL, 0},
        {UI_MENU_SUBMENU, "Cursor Style", NULL, 2500, 0, 0, cursor_items,
         (int)(sizeof(cursor_items) / sizeof(cursor_items[0]))}
    };
    static const UIMenuItem tab_items[] = {
        {UI_MENU_COMMAND, "Next Tab", "Ctrl+Tab",
         APP_COMMAND_NEXT_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Previous Tab", "Ctrl+Shift+Tab",
         APP_COMMAND_PREVIOUS_TAB, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Close Tab", "Ctrl+Shift+W",
         APP_COMMAND_CLOSE_TAB, 0, 0, NULL, 0}
    };
    static const UIMenuItem help_items[] = {
        {UI_MENU_COMMAND, "About Kapsule", NULL,
         APP_COMMAND_ABOUT, 0, 0, NULL, 0}
    };
    static const UIMenu menus[] = {
        {{0.0f, 0.0f, 0.0f, 0.0f}, "File", file_items,
         (int)(sizeof(file_items) / sizeof(file_items[0]))},
        {{0.0f, 0.0f, 0.0f, 0.0f}, "Edit", edit_items,
         (int)(sizeof(edit_items) / sizeof(edit_items[0]))},
        {{0.0f, 0.0f, 0.0f, 0.0f}, "View", view_items,
         (int)(sizeof(view_items) / sizeof(view_items[0]))},
        {{0.0f, 0.0f, 0.0f, 0.0f}, "Terminal", terminal_items,
         (int)(sizeof(terminal_items) / sizeof(terminal_items[0]))},
        {{0.0f, 0.0f, 0.0f, 0.0f}, "Tabs", tab_items,
         (int)(sizeof(tab_items) / sizeof(tab_items[0]))},
        {{0.0f, 0.0f, 0.0f, 0.0f}, "Help", help_items,
         (int)(sizeof(help_items) / sizeof(help_items[0]))}
    };
    UIMenuBarResult result;

    if(app == NULL)
        return;
    result = MenuBar(1200, bounds, menus,
                     (int)(sizeof(menus) / sizeof(menus[0])),
                     &app->top_menu_index);
    app_execute_command(app, result.activated_id);
}
