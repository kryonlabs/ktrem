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

#ifdef KRYON_NATIVE_PLAN9
#include "kryon_plan9.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct CodepointBuilder {
    int *values;
    int count;
    int capacity;
} CodepointBuilder;

static int codepoints_append(CodepointBuilder *builder, int codepoint)
{
    int *next;

    if(builder == NULL || codepoint <= 0)
        return 0;
    if(builder->count >= builder->capacity) {
        int next_capacity = builder->capacity > 0 ? builder->capacity * 2 : 512;

        next = realloc(builder->values,
                       (size_t)next_capacity * sizeof(*builder->values));
        if(next == NULL)
            return 0;
        builder->values = next;
        builder->capacity = next_capacity;
    }
    builder->values[builder->count++] = codepoint;
    return 1;
}

static int codepoints_append_range(CodepointBuilder *builder, int first,
                                   int last)
{
    int codepoint;

    for(codepoint = first; codepoint <= last; codepoint++) {
        if(!codepoints_append(builder, codepoint))
            return 0;
    }
    return 1;
}

static int *terminal_codepoints(int *out_count)
{
    CodepointBuilder builder = {0};
    static const int individual[] = {
        0x1f310, /* globe */
        0x1f4a1, /* light bulb */
        0x1f600, /* grinning face */
        0x6e2c, 0x8a66
    };
    int i;

    if(out_count == NULL)
        return NULL;
    *out_count = 0;
    if(!codepoints_append_range(&builder, 0x0020, 0x007e) ||
       !codepoints_append_range(&builder, 0x00a0, 0x024f) ||
       !codepoints_append_range(&builder, 0x0300, 0x036f) ||
       !codepoints_append_range(&builder, 0x0370, 0x03ff) ||
       !codepoints_append_range(&builder, 0x0400, 0x052f) ||
       !codepoints_append_range(&builder, 0x2000, 0x206f) ||
       !codepoints_append_range(&builder, 0x2070, 0x209f) ||
       !codepoints_append_range(&builder, 0x20a0, 0x20cf) ||
       !codepoints_append_range(&builder, 0x2100, 0x214f) ||
       !codepoints_append_range(&builder, 0x2150, 0x218f) ||
       !codepoints_append_range(&builder, 0x2190, 0x21ff) ||
       !codepoints_append_range(&builder, 0x2200, 0x22ff) ||
       !codepoints_append_range(&builder, 0x2300, 0x23ff) ||
       !codepoints_append_range(&builder, 0x2400, 0x243f) ||
       !codepoints_append_range(&builder, 0x2460, 0x24ff) ||
       !codepoints_append_range(&builder, 0x2500, 0x259f) ||
       !codepoints_append_range(&builder, 0x25a0, 0x25ff) ||
       !codepoints_append_range(&builder, 0x2600, 0x27bf) ||
       !codepoints_append_range(&builder, 0x2800, 0x28ff) ||
       !codepoints_append_range(&builder, 0x2b00, 0x2bff) ||
       !codepoints_append_range(&builder, 0x1f300, 0x1f5ff) ||
       !codepoints_append_range(&builder, 0x1f600, 0x1f64f) ||
       !codepoints_append_range(&builder, 0xe0a0, 0xe0ff) ||
       !codepoints_append_range(&builder, 0xe700, 0xe7c5) ||
       !codepoints_append_range(&builder, 0xf000, 0xf2ff)) {
        free(builder.values);
        return NULL;
    }
    for(i = 0; i < (int)(sizeof(individual) / sizeof(individual[0])); i++) {
        if(!codepoints_append(&builder, individual[i])) {
            free(builder.values);
            return NULL;
        }
    }
    *out_count = builder.count;
    return builder.values;
}

static int *terminal_cjk_codepoints(int *out_count)
{
    CodepointBuilder builder = {0};
    static const int individual[] = {
        0x6e2c, /* CJK sample: test */
        0x8a66
    };
    int i;

    if(out_count == NULL)
        return NULL;
    *out_count = 0;
    if(!codepoints_append_range(&builder, 0x3040, 0x309f) ||
       !codepoints_append_range(&builder, 0x30a0, 0x30ff) ||
       !codepoints_append_range(&builder, 0x3400, 0x34ff) ||
       !codepoints_append_range(&builder, 0x4e00, 0x52ff) ||
       !codepoints_append_range(&builder, 0x6e00, 0x6eff) ||
       !codepoints_append_range(&builder, 0x8a00, 0x8aff)) {
        free(builder.values);
        return NULL;
    }
    for(i = 0; i < (int)(sizeof(individual) / sizeof(individual[0])); i++) {
        if(!codepoints_append(&builder, individual[i])) {
            free(builder.values);
            return NULL;
        }
    }
    *out_count = builder.count;
    return builder.values;
}

#ifdef KRYON_NATIVE_PLAN9
static const char *native_rill_control_path(void)
{
    const char *path;

    path = getenv("rillctl");
    if(path != NULL && path[0] != '\0')
        return path;
    path = getenv("RILLCTL");
    if(path != NULL && path[0] != '\0')
        return path;
    path = getenv("RILL_COMMAND_FILE");
    if(path != NULL && path[0] != '\0')
        return path;
    return NULL;
}

static int native_request_rill_terminal(void)
{
    const char *path;
    FILE *file;

    path = native_rill_control_path();
    if(path == NULL)
        return 0;
    file = fopen(path, "a");
    if(file == NULL)
        return 0;
    fprintf(file, "open kterm\n");
    fclose(file);
    return 1;
}

static int native_can_open(const char *path, int mode)
{
    int fd;

    fd = open((char *)path, mode);
    if(fd < 0)
        return 0;
    close(fd);
    return 1;
}

static int native_graphics_namespace_ready(void)
{
    return native_can_open("/dev/draw/new", OREAD) &&
           native_can_open("/dev/mouse", OREAD);
}
#endif

static int path_in_list(const char *const *paths, int count, const char *path)
{
    int i;

    if(path == NULL || path[0] == '\0')
        return 1;
    for(i = 0; i < count; i++) {
        if(paths[i] != NULL && strcmp(paths[i], path) == 0)
            return 1;
    }
    return 0;
}

static int fontconfig_match_charset(unsigned int codepoint, char *out,
                                    int out_size)
{
#if !defined(_WIN32) && !defined(PLATFORM_WEB) && \
    !defined(KRYON_NATIVE_PLAN9)
    char command[96];
    char line[512];
    FILE *pipe;
    size_t len;

    if(out == NULL || out_size <= 0 || codepoint == 0)
        return 0;
    snprintf(command, sizeof(command),
             "fc-match -f '%%{file}\\n' ':charset=%x'", codepoint);
    pipe = popen(command, "r");
    if(pipe == NULL)
        return 0;
    if(fgets(line, sizeof(line), pipe) == NULL) {
        pclose(pipe);
        return 0;
    }
    pclose(pipe);
    len = strlen(line);
    while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                      line[len - 1] == ' ' || line[len - 1] == '\t'))
        line[--len] = '\0';
    if(len == 0 || len >= (size_t)out_size || access(line, R_OK) != 0)
        return 0;
    snprintf(out, (size_t)out_size, "%s", line);
    return 1;
#else
    (void)codepoint;
    (void)out;
    (void)out_size;
    return 0;
#endif
}

static void register_terminal_fallback(
    const char *name, const char *const *static_paths, int static_count,
    unsigned int match_codepoint, const int *codepoints, int codepoint_count)
{
    const char *tried[16];
    int tried_count = 0;
    char matched[512];
    int i;

    for(i = 0; i < static_count && static_paths[i] != NULL; i++) {
        const char *path = static_paths[i];

        if(path_in_list(tried, tried_count, path))
            continue;
        if(tried_count < (int)(sizeof(tried) / sizeof(tried[0])))
            tried[tried_count++] = path;
        if(RegisterUIFontFileSource(name, path, codepoints, codepoint_count))
            return;
    }
    if(fontconfig_match_charset(match_codepoint, matched, sizeof(matched)) &&
       !path_in_list(tried, tried_count, matched))
        (void)RegisterUIFontFileSource(name, matched, codepoints,
                                       codepoint_count);
}

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

static int initial_window_width(const LaunchOptions *launch)
{
    if(launch != NULL && launch->geometry_cols > 0)
        return launch->geometry_cols * 10 + 20;
    return 980;
}

static int initial_window_height(const LaunchOptions *launch)
{
    if(launch != NULL && launch->drop_down)
        return 420;
    if(launch != NULL && launch->geometry_rows > 0)
        return launch->geometry_rows * 20 + 78;
    return 660;
}

static void set_launch_window_flags(const LaunchOptions *launch)
{
    unsigned int flags = FLAG_WINDOW_RESIZABLE;

    if(launch != NULL && launch->drop_down)
        flags |= FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST |
                 FLAG_WINDOW_ALWAYS_RUN;
    else if(launch != NULL && !launch->show_borders)
        flags |= FLAG_WINDOW_UNDECORATED;
    SetConfigFlags(flags);
}

static void apply_launch_window_state(const LaunchOptions *launch)
{
    int monitor;
    int monitor_w;
    int monitor_h;
    int width;
    int height;
    Vector2 pos;

    if(launch == NULL)
        return;
    if(launch->drop_down) {
        monitor = GetCurrentMonitor();
        monitor_w = GetMonitorWidth(monitor);
        monitor_h = GetMonitorHeight(monitor);
        pos = GetMonitorPosition(monitor);
        width = monitor_w > 0 ? monitor_w : GetScreenWidth();
        height = monitor_h > 0 ? monitor_h * 45 / 100 : 420;
        if(height < 280)
            height = 280;
        if(height > 720)
            height = 720;
        SetWindowState(FLAG_WINDOW_TOPMOST);
        SetWindowSize(width, height);
        SetWindowPosition((int)pos.x, (int)pos.y);
        return;
    }
    if(launch->maximize)
        MaximizeWindow();
    if(launch->fullscreen)
        ToggleFullscreen();
}

static int target_fps(void)
{
    const char *value = getenv("KAPSULE_TARGET_FPS");
    char *end = NULL;
    long fps;

    if(value == NULL || value[0] == '\0')
#if defined(KRYON_NATIVE_PLAN9)
        return 30;
#else
        return 60;
#endif
    fps = strtol(value, &end, 10);
    if(end == value || fps < 0 || fps > 1000)
        return 60;
    return (int)fps;
}

static int active_target_fps(int idle_fps)
{
    const char *value = getenv("KAPSULE_ACTIVE_FPS");
    char *end = NULL;
    long fps;

    if(value == NULL || value[0] == '\0')
#if defined(KRYON_NATIVE_PLAN9)
        return idle_fps < 60 ? 60 : idle_fps;
#else
        return idle_fps < 1000 ? 1000 : idle_fps;
#endif
    fps = strtol(value, &end, 10);
    if(end == value || fps < 0 || fps > 1000)
#if defined(KRYON_NATIVE_PLAN9)
        return idle_fps < 60 ? 60 : idle_fps;
#else
        return idle_fps < 1000 ? 1000 : idle_fps;
#endif
    if(fps > 0 && fps < idle_fps)
        return idle_fps;
    return (int)fps;
}

static int pty_burst_ms(void)
{
    const char *value = getenv("KAPSULE_PTY_BURST_MS");
    char *end = NULL;
    long ms;

    if(value == NULL || value[0] == '\0')
#if defined(KRYON_NATIVE_PLAN9)
        return 0;
#else
        return 8;
#endif
    ms = strtol(value, &end, 10);
    if(end == value || ms < 0 || ms > 20)
#if defined(KRYON_NATIVE_PLAN9)
        return 0;
#else
        return 8;
#endif
    return (int)ms;
}

static int drain_terminal_output(Session *session, int burst_ms)
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

static int app_interaction_active(const State *app)
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
    static const char *cjk_paths[] = {
        "/usr/share/fonts/truetype/fonts-ukij-uyghur/UKIJCJK.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        NULL
    };
    static const char *symbol_paths[] = {
        "/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf",
        "/usr/share/fonts/opentype/urw-base35/StandardSymbolsPS.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        NULL
    };
    static const char *emoji_paths[] = {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        NULL
    };
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
    int codepoint_count = 0;
    int *codepoints = terminal_codepoints(&codepoint_count);
    int cjk_codepoint_count = 0;
    int *cjk_codepoints = terminal_cjk_codepoints(&cjk_codepoint_count);
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
                                codepoints, codepoint_count))
        goto fallbacks;
    for(i = 0; terminal_paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource("kapsule-terminal", terminal_paths[i],
                                    codepoints, codepoint_count))
            break;
    }
fallbacks:
    register_terminal_fallback("kapsule-terminal-cjk", cjk_paths,
                               (int)(sizeof(cjk_paths) / sizeof(cjk_paths[0])),
                               0x6e2c, cjk_codepoints, cjk_codepoint_count);
    register_terminal_fallback(
        "kapsule-terminal-symbols", symbol_paths,
        (int)(sizeof(symbol_paths) / sizeof(symbol_paths[0])), 0x2800,
        codepoints, codepoint_count);
    register_terminal_fallback(
        "kapsule-terminal-emoji", emoji_paths,
        (int)(sizeof(emoji_paths) / sizeof(emoji_paths[0])), 0x1f600,
        codepoints, codepoint_count);
    (void)UseUIFont("kapsule-terminal");
    free(cjk_codepoints);
    free(codepoints);
}

int main(int argc, char **argv)
{
    State app;
    int i;
    int idle_fps;
    int busy_fps;
    int burst_ms;
    double fast_poll_until = 0.0;
    char arg_error[256];
    LaunchParseResult parse_result;

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
    launch_options_defaults(&app.launch);
    config_load(&app.config);
    parse_result = launch_options_parse(&app.launch, &app.config, argc, argv,
                                        arg_error, (int)sizeof(arg_error));
    if(parse_result == LAUNCH_PARSE_HELP) {
        launch_options_print_usage();
        return 0;
    }
    if(parse_result == LAUNCH_PARSE_VERSION) {
        launch_options_print_version();
        return 0;
    }
    if(parse_result == LAUNCH_PARSE_COLOR_TABLE) {
        launch_options_print_color_table();
        return 0;
    }
    if(parse_result == LAUNCH_PARSE_ERROR) {
        fprintf(stderr, "%s\n", arg_error[0] != '\0' ? arg_error :
                "invalid launch options");
        return 2;
    }
#ifdef KRYON_NATIVE_PLAN9
    if(native_request_rill_terminal())
        return 0;
    if(!native_graphics_namespace_ready()) {
        fprintf(stderr,
                "kterm: no Plan 9 graphics namespace; open kterm from Rill\n");
        return 1;
    }
#endif
    SetTraceLogLevel(LOG_WARNING);
    set_launch_window_flags(&app.launch);
    InitWindow(initial_window_width(&app.launch),
               initial_window_height(&app.launch), "Kapsule");
    SetExitKey(KEY_NULL);
    if(!IsWindowReady() || GetFontDefault().texture.id == 0) {
        fprintf(stderr, "kapsule: graphics backend failed to initialize\n");
        return 1;
    }
    apply_launch_window_state(&app.launch);
    snprintf(app.window_title, sizeof(app.window_title), "Kapsule");
    InitUI(frame_width(), frame_height(), 1.0f);
    app.window_focused = IsWindowFocused() ? 1 : 0;
    load_kryon_font(&app.config);
    refresh_app_theme(&app);
    app.next_theme_refresh = GetTime() + 2.0;
    idle_fps = target_fps();
    busy_fps = active_target_fps(idle_fps);
    burst_ms = pty_burst_ms();
    SetTargetFPS(idle_fps);
    for(i = 0; i < MAX_SESSIONS; i++)
        session_init(&app.sessions[i]);
    if(!open_launch_sessions(&app))
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
            int bytes;

            sync_host_clipboard_to_terminal(session);
            bytes = drain_terminal_output(session, burst_ms);
            if(bytes > 0)
                fast_poll_until = GetTime() + 0.25;
            SetTargetFPS(GetTime() < fast_poll_until ? busy_fps : idle_fps);
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
            if(app_interaction_active(&app))
                fast_poll_until = GetTime() + 0.25;
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
