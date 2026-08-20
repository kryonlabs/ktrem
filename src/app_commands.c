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
    app->config.font_size = clamp_int(font_size, KAPSULE_MIN_FONT_SIZE,
                                      KAPSULE_MAX_FONT_SIZE);
    config_save(&app->config);
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
        set_font_size(app, KAPSULE_DEFAULT_FONT_SIZE);
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

    if(session == NULL)
        return 0;
    if(ctrl && shift && IsKeyPressed(KEY_T)) {
        app_execute_command(app, APP_COMMAND_NEW_TAB);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_W)) {
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
    if(ctrl && shift && IsKeyPressed(KEY_C)) {
        app_execute_command(app, APP_COMMAND_COPY);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_V)) {
        app_execute_command(app, APP_COMMAND_PASTE);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_A)) {
        app_execute_command(app, APP_COMMAND_SELECT_ALL);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_F)) {
        app_execute_command(app, APP_COMMAND_FIND);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_G)) {
        app_execute_command(app, APP_COMMAND_FIND_NEXT);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_B)) {
        app_execute_command(app, APP_COMMAND_FIND_PREVIOUS);
        return 1;
    }
    if(ctrl && shift && IsKeyPressed(KEY_Q)) {
        app_execute_command(app, APP_COMMAND_QUIT);
        return 1;
    }
    return 0;
}
