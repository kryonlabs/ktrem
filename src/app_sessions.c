#include "app_sessions.h"

#include "profile.h"
#include "session_store.h"

#include "kryon.h"

#include <stdio.h>

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

Session *active_session(State *app)
{
    if(app == NULL || app->active < 0 || app->active >= app->session_count)
        return NULL;
    return &app->sessions[app->active];
}

void set_active_session(State *app, int index)
{
    int old;

    if(app == NULL || index < 0 || index >= app->session_count)
        return;
    old = app->active;
    if(old == index) {
        app->selection.active = 0;
        return;
    }
    if(app->window_focused && old >= 0 && old < app->session_count)
        terminal_send_focus(&app->sessions[old].terminal, 0);
    app->active = index;
    if(app->window_focused)
        terminal_send_focus(&app->sessions[index].terminal, 1);
    app->selection.active = 0;
}

static const char *initial_cwd(const Config *config)
{
    if(config != NULL && config->working_directory[0] != '\0')
        return config->working_directory;
    return GetWorkingDirectory();
}

static void estimate_terminal_grid(State *app, int *cols, int *rows)
{
    int menu_h = ScaleUIPx(34);
    int tab_h = TabBarHeight();
    int chrome_h = menu_h + tab_h;
    TerminalPaneMetrics metrics;

    if(cols == NULL || rows == NULL)
        return;
    *cols = 100;
    *rows = 30;
    if(app == NULL)
        return;
    if(app->launch.geometry_cols > 0 && app->launch.geometry_rows > 0) {
        *cols = clamp_int(app->launch.geometry_cols, 8, MAX_COLS);
        *rows = clamp_int(app->launch.geometry_rows, 4, MAX_ROWS);
        return;
    }
    UseUIFont("ktrem-terminal");
    metrics = MeasureTerminalPaneContent(
        TerminalPaneContentBounds(
            (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
            chrome_h, 0),
        app->config.font_size);
    UseUIFont("ktrem-ui");
    *cols = clamp_int(metrics.cols, 8, MAX_COLS);
    *rows = clamp_int(metrics.rows, 4, MAX_ROWS);
}

static void apply_profile_defaults_to_terminal(State *app,
                                               TerminalState *terminal)
{
    if(app == NULL || terminal == NULL)
        return;
    terminal_profile_apply_new(&app->config, &app->palette, terminal);
}

void seed_theme_defaults_to_terminal(State *app, TerminalState *terminal)
{
    if(app == NULL || terminal == NULL)
        return;
    terminal_profile_seed_missing(&app->config, &app->palette, terminal);
}

static void open_session_with_launch(State *app, const char *cwd,
                                     const char *command, const char *title)
{
    Session *session;
    int cols = 100;
    int rows = 30;
    int new_index;

    if(app == NULL || app->session_count >= MAX_SESSIONS)
        return;
    new_index = app->session_count;
    estimate_terminal_grid(app, &cols, &rows);
    session = &app->sessions[new_index];
    session_init(session);
    session_open(session,
                 cwd != NULL && cwd[0] != '\0' ? cwd :
                     initial_cwd(&app->config),
                 app->config.shell,
                 command != NULL ? command : app->config.command, cols, rows,
                 app->config.scrollback_limit);
    if(title != NULL && title[0] != '\0')
        session_set_title(session, title);
    apply_profile_defaults_to_terminal(app, &session->terminal);
    app->session_count++;
    set_active_session(app, new_index);
    app->rename_index = -1;
}

void open_session(State *app, const char *command)
{
    const char *title = NULL;

    if(app != NULL && app->session_count == 0 &&
       app->launch.initial_title[0] != '\0')
        title = app->launch.initial_title;
    open_session_with_launch(app, NULL, command, title);
}

int open_launch_sessions(State *app)
{
    int i;
    int opened = 0;

    if(app == NULL || app->launch.tab_count <= 0)
        return 0;
    for(i = 0; i < app->launch.tab_count && app->session_count < MAX_SESSIONS;
        i++) {
        LaunchTabSpec *tab = &app->launch.tabs[i];

        open_session_with_launch(app, tab->working_directory, tab->command,
                                 tab->title);
        opened++;
    }
    return opened;
}

void close_session(State *app, int index)
{
    int i;
    int closing_active;

    if(app == NULL || index < 0 || index >= app->session_count)
        return;
    closing_active = index == app->active;
    if(closing_active && app->window_focused)
        terminal_send_focus(&app->sessions[index].terminal, 0);
    session_close(&app->sessions[index]);
    for(i = index; i < app->session_count - 1; i++)
        app->sessions[i] = app->sessions[i + 1];
    app->session_count--;
    if(app->session_count <= 0) {
        open_session(app, NULL);
        return;
    }
    if(!closing_active && index < app->active)
        app->active--;
    if(app->active >= app->session_count)
        app->active = app->session_count - 1;
    if(app->active < 0)
        app->active = 0;
    if(closing_active && app->window_focused)
        terminal_send_focus(&app->sessions[app->active].terminal, 1);
    app->selection.active = 0;
    app->rename_index = -1;
}

void save_sessions(State *app)
{
    if(app == NULL)
        return;
    session_store_save(app->sessions, app->session_count, app->active);
}

int restore_sessions(State *app)
{
    SessionRecord records[MAX_SESSIONS];
    int restored = 0;
    int active = 0;
    int i;
    int cols = 100;
    int rows = 30;

    if(app == NULL || app->session_count > 0 || app->config.command[0] != '\0' ||
       (restored = session_store_load(records, MAX_SESSIONS, &active)) <= 0)
        return 0;
    estimate_terminal_grid(app, &cols, &rows);
    for(i = 0; i < restored; i++) {
        SessionRecord *record = &records[i];

        if(record->cwd[0] == '\0')
            snprintf(record->cwd, sizeof(record->cwd), "%s",
                     initial_cwd(&app->config));
        if(record->shell[0] == '\0')
            snprintf(record->shell, sizeof(record->shell), "%s",
                     app->config.shell);
        session_init(&app->sessions[i]);
        session_open(&app->sessions[i], record->cwd, record->shell,
                     record->command, cols, rows,
                     app->config.scrollback_limit);
        app->sessions[i].scroll_offset = max_int(0, record->scroll_offset);
        apply_profile_defaults_to_terminal(app, &app->sessions[i].terminal);
        if(record->title[0] != '\0')
            session_restore_title(&app->sessions[i], record->title,
                                  record->title_override);
    }
    app->session_count = restored;
    app->active = clamp_int(active, 0, restored - 1);
    app->selection.active = 0;
    app->rename_index = -1;
    return 1;
}
