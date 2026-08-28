#include "app_commands.h"

#include "app_clipboard.h"
#include "app_profile.h"
#include "app_search.h"
#include "app_sessions.h"
#include "config.h"
#include "input.h"
#include "profile.h"

#include "kryon.h"

static int clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static void set_font_size(State *app, int font_size)
{
    if(app == NULL)
        return;
    app->config.font_size = clamp_int(font_size, KTREM_MIN_FONT_SIZE,
                                      KTREM_MAX_FONT_SIZE);
    config_save(&app->config);
}

static int handle_alt_tab_shortcut(State *app)
{
    static const int keypad_keys[MAX_SESSIONS] = {
        KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
        KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8
    };
    int i;

    if(app == NULL)
        return 0;
    for(i = 0; i < app->session_count && i < MAX_SESSIONS; i++) {
        if(IsLayoutKeyPressed('1' + i) || IsKeyPressed(keypad_keys[i])) {
            set_active_session(app, i);
            return 1;
        }
    }
    return 0;
}

void app_execute_command(State *app, int command)
{
    Session *session = active_session(app);

    if(app == NULL)
        return;
    switch(command) {
    case APP_COMMAND_NEW_TAB:
        open_session(app, NULL);
        break;
    case APP_COMMAND_CLOSE_TAB:
        close_session(app, app->active);
        break;
    case APP_COMMAND_QUIT:
        app->quit_requested = 1;
        break;
    case APP_COMMAND_COPY:
        copy_selection(app);
        break;
    case APP_COMMAND_PASTE:
        paste_clipboard_to_session(session);
        break;
    case APP_COMMAND_SELECT_ALL:
        select_all(app);
        break;
    case APP_COMMAND_FONT_INCREASE:
        set_font_size(app, app->config.font_size + 1);
        break;
    case APP_COMMAND_FONT_DECREASE:
        set_font_size(app, app->config.font_size - 1);
        break;
    case APP_COMMAND_FONT_RESET:
        set_font_size(app, KTREM_DEFAULT_FONT_SIZE);
        break;
    case APP_COMMAND_NEXT_TAB:
        if(app->session_count > 0)
            set_active_session(app, (app->active + 1) % app->session_count);
        break;
    case APP_COMMAND_PREVIOUS_TAB:
        if(app->session_count > 0)
            set_active_session(app, (app->active + app->session_count - 1) %
                                    app->session_count);
        break;
    case APP_COMMAND_FIND:
        focus_search(app);
        break;
    case APP_COMMAND_FIND_NEXT:
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, 1);
        break;
    case APP_COMMAND_FIND_PREVIOUS:
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, -1);
        break;
    case APP_COMMAND_PROFILE_SHELL:
        open_profile_prompt(app, PROFILE_PROMPT_SHELL);
        break;
    case APP_COMMAND_PROFILE_CWD:
        open_profile_prompt(app, PROFILE_PROMPT_CWD);
        break;
    case APP_COMMAND_PROFILE_FONT_FILE:
        open_profile_prompt(app, PROFILE_PROMPT_FONT_FILE);
        break;
    case APP_COMMAND_PROFILE_FONT_SIZE:
        open_profile_prompt(app, PROFILE_PROMPT_FONT_SIZE);
        break;
    case APP_COMMAND_PROFILE_SCROLLBACK:
        open_profile_prompt(app, PROFILE_PROMPT_SCROLLBACK);
        break;
    case APP_COMMAND_PROFILE_FOREGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_FOREGROUND);
        break;
    case APP_COMMAND_PROFILE_BACKGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_BACKGROUND);
        break;
    case APP_COMMAND_PROFILE_CURSOR_COLOR:
        open_profile_prompt(app, PROFILE_PROMPT_CURSOR_COLOR);
        break;
    case APP_COMMAND_PROFILE_SELECTION_FOREGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_SELECTION_FOREGROUND);
        break;
    case APP_COMMAND_PROFILE_SELECTION_BACKGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_SELECTION_BACKGROUND);
        break;
    case APP_COMMAND_PROFILE_DYNAMIC_TITLE_MODE:
        open_profile_prompt(app, PROFILE_PROMPT_DYNAMIC_TITLE_MODE);
        break;
    case APP_COMMAND_PROFILE_BACKSPACE_BINDING:
        open_profile_prompt(app, PROFILE_PROMPT_BACKSPACE_BINDING);
        break;
    case APP_COMMAND_PROFILE_DELETE_BINDING:
        open_profile_prompt(app, PROFILE_PROMPT_DELETE_BINDING);
        break;
    case APP_COMMAND_PROFILE_AMBIGUOUS_WIDTH:
        open_profile_prompt(app, PROFILE_PROMPT_AMBIGUOUS_WIDTH);
        break;
    case APP_COMMAND_PROFILE_ALLOW_BOLD:
        open_profile_prompt(app, PROFILE_PROMPT_ALLOW_BOLD);
        break;
    case APP_COMMAND_PROFILE_UNLIMITED_SCROLLBACK:
        open_profile_prompt(app, PROFILE_PROMPT_UNLIMITED_SCROLLBACK);
        break;
    case APP_COMMAND_PROFILE_AUTO_HIDE_MOUSE:
        open_profile_prompt(app, PROFILE_PROMPT_AUTO_HIDE_MOUSE);
        break;
    case APP_COMMAND_PROFILE_MIDDLE_CLICK_CLOSE_TAB:
        open_profile_prompt(app, PROFILE_PROMPT_MIDDLE_CLICK_CLOSE_TAB);
        break;
    case APP_COMMAND_PROFILE_ALWAYS_SHOW_TABS:
        open_profile_prompt(app, PROFILE_PROMPT_ALWAYS_SHOW_TABS);
        break;
    case APP_COMMAND_PROFILE_DISABLE_MENU_MNEMONICS:
        open_profile_prompt(app, PROFILE_PROMPT_DISABLE_MENU_MNEMONICS);
        break;
    case APP_COMMAND_PROFILE_DISABLE_MENU_SHORTCUT:
        open_profile_prompt(app, PROFILE_PROMPT_DISABLE_MENU_SHORTCUT);
        break;
    case APP_COMMAND_PROFILE_DISABLE_HELP_SHORTCUT:
        open_profile_prompt(app, PROFILE_PROMPT_DISABLE_HELP_SHORTCUT);
        break;
    case APP_COMMAND_PROFILE_BACKGROUND_OPACITY:
        open_profile_prompt(app, PROFILE_PROMPT_BACKGROUND_OPACITY);
        break;
    case APP_COMMAND_PROFILE_BACKGROUND_IMAGE:
        open_profile_prompt(app, PROFILE_PROMPT_BACKGROUND_IMAGE);
        break;
    case APP_COMMAND_CURSOR_BLOCK:
        app->config.cursor_style = TERMINAL_CURSOR_BLOCK;
        config_save(&app->config);
        break;
    case APP_COMMAND_CURSOR_UNDERLINE:
        app->config.cursor_style = TERMINAL_CURSOR_UNDERLINE;
        config_save(&app->config);
        break;
    case APP_COMMAND_CURSOR_BAR:
        app->config.cursor_style = TERMINAL_CURSOR_BAR;
        config_save(&app->config);
        break;
    case APP_COMMAND_ABOUT:
        app->about_visible = 1;
        break;
    default:
        break;
    }
}

int app_handle_shortcuts(State *app)
{
    Session *session = active_session(app);
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

    if(session == NULL)
        return 0;
    if(alt && handle_alt_tab_shortcut(app))
        return 1;
    if(ctrl && shift && IsLayoutKeyPressed('t')) {
        app_execute_command(app, APP_COMMAND_NEW_TAB);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('w')) {
        app_execute_command(app, APP_COMMAND_CLOSE_TAB);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_TAB)) {
        app_execute_command(app, APP_COMMAND_PREVIOUS_TAB);
        return 1;
    }
    if(ctrl && IsKeyPressed(KEY_PAGE_UP)) {
        app_execute_command(app, APP_COMMAND_PREVIOUS_TAB);
        return 1;
    }
    if(ctrl && IsKeyPressed(KEY_TAB)) {
        app_execute_command(app, APP_COMMAND_NEXT_TAB);
        return 1;
    }
    if(ctrl && IsKeyPressed(KEY_PAGE_DOWN)) {
        app_execute_command(app, APP_COMMAND_NEXT_TAB);
        return 1;
    }
    if(ctrl && (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) {
        app_execute_command(app, APP_COMMAND_FONT_INCREASE);
        return 1;
    }
    if(ctrl && (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) {
        app_execute_command(app, APP_COMMAND_FONT_DECREASE);
        return 1;
    }
    if(ctrl && (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0))) {
        app_execute_command(app, APP_COMMAND_FONT_RESET);
        return 1;
    }
    if(ctrl && IsKeyPressed(KEY_INSERT)) {
        app_execute_command(app, APP_COMMAND_COPY);
        return 1;
    }
    if(shift && IsKeyPressed(KEY_INSERT)) {
        app_execute_command(app, APP_COMMAND_PASTE);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('c')) {
        app_execute_command(app, APP_COMMAND_COPY);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('v')) {
        app_execute_command(app, APP_COMMAND_PASTE);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('a')) {
        app_execute_command(app, APP_COMMAND_SELECT_ALL);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('f')) {
        app_execute_command(app, APP_COMMAND_FIND);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('g')) {
        app_execute_command(app, APP_COMMAND_FIND_NEXT);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('b')) {
        app_execute_command(app, APP_COMMAND_FIND_PREVIOUS);
        return 1;
    }
    if(ctrl && shift && IsLayoutKeyPressed('q')) {
        app_execute_command(app, APP_COMMAND_QUIT);
        return 1;
    }
    return 0;
}
