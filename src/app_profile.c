#include "app_profile.h"

#include "app_sessions.h"

#include <stdio.h>
#include <string.h>

void sync_terminal_theme_defaults(State *app,
                                  TerminalProfileColors old_colors)
{
    int i;
    TerminalProfileColors new_colors;

    if(app == NULL)
        return;
    new_colors = terminal_profile_colors(&app->config, &app->palette);
    for(i = 0; i < app->session_count; i++) {
        terminal_profile_sync_changed(&app->sessions[i].terminal, old_colors,
                                      new_colors);
    }
}

void open_profile_prompt(State *app, int kind)
{
    Session *session = active_session(app);
    int formatted;

    if(app == NULL)
        return;
    app->profile_prompt = kind;
    app->profile_focused = 1;
    app->profile_text[0] = '\0';
    formatted = FormatTerminalPaneProfilePromptValue(
        app->profile_text, (int)sizeof(app->profile_text), &app->config,
        kind);
    if(kind == PROFILE_PROMPT_SHELL && app->profile_text[0] == '\0' &&
       session != NULL) {
        snprintf(app->profile_text, sizeof(app->profile_text), "%s",
                 session->shell);
    } else if(kind == PROFILE_PROMPT_CWD) {
        char cwd[sizeof(app->profile_text)];

        cwd[0] = '\0';
        if(session != NULL)
            session_current_cwd(session, cwd, (int)sizeof(cwd));
        if(cwd[0] != '\0')
            snprintf(app->profile_text, sizeof(app->profile_text), "%s", cwd);
        else if(!formatted)
            FormatTerminalPaneProfilePromptValue(
                app->profile_text, (int)sizeof(app->profile_text),
                &app->config, kind);
    }
    app->profile_cursor = (int)strlen(app->profile_text);
}

const char *profile_prompt_title(int kind)
{
    return TerminalPaneProfilePromptTitle(kind);
}

void apply_profile_prompt(State *app)
{
    int i;
    int affects_colors;
    TerminalProfileColors old_colors;

    if(app == NULL)
        return;
    affects_colors =
        TerminalPaneProfilePromptAffectsColors(app->profile_prompt);
    old_colors = terminal_profile_colors(&app->config, &app->palette);
    if(!ApplyTerminalPaneProfilePromptValue(&app->config,
                                            config_profile_limits(),
                                            app->profile_prompt,
                                            app->profile_text))
        return;
    if(TerminalPaneProfilePromptAffectsFont(app->profile_prompt) &&
       app->config.terminal_font[0] != '\0')
        RegisterUIFontFileSource("kapsule-terminal",
                                 app->config.terminal_font, NULL, 0);
    if(TerminalPaneProfilePromptAffectsScrollback(app->profile_prompt)) {
        for(i = 0; i < app->session_count; i++)
            terminal_set_scrollback_limit(&app->sessions[i].terminal,
                                          app->config.scrollback_limit);
    }
    if(affects_colors)
        sync_terminal_theme_defaults(app, old_colors);
    config_save(&app->config);
}
