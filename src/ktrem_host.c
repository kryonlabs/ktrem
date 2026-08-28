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
#include <unistd.h>

#ifndef KTREM_KRYON_FONT_PATH
#define KTREM_KRYON_FONT_PATH "fonts/noto/NotoSans-Regular.ttf"
#endif

typedef struct KtremHost {
    AppHost host;
    State app;
    AppScreenInfo screen;
    int focused;
    int width;
    int height;
    int initialized;
    int burst_ms;
    double fast_poll_until;
} KtremHost;

static int
target_fps(void)
{
    const char *value = getenv("KTREM_TARGET_FPS");
    char *end = NULL;
    long fps;

    if(value == NULL || value[0] == '\0')
        return 60;
    fps = strtol(value, &end, 10);
    if(end == value || fps < 0 || fps > 1000)
        return 60;
    return (int)fps;
}

static int
pty_burst_ms(void)
{
    const char *value = getenv("KTREM_PTY_BURST_MS");
    char *end = NULL;
    long ms;

    if(value == NULL || value[0] == '\0')
        return 8;
    ms = strtol(value, &end, 10);
    if(end == value || ms < 0 || ms > 20)
        return 8;
    return (int)ms;
}

static int
drain_terminal_output(Session *session, int burst_ms)
{
    int bytes;
    int empty_polls = 0;
    double deadline;

    if(session == NULL)
        return 0;
    bytes = terminal_poll_bytes(&session->terminal);
    if(bytes <= 0 || burst_ms <= 0)
        return bytes;
    deadline = GetTime() + (double)burst_ms / 1000.0;
    while(GetTime() < deadline) {
        int more = terminal_poll_bytes(&session->terminal);

        if(more > 0) {
            bytes += more;
            empty_polls = 0;
            continue;
        }
        empty_polls++;
        if(empty_polls >= 2)
            break;
        usleep(250);
    }
    return bytes;
}

static int
app_interaction_active(const State *app)
{
    if(app == NULL)
        return 0;
    if(app->search_visible || app->context_menu_open ||
       app->rename_index >= 0 || app->about_visible ||
       app->profile_prompt != PROFILE_PROMPT_NONE)
        return 1;
    if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
       IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ||
       IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
        return 1;
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
       IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
       IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        return 1;
    return 0;
}

static void
refresh_app_theme(State *app)
{
    TerminalProfileColors old_colors;

    if(app == NULL)
        return;
    old_colors = terminal_profile_colors(&app->config, &app->palette);
    palette_default(&app->palette);
    palette_apply_system_theme(&app->palette);
    sync_terminal_theme_defaults(app, old_colors);
}

static void
load_kryon_font(const Config *config)
{
    static const char *ui_paths[] = {
        KTREM_KRYON_FONT_PATH,
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
        if(RegisterUIFontFileSource("ktrem-ui", ui_paths[i], NULL, 0) &&
           UseUIFont("ktrem-ui")) {
            ui_loaded = 1;
            break;
        }
    }
    if(!ui_loaded)
        EnsureUIDefaultFont();
    if(config != NULL && config->terminal_font[0] != '\0' &&
       RegisterUIFontFileSource("ktrem-terminal", config->terminal_font,
                                NULL, 0))
        goto done;
    for(i = 0; terminal_paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource("ktrem-terminal", terminal_paths[i],
                                    NULL, 0))
            break;
    }
done:
    (void)UseUIFont("ktrem-terminal");
}

static void
ktrem_state_init(KtremHost *host)
{
    State *app = &host->app;
    int i;

    memset(app, 0, sizeof(*app));
    app->top_menu_index = -1;
    app->rename_index = -1;
    app->mouse_report_col = -1;
    app->mouse_report_row = -1;
    app->mouse_report_x = -1;
    app->mouse_report_y = -1;
    app->mouse_report_button = TERMINAL_MOUSE_RELEASE;
    app->selection_click_row = -1;
    app->selection_click_col = -1;
    config_defaults(&app->config);
    launch_options_defaults(&app->launch);
    config_load(&app->config);
    app->config.command[0] = '\0';
    snprintf(app->window_title, sizeof(app->window_title), "ktrem");
    app->window_focused = 0;
    load_kryon_font(&app->config);
    refresh_app_theme(app);
    app->next_theme_refresh = GetTime() + 2.0;
    host->burst_ms = pty_burst_ms();
    SetTargetFPS(target_fps());
    for(i = 0; i < MAX_SESSIONS; i++)
        session_init(&app->sessions[i]);
    if(!open_launch_sessions(app))
        open_session(app, NULL);
    host->initialized = 1;
}

static int
ktrem_screen_count(void *userdata)
{
    (void)userdata;
    return 1;
}

static AppScreenInfo
ktrem_screen(void *userdata, int index)
{
    KtremHost *host = userdata;
    AppScreenInfo empty = {0};

    if(host == NULL || index != 0)
        return empty;
    return host->screen;
}

static void
ktrem_select_screen(void *userdata, int index)
{
    (void)userdata;
    (void)index;
}

static int
ktrem_select_source_path(void *userdata, const char *source_path)
{
    (void)userdata;
    (void)source_path;
    return 0;
}

static void
ktrem_resize(void *userdata, int width, int height)
{
    KtremHost *host = userdata;

    if(host == NULL)
        return;
    host->width = width;
    host->height = height;
}

static void
ktrem_set_focused(void *userdata, int focused)
{
    KtremHost *host = userdata;
    Session *session;

    if(host == NULL)
        return;
    if(!host->initialized)
        ktrem_state_init(host);
    if(host->focused == (focused != 0))
        return;
    host->focused = focused != 0;
    host->app.window_focused = host->focused;
    session = active_session(&host->app);
    if(session != NULL)
        terminal_send_focus(&session->terminal, host->focused);
}

static void
ktrem_opaque_draw_rectangle_rec(Rectangle rect, Color color)
{
    color.a = 255;
    DrawRectangleRec(rect, color);
}

static void
ktrem_draw(void *userdata, Rectangle viewport)
{
    KtremHost *host = userdata;
    State *app;
    Session *session;

    if(host == NULL)
        return;
    if(!host->initialized)
        ktrem_state_init(host);
    app = &host->app;
    if(app->session_count == 0)
        open_session(app, NULL);
    session = active_session(app);
    if(session != NULL) {
        int bytes;

        sync_host_clipboard_to_terminal(session);
        bytes = drain_terminal_output(session, host->burst_ms);
        if(bytes > 0)
            host->fast_poll_until = GetTime() + 0.25;
        if(terminal_consume_bell(&session->terminal))
            app->bell_until = GetTime() + 0.18;
        session_sync_terminal_metadata_with_mode(
            session, app->config.dynamic_title_mode);
        flush_terminal_clipboard_to_host(session);
    }
    if(GetTime() >= app->next_theme_refresh) {
        refresh_app_theme(app);
        app->next_theme_refresh = GetTime() + 2.0;
    }
    ktrem_opaque_draw_rectangle_rec(viewport, app->palette.background);
    app_update_auto_hide_mouse(app);
    if(session != NULL) {
        BeginScissorMode((int)viewport.x, (int)viewport.y,
                         (int)viewport.width, (int)viewport.height);
        draw_terminal_view(app, session, viewport);
        if(host->focused) {
            app_handle_input(app);
            if(app_interaction_active(app))
                host->fast_poll_until = GetTime() + 0.25;
        }
        EndScissorMode();
    } else {
        draw_starting_frame(app);
    }
}

AppHost *
CreateAppHost(int abi_version, const char *project_path)
{
    KtremHost *host;

    (void)project_path;
    if(abi_version != APP_HOST_ABI_VERSION)
        return NULL;
    host = calloc(1, sizeof(*host));
    if(host == NULL)
        return NULL;
    host->screen.id = "terminal";
    host->screen.group = "Applications";
    host->screen.title = "ktrem";
    host->host.userdata = host;
    host->host.screen_count = ktrem_screen_count;
    host->host.screen = ktrem_screen;
    host->host.select_screen = ktrem_select_screen;
    host->host.select_source_path = ktrem_select_source_path;
    host->host.draw = ktrem_draw;
    host->host.resize = ktrem_resize;
    host->host.set_focused = ktrem_set_focused;
    return &host->host;
}

void
DestroyAppHost(AppHost *app_host)
{
    KtremHost *host = (KtremHost *)app_host;
    int i;

    if(host == NULL)
        return;
    if(host->initialized) {
        save_sessions(&host->app);
        for(i = 0; i < host->app.session_count; i++)
            session_close(&host->app.sessions[i]);
        release_terminal_view_resources(&host->app);
        if(host->app.mouse_hidden)
            ShowCursor();
    }
    free(host);
}
