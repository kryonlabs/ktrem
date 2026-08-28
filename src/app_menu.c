#include "app_menu.h"

#include "app_commands.h"

#include "kryon.h"

void draw_app_menu_bar(State *app, Rectangle bounds)
{
    static const MenuItem file_items[] = {
        {MenuCommand, "New Tab", "Ctrl+Shift+T",
         APP_COMMAND_NEW_TAB, 0, 0, NULL, 0},
        {MenuCommand, "Close Tab", "Ctrl+Shift+W",
         APP_COMMAND_CLOSE_TAB, 0, 0, NULL, 0},
        {MenuSeparator, NULL, NULL, 0, 0, 0, NULL, 0},
        {MenuCommand, "Quit", "Ctrl+Shift+Q",
         APP_COMMAND_QUIT, 0, 0, NULL, 0}
    };
    static const MenuItem edit_items[] = {
        {MenuCommand, "Copy", "Ctrl+Shift+C",
         APP_COMMAND_COPY, 0, 0, NULL, 0},
        {MenuCommand, "Paste", "Ctrl+Shift+V",
         APP_COMMAND_PASTE, 0, 0, NULL, 0},
        {MenuCommand, "Select All", "Ctrl+Shift+A",
         APP_COMMAND_SELECT_ALL, 0, 0, NULL, 0},
        {MenuSeparator, NULL, NULL, 0, 0, 0, NULL, 0},
        {MenuCommand, "Find", "Ctrl+Shift+F",
         APP_COMMAND_FIND, 0, 0, NULL, 0},
        {MenuCommand, "Find Next", "Ctrl+Shift+G",
         APP_COMMAND_FIND_NEXT, 0, 0, NULL, 0},
        {MenuCommand, "Find Previous", "Ctrl+Shift+B",
         APP_COMMAND_FIND_PREVIOUS, 0, 0, NULL, 0}
    };
    static const MenuItem view_items[] = {
        {MenuCommand, "Zoom In", "Ctrl++",
         APP_COMMAND_FONT_INCREASE, 0, 0, NULL, 0},
        {MenuCommand, "Zoom Out", "Ctrl+-",
         APP_COMMAND_FONT_DECREASE, 0, 0, NULL, 0},
        {MenuCommand, "Normal Size", "Ctrl+0",
         APP_COMMAND_FONT_RESET, 0, 0, NULL, 0}
    };
    static const MenuItem cursor_items[] = {
        {MenuCommand, "Block", NULL,
         APP_COMMAND_CURSOR_BLOCK, 0, 0, NULL, 0},
        {MenuCommand, "Underline", NULL,
         APP_COMMAND_CURSOR_UNDERLINE, 0, 0, NULL, 0},
        {MenuCommand, "Bar", NULL,
         APP_COMMAND_CURSOR_BAR, 0, 0, NULL, 0}
    };
    static const MenuItem terminal_items[] = {
        {MenuCommand, "New Tab", "Ctrl+Shift+T",
         APP_COMMAND_NEW_TAB, 0, 0, NULL, 0},
        {MenuCommand, "Paste", "Ctrl+Shift+V",
         APP_COMMAND_PASTE, 0, 0, NULL, 0},
        {MenuSeparator, NULL, NULL, 0, 0, 0, NULL, 0},
        {MenuCommand, "Shell...", NULL,
         APP_COMMAND_PROFILE_SHELL, 0, 0, NULL, 0},
        {MenuCommand, "Working Directory...", NULL,
         APP_COMMAND_PROFILE_CWD, 0, 0, NULL, 0},
        {MenuCommand, "Terminal Font...", NULL,
         APP_COMMAND_PROFILE_FONT_FILE, 0, 0, NULL, 0},
        {MenuCommand, "Font Size...", NULL,
         APP_COMMAND_PROFILE_FONT_SIZE, 0, 0, NULL, 0},
        {MenuCommand, "Scrollback...", NULL,
         APP_COMMAND_PROFILE_SCROLLBACK, 0, 0, NULL, 0},
        {MenuCommand, "Foreground Color...", NULL,
         APP_COMMAND_PROFILE_FOREGROUND, 0, 0, NULL, 0},
        {MenuCommand, "Background Color...", NULL,
         APP_COMMAND_PROFILE_BACKGROUND, 0, 0, NULL, 0},
        {MenuCommand, "Cursor Color...", NULL,
         APP_COMMAND_PROFILE_CURSOR_COLOR, 0, 0, NULL, 0},
        {MenuCommand, "Selection Foreground...", NULL,
         APP_COMMAND_PROFILE_SELECTION_FOREGROUND, 0, 0, NULL, 0},
        {MenuCommand, "Selection Background...", NULL,
         APP_COMMAND_PROFILE_SELECTION_BACKGROUND, 0, 0, NULL, 0},
        {MenuSubmenu, "Cursor Style", NULL, 2500, 0, 0, cursor_items,
         (int)(sizeof(cursor_items) / sizeof(cursor_items[0]))},
        {MenuSeparator, NULL, NULL, 0, 0, 0, NULL, 0},
        {MenuCommand, "Dynamic Title Mode...", NULL,
         APP_COMMAND_PROFILE_DYNAMIC_TITLE_MODE, 0, 0, NULL, 0},
        {MenuCommand, "Backspace Key...", NULL,
         APP_COMMAND_PROFILE_BACKSPACE_BINDING, 0, 0, NULL, 0},
        {MenuCommand, "Delete Key...", NULL,
         APP_COMMAND_PROFILE_DELETE_BINDING, 0, 0, NULL, 0},
        {MenuCommand, "Ambiguous Width...", NULL,
         APP_COMMAND_PROFILE_AMBIGUOUS_WIDTH, 0, 0, NULL, 0},
        {MenuCommand, "Allow Bold Text...", NULL,
         APP_COMMAND_PROFILE_ALLOW_BOLD, 0, 0, NULL, 0},
        {MenuCommand, "Unlimited Scrollback...", NULL,
         APP_COMMAND_PROFILE_UNLIMITED_SCROLLBACK, 0, 0, NULL, 0},
        {MenuCommand, "Auto-hide Mouse...", NULL,
         APP_COMMAND_PROFILE_AUTO_HIDE_MOUSE, 0, 0, NULL, 0},
        {MenuCommand, "Middle Click Closes Tab...", NULL,
         APP_COMMAND_PROFILE_MIDDLE_CLICK_CLOSE_TAB, 0, 0, NULL, 0},
        {MenuCommand, "Always Show Tabs...", NULL,
         APP_COMMAND_PROFILE_ALWAYS_SHOW_TABS, 0, 0, NULL, 0},
        {MenuCommand, "Disable Menu Access Keys...", NULL,
         APP_COMMAND_PROFILE_DISABLE_MENU_MNEMONICS, 0, 0, NULL, 0},
        {MenuCommand, "Disable F10 Menu Shortcut...", NULL,
         APP_COMMAND_PROFILE_DISABLE_MENU_SHORTCUT, 0, 0, NULL, 0},
        {MenuCommand, "Disable F1 Help Shortcut...", NULL,
         APP_COMMAND_PROFILE_DISABLE_HELP_SHORTCUT, 0, 0, NULL, 0},
        {MenuCommand, "Background Opacity...", NULL,
         APP_COMMAND_PROFILE_BACKGROUND_OPACITY, 0, 0, NULL, 0},
        {MenuCommand, "Background Image...", NULL,
         APP_COMMAND_PROFILE_BACKGROUND_IMAGE, 0, 0, NULL, 0}
    };
    static const MenuItem tab_items[] = {
        {MenuCommand, "New Tab", "Ctrl+Shift+T",
         APP_COMMAND_NEW_TAB, 0, 0, NULL, 0},
        {MenuCommand, "Close Tab", "Ctrl+Shift+W",
         APP_COMMAND_CLOSE_TAB, 0, 0, NULL, 0},
        {MenuSeparator, NULL, NULL, 0, 0, 0, NULL, 0},
        {MenuCommand, "Next Tab", "Ctrl+Tab",
         APP_COMMAND_NEXT_TAB, 0, 0, NULL, 0},
        {MenuCommand, "Previous Tab", "Ctrl+Shift+Tab",
         APP_COMMAND_PREVIOUS_TAB, 0, 0, NULL, 0}
    };
    static const MenuItem help_items[] = {
        {MenuCommand, "About ktrem", NULL,
         APP_COMMAND_ABOUT, 0, 0, NULL, 0}
    };
    static const Menu menus[] = {
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
    MenuBarResult result;

    if(app == NULL)
        return;
    result = MenuBar(1200, bounds, menus,
                     (int)(sizeof(menus) / sizeof(menus[0])),
                     &app->top_menu_index);
    app_execute_command(app, result.activated_id);
}
