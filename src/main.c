#include "app_clipboard.h"
#include "app_input.h"
#include "app_profile.h"
#include "app_sessions.h"
#include "app_state.h"
#include "app_terminal_view.h"
#include "config.h"
#include "palette.h"
#include "session.h"
#include "terminal.h"

#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sync_window_title(State *app)
{
    Session *session;
    char title[160];

    if(app == NULL)
        return;
    session = active_session(app);
    if(session != NULL && session_title(session)[0] != '\0')
        snprintf(title, sizeof(title), "%s - Kapsule", session_title(session));
    else
        snprintf(title, sizeof(title), "Kapsule");
    if(strcmp(app->window_title, title) != 0) {
        SetWindowTitle(title);
        snprintf(app->window_title, sizeof(app->window_title), "%s", title);
    }
}

static int frame_width(void)
{
    return GetScreenWidth();
}

static int frame_height(void)
{
    return GetScreenHeight();
}

static void refresh_app_theme(State *app)
{
    TerminalProfileColors old_colors;

    if(app == NULL)
        return;
    old_colors = terminal_profile_colors(&app->config, &app->palette);
    palette_default(&app->palette);
    palette_apply_system_theme(&app->palette);
    sync_terminal_theme_defaults(app, old_colors);
}

static void load_kryon_font(const Config *config)
{
    static const char *ui_paths[] = {
        "../kryon/fonts/noto/NotoSans-Regular.ttf",
        "fonts/noto/NotoSans-Regular.ttf",
        "vendor/kryon/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    static const char *terminal_paths[] = {
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/opentype/urw-base35/NimbusMonoPS-Regular.otf",
        "../kryon/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    int i;
    int ui_loaded = 0;

    for(i = 0; ui_paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource("kapsule-ui", ui_paths[i], NULL, 0) &&
           UseUIFont("kapsule-ui")) {
            ui_loaded = 1;
            break;
        }
    }
    if(!ui_loaded)
        EnsureUIDefaultFont();
    if(config != NULL && config->terminal_font[0] != '\0' &&
       RegisterUIFontFileSource("kapsule-terminal", config->terminal_font,
                                NULL, 0))
        return;
    for(i = 0; terminal_paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource("kapsule-terminal", terminal_paths[i],
                                    NULL, 0))
            return;
    }
}

static void parse_args(Config *config, int argc, char **argv)
{
    int i;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--font-size") == 0 && i + 1 < argc)
            config_apply_arg(config, "font-size", argv[++i]);
        else if(strcmp(argv[i], "--working-directory") == 0 && i + 1 < argc)
            config_apply_arg(config, "working-directory", argv[++i]);
        else if(strcmp(argv[i], "--shell") == 0 && i + 1 < argc)
            config_apply_arg(config, "shell", argv[++i]);
        else if(strcmp(argv[i], "--command") == 0 && i + 1 < argc)
            config_apply_arg(config, "command", argv[++i]);
        else if(strcmp(argv[i], "--scrollback") == 0 && i + 1 < argc)
            config_apply_arg(config, "scrollback", argv[++i]);
        else if(strcmp(argv[i], "--cursor-style") == 0 && i + 1 < argc)
            config_apply_arg(config, "cursor-style", argv[++i]);
        else if(strcmp(argv[i], "--terminal-font") == 0 && i + 1 < argc)
            config_apply_arg(config, "terminal-font", argv[++i]);
        else if(strcmp(argv[i], "--terminal-foreground") == 0 && i + 1 < argc)
            config_apply_arg(config, "terminal-foreground", argv[++i]);
        else if(strcmp(argv[i], "--terminal-background") == 0 && i + 1 < argc)
            config_apply_arg(config, "terminal-background", argv[++i]);
        else if(strcmp(argv[i], "--terminal-cursor") == 0 && i + 1 < argc)
            config_apply_arg(config, "terminal-cursor", argv[++i]);
        else if(strcmp(argv[i], "--terminal-selection-foreground") == 0 &&
                i + 1 < argc)
            config_apply_arg(config, "terminal-selection-foreground",
                             argv[++i]);
        else if(strcmp(argv[i], "--terminal-selection-background") == 0 &&
                i + 1 < argc)
            config_apply_arg(config, "terminal-selection-background",
                             argv[++i]);
        else if(strcmp(argv[i], "--help") == 0) {
            printf("usage: kapsule [--working-directory PATH] [--shell PATH] "
                   "[--command CMD] [--font-size N] [--scrollback N] "
                   "[--cursor-style block|underline|bar] "
                   "[--terminal-font PATH] "
                   "[--terminal-foreground #rrggbb|default] "
                   "[--terminal-background #rrggbb|default] "
                   "[--terminal-cursor #rrggbb|default] "
                   "[--terminal-selection-foreground #rrggbb|default] "
                   "[--terminal-selection-background #rrggbb|default]\n");
            exit(0);
        }
    }
}

int main(int argc, char **argv)
{
    State app;
    int i;

    memset(&app, 0, sizeof(app));
    app.top_menu_index = -1;
    app.rename_index = -1;
    app.mouse_report_col = -1;
    app.mouse_report_row = -1;
    app.mouse_report_x = -1;
    app.mouse_report_y = -1;
    app.mouse_report_button = TERMINAL_MOUSE_RELEASE;
    app.selection_click_row = -1;
    app.selection_click_col = -1;
    config_defaults(&app.config);
    config_load(&app.config);
    parse_args(&app.config, argc, argv);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(980, 660, "Kapsule");
    snprintf(app.window_title, sizeof(app.window_title), "Kapsule");
    InitUI(frame_width(), frame_height(), 1.0f);
    app.window_focused = IsWindowFocused() ? 1 : 0;
    load_kryon_font(&app.config);
    refresh_app_theme(&app);
    app.next_theme_refresh = GetTime() + 2.0;
    SetTargetFPS(60);
    for(i = 0; i < MAX_SESSIONS; i++)
        session_init(&app.sessions[i]);
    restore_sessions(&app);
    BeginDrawing();
    ClearBackground(app.palette.background);
    BeginUIFrame(frame_width(), frame_height(), 1.0f);
    draw_starting_frame(&app);
    EndUIFrame();
    EndDrawing();

    while(!WindowShouldClose() && !app.quit_requested) {
        Session *session = active_session(&app);

        if(session == NULL && app.session_count == 0) {
            open_session(&app, NULL);
            session = active_session(&app);
        }
        if(session != NULL) {
            sync_host_clipboard_to_terminal(session);
            terminal_poll(&session->terminal);
            if(terminal_consume_bell(&session->terminal))
                app.bell_until = GetTime() + 0.18;
            session_sync_terminal_metadata(session);
        }
        sync_window_title(&app);
        if(session != NULL) {
            int focused = IsWindowFocused() ? 1 : 0;

            if(focused != app.window_focused) {
                terminal_send_focus(&session->terminal, focused);
                app.window_focused = focused;
            }
        }
        if(session != NULL)
            flush_terminal_clipboard_to_host(session);
        if(GetTime() >= app.next_theme_refresh) {
            refresh_app_theme(&app);
            app.next_theme_refresh = GetTime() + 2.0;
        }
        BeginDrawing();
        ClearBackground(app.palette.background);
        BeginUIFrame(frame_width(), frame_height(), 1.0f);
        if(session != NULL) {
            draw_terminal_view(&app, session,
                               (Rectangle){0, 0, (float)frame_width(),
                                           (float)frame_height()});
            app_handle_input(&app);
        } else {
            draw_starting_frame(&app);
        }
        EndUIFrame();
        EndDrawing();
    }

    save_sessions(&app);
    for(i = 0; i < app.session_count; i++)
        session_close(&app.sessions[i]);
    CloseWindow();
    return 0;
}
