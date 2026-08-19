#include "config.h"
#include "input.h"
#include "palette.h"
#include "session.h"
#include "terminal.h"

#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SESSIONS 8
#define COPY_BUFFER_SIZE 262144

typedef struct Selection {
    int active;
    int dragging;
    int start_row;
    int start_col;
    int end_row;
    int end_col;
} Selection;

typedef struct State {
    Config config;
    Palette palette;
    Session sessions[MAX_SESSIONS];
    int session_count;
    int active;
    Selection selection;
    Rectangle viewport;
    int first_visible_row;
    int visible_rows;
    int cell_w;
    int line_h;
} State;

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

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

static Session *active_session(State *app)
{
    if(app == NULL || app->active < 0 || app->active >= app->session_count)
        return NULL;
    return &app->sessions[app->active];
}

static const char *initial_cwd(const Config *config)
{
    if(config != NULL && config->working_directory[0] != '\0')
        return config->working_directory;
    return GetWorkingDirectory();
}

static void load_kryon_font(void)
{
    static const char *paths[] = {
        "../kryon/fonts/noto/NotoSans-Regular.ttf",
        "fonts/noto/NotoSans-Regular.ttf",
        "vendor/kryon/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    int i;

    for(i = 0; paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource("kapsule-ui", paths[i], NULL, 0) &&
           UseUIFont("kapsule-ui"))
            return;
    }
    EnsureUIDefaultFont();
}

static void open_session(State *app, const char *command)
{
    Session *session;

    if(app == NULL || app->session_count >= MAX_SESSIONS)
        return;
    session = &app->sessions[app->session_count];
    session_init(session);
    session_open(session, initial_cwd(&app->config), app->config.shell,
                 command != NULL ? command : app->config.command, 100, 30,
                 app->config.scrollback_limit);
    app->active = app->session_count;
    app->session_count++;
    app->selection.active = 0;
}

static void close_session(State *app, int index)
{
    int i;

    if(app == NULL || index < 0 || index >= app->session_count)
        return;
    session_close(&app->sessions[index]);
    for(i = index; i < app->session_count - 1; i++)
        app->sessions[i] = app->sessions[i + 1];
    app->session_count--;
    if(app->session_count <= 0) {
        open_session(app, NULL);
        return;
    }
    if(app->active >= app->session_count)
        app->active = app->session_count - 1;
    if(app->active < 0)
        app->active = 0;
    app->selection.active = 0;
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
        else if(strcmp(argv[i], "--help") == 0) {
            printf("usage: kapsule [--working-directory PATH] [--shell PATH] "
                   "[--command CMD] [--font-size N] [--scrollback N]\n");
            exit(0);
        }
    }
}

static int visible_row_from_mouse(const State *app, Vector2 mouse)
{
    int row;

    if(app == NULL || !CheckCollisionPointRec(mouse, app->viewport))
        return -1;
    row = (int)((mouse.y - app->viewport.y) / (float)app->line_h);
    if(row < 0 || row >= app->visible_rows)
        return -1;
    return app->first_visible_row + row;
}

static int visible_col_from_mouse(const State *app, Vector2 mouse)
{
    int col;

    if(app == NULL || !CheckCollisionPointRec(mouse, app->viewport))
        return -1;
    col = (int)((mouse.x - app->viewport.x) / (float)app->cell_w);
    return max_int(0, col);
}

static int selection_contains(const Selection *selection, int row, int col)
{
    int sr;
    int er;
    int sc;
    int ec;

    if(selection == NULL || !selection->active)
        return 0;
    sr = selection->start_row;
    er = selection->end_row;
    sc = selection->start_col;
    ec = selection->end_col;
    if(sr > er || (sr == er && sc > ec)) {
        int tmp;

        tmp = sr;
        sr = er;
        er = tmp;
        tmp = sc;
        sc = ec;
        ec = tmp;
    }
    if(row < sr || row > er)
        return 0;
    if(row == sr && col < sc)
        return 0;
    if(row == er && col >= ec)
        return 0;
    return 1;
}

static void copy_selection(State *app)
{
    static char buffer[COPY_BUFFER_SIZE];
    Session *session = active_session(app);
    Selection *selection = &app->selection;
    int sr;
    int er;
    int sc;
    int ec;
    int row;
    size_t used = 0;

    if(session == NULL || !selection->active)
        return;
    sr = selection->start_row;
    er = selection->end_row;
    sc = selection->start_col;
    ec = selection->end_col;
    if(sr > er || (sr == er && sc > ec)) {
        int tmp;

        tmp = sr;
        sr = er;
        er = tmp;
        tmp = sc;
        sc = ec;
        ec = tmp;
    }
    buffer[0] = '\0';
    for(row = sr; row <= er && used < sizeof(buffer) - 1; row++) {
        char line[4096];
        int len;
        int start = row == sr ? sc : 0;
        int end;
        int i;

        terminal_visible_line(&session->terminal, row, line, sizeof(line));
        len = (int)strlen(line);
        end = row == er ? ec : len;
        start = clamp_int(start, 0, len);
        end = clamp_int(end, start, len);
        for(i = start; i < end && used < sizeof(buffer) - 1; i++)
            buffer[used++] = line[i];
        if(row != er && used < sizeof(buffer) - 1)
            buffer[used++] = '\n';
    }
    buffer[used] = '\0';
    if(buffer[0] != '\0')
        SetClipboardText(buffer);
}

static void handle_selection(State *app)
{
    Vector2 mouse = GetMousePosition();
    int row = visible_row_from_mouse(app, mouse);
    int col = visible_col_from_mouse(app, mouse);

    if(row >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        app->selection.active = 1;
        app->selection.dragging = 1;
        app->selection.start_row = row;
        app->selection.end_row = row;
        app->selection.start_col = col;
        app->selection.end_col = col + 1;
    }
    if(app->selection.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       row >= 0) {
        app->selection.end_row = row;
        app->selection.end_col = col + 1;
    }
    if(app->selection.dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->selection.dragging = 0;
}

static void draw_menu_bar(State *app, Rectangle bounds)
{
    static const char *menus[] = {"File", "Edit", "View", "Terminal", "Tabs",
                                  "Help"};
    int x = (int)bounds.x + 8;
    int i;

    DrawRectangleRec(bounds, app->palette.chrome_light);
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1), app->palette.chrome_border);
    for(i = 0; i < (int)(sizeof(menus) / sizeof(menus[0])); i++) {
        DrawUIText(menus[i], x, (int)bounds.y + 6, 13,
                   app->palette.menu_text);
        x += MeasureUIText(menus[i], 13) + 22;
    }
}

static void draw_tabs(State *app, Rectangle bounds)
{
    int i;
    int x = (int)bounds.x;
    int h = (int)bounds.height;
    int tab_w;

    DrawRectangleRec(bounds, app->palette.chrome);
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1), app->palette.chrome_border);
    tab_w = max_int(110, min_int(220, (int)bounds.width / MAX_SESSIONS));
    for(i = 0; i < app->session_count; i++) {
        Rectangle tab = {(float)(x + 6 + i * (tab_w + 2)), bounds.y + 4,
                         (float)tab_w, (float)(h - 4)};
        Color bg = i == app->active ? app->palette.tab_active : app->palette.tab;
        const char *title = session_title(&app->sessions[i]);

        DrawRectangleRec(tab, bg);
        DrawRectangleLines((int)tab.x, (int)tab.y, (int)tab.width,
                           (int)tab.height, app->palette.chrome_border);
        DrawUIText(title, (int)tab.x + 10, (int)tab.y + 9, 14,
                   app->palette.menu_text);
        if(CheckCollisionPointRec(GetMousePosition(), tab) &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            app->active = i;
            app->selection.active = 0;
        }
    }
    DrawUIText("+", x + 10 + app->session_count * (tab_w + 2),
               (int)bounds.y + 7, 20, app->palette.menu_text);
    if(CheckCollisionPointRec(GetMousePosition(),
                              (Rectangle){(float)(x + app->session_count *
                                                        (tab_w + 2)),
                                          bounds.y, 40, bounds.height}) &&
       IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        open_session(app, NULL);
}

static void draw_line_cells(State *app, const Terminal *terminal, int visible_row,
                            int y)
{
    int screen_row = visible_row - terminal->scrollback_count;
    int col;

    if(screen_row < 0 || screen_row >= terminal->rows)
        return;
    for(col = 0; col < terminal->cols; col++) {
        const Cell *cell = terminal_cell(terminal, col, screen_row);
        char text[8];
        int len = 0;
        Color fg;
        Color bg;
        int selected = selection_contains(&app->selection, visible_row, col);

        if(cell == NULL)
            continue;
        fg = palette_resolve(&app->palette, cell->fg, app->palette.foreground);
        bg = palette_resolve(&app->palette, cell->bg,
                             app->palette.terminal_background);
        if((cell->style & STYLE_INVERSE) != 0) {
            Color tmp = fg;
            fg = bg;
            bg = tmp;
        }
        if(selected) {
            DrawRectangle((int)app->viewport.x + col * app->cell_w, y,
                          app->cell_w, app->line_h, app->palette.selection);
        } else if(cell->bg != COLOR_DEFAULT ||
                  (cell->style & STYLE_INVERSE) != 0) {
            DrawRectangle((int)app->viewport.x + col * app->cell_w, y,
                          app->cell_w, app->line_h, bg);
        }
        if(cell->codepoint == 0 || cell->codepoint == ' ')
            continue;
        if(cell->codepoint < 0x80) {
            text[len++] = (char)cell->codepoint;
        } else if(cell->codepoint < 0x800) {
            text[len++] = (char)(0xc0 | (cell->codepoint >> 6));
            text[len++] = (char)(0x80 | (cell->codepoint & 0x3f));
        } else if(cell->codepoint < 0x10000) {
            text[len++] = (char)(0xe0 | (cell->codepoint >> 12));
            text[len++] = (char)(0x80 | ((cell->codepoint >> 6) & 0x3f));
            text[len++] = (char)(0x80 | (cell->codepoint & 0x3f));
        } else {
            text[len++] = '?';
        }
        text[len] = '\0';
        DrawUIText(text, (int)app->viewport.x + col * app->cell_w, y,
                   app->config.font_size, fg);
        if((cell->style & STYLE_UNDERLINE) != 0)
            DrawRectangle((int)app->viewport.x + col * app->cell_w,
                          y + app->line_h - 3, app->cell_w, 1, fg);
    }
}

static void draw_scrollback_line(State *app, const Terminal *terminal,
                                 int visible_row, int y)
{
    char line[4096];
    int col;
    int len;

    terminal_visible_line(terminal, visible_row, line, sizeof(line));
    len = (int)strlen(line);
    for(col = 0; col < len; col++) {
        if(selection_contains(&app->selection, visible_row, col)) {
            DrawRectangle((int)app->viewport.x + col * app->cell_w, y,
                          app->cell_w, app->line_h, app->palette.selection);
        }
    }
    DrawUIText(line, (int)app->viewport.x, y, app->config.font_size,
               app->palette.muted);
}

static void draw_terminal_view(State *app, Session *session, Rectangle bounds)
{
    Terminal *terminal = &session->terminal;
    int menu_h = 26;
    int tab_h = 34;
    int chrome_h = menu_h + tab_h;
    int total_rows;
    int row;
    int cols;
    int max_scroll;

    app->cell_w = MeasureUIText("M", app->config.font_size);
    if(app->cell_w < 8)
        app->cell_w = app->config.font_size * 6 / 10;
    if(app->cell_w < 8)
        app->cell_w = 8;
    app->line_h = app->config.font_size + 4;
    app->viewport =
        (Rectangle){bounds.x + (float)app->config.padding,
                    bounds.y + (float)(chrome_h + app->config.padding),
                    bounds.width - (float)(app->config.padding * 2),
                    bounds.height -
                        (float)(chrome_h + app->config.padding * 2)};
    cols = (int)app->viewport.width / app->cell_w;
    app->visible_rows = (int)app->viewport.height / app->line_h;
    terminal_resize(terminal, cols, app->visible_rows);

    total_rows = terminal_visible_line_count(terminal);
    max_scroll = max_int(0, total_rows - app->visible_rows);
    session->scroll_offset = clamp_int(session->scroll_offset, 0, max_scroll);
    app->first_visible_row = max_int(0, total_rows - app->visible_rows -
                                           session->scroll_offset);

    DrawRectangleRec(bounds, app->palette.background);
    draw_menu_bar(app, (Rectangle){bounds.x, bounds.y, bounds.width,
                                   (float)menu_h});
    draw_tabs(app, (Rectangle){bounds.x, bounds.y + (float)menu_h,
                               bounds.width, (float)tab_h});
    DrawRectangleRec(app->viewport, app->palette.terminal_background);
    DrawRectangleLines((int)app->viewport.x, (int)app->viewport.y,
                       (int)app->viewport.width, (int)app->viewport.height,
                       app->palette.chrome_border);

    BeginScissorMode((int)app->viewport.x, (int)app->viewport.y,
                     (int)app->viewport.width, (int)app->viewport.height);
    for(row = 0; row < app->visible_rows; row++) {
        int visible_row = app->first_visible_row + row;
        int y = (int)app->viewport.y + row * app->line_h;

        if(visible_row < terminal->scrollback_count)
            draw_scrollback_line(app, terminal, visible_row, y);
        else
            draw_line_cells(app, terminal, visible_row, y);
    }
    if(terminal->cursor_visible && session->scroll_offset == 0 &&
       ((int)(GetTime() * 2.0) & 1) == 0) {
        int cursor_visible_row = terminal->scrollback_count + terminal->cursor_row;
        int cursor_y = cursor_visible_row - app->first_visible_row;

        if(cursor_y >= 0 && cursor_y < app->visible_rows) {
            DrawRectangle((int)app->viewport.x + terminal->cursor_col * app->cell_w,
                          (int)app->viewport.y + cursor_y * app->line_h +
                              app->line_h - 3,
                          app->cell_w, 2, app->palette.foreground);
        }
    }
    EndScissorMode();

    if(session->scroll_offset > 0) {
        char label[64];

        snprintf(label, sizeof(label), "%d lines", session->scroll_offset);
        DrawUIText(label,
                   (int)(app->viewport.x + app->viewport.width -
                         MeasureUIText(label, 13) - 8),
                   (int)app->viewport.y + 8, 13, app->palette.muted);
    }
}

static void draw_starting_frame(State *app)
{
    Rectangle bounds = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    int menu_h = 26;
    int tab_h = 34;
    int pad = app != NULL ? app->config.padding : 8;
    Rectangle viewport = {
        bounds.x + (float)pad,
        bounds.y + (float)(menu_h + tab_h + pad),
        bounds.width - (float)(pad * 2),
        bounds.height - (float)(menu_h + tab_h + pad * 2)
    };

    DrawRectangleRec(bounds, app->palette.background);
    draw_menu_bar(app, (Rectangle){bounds.x, bounds.y, bounds.width,
                                   (float)menu_h});
    draw_tabs(app, (Rectangle){bounds.x, bounds.y + (float)menu_h,
                               bounds.width, (float)tab_h});
    DrawRectangleRec(viewport, app->palette.terminal_background);
    DrawRectangleLines((int)viewport.x, (int)viewport.y,
                       (int)viewport.width, (int)viewport.height,
                       app->palette.chrome_border);
    DrawUIText("Starting terminal...", (int)viewport.x + 10,
               (int)viewport.y + 10, app->config.font_size,
               app->palette.foreground);
}

static void handle_shortcuts(State *app)
{
    Session *session = active_session(app);
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if(session == NULL)
        return;
    if(ctrl && shift && IsKeyPressed(KEY_T)) {
        open_session(app, NULL);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_W)) {
        close_session(app, app->active);
        return;
    }
    if(ctrl && IsKeyPressed(KEY_TAB)) {
        app->active = (app->active + 1) % app->session_count;
        app->selection.active = 0;
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_C)) {
        copy_selection(app);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_V)) {
        const char *text = GetClipboardText();

        terminal_send_paste(&session->terminal, text);
        session->scroll_offset = 0;
        return;
    }
}

static void handle_input(State *app)
{
    Session *session = active_session(app);
    float wheel;
    int ctrl;
    int shift;

    if(session == NULL)
        return;
    wheel = GetMouseWheelMove();
    if(CheckCollisionPointRec(GetMousePosition(), app->viewport) && wheel != 0.0f) {
        session->scroll_offset += (int)(wheel * 3.0f);
        if(session->scroll_offset < 0)
            session->scroll_offset = 0;
    }

    handle_selection(app);
    handle_shortcuts(app);

    ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if(ctrl && shift && (IsKeyDown(KEY_C) || IsKeyDown(KEY_V) ||
                         IsKeyDown(KEY_T) || IsKeyDown(KEY_W)))
        return;
    if(IsWindowFocused()) {
        if(IsKeyPressed(KEY_PAGE_UP) && shift) {
            session->scroll_offset += app->visible_rows;
            return;
        }
        if(IsKeyPressed(KEY_PAGE_DOWN) && shift) {
            session->scroll_offset -= app->visible_rows;
            if(session->scroll_offset < 0)
                session->scroll_offset = 0;
            return;
        }
        input_send_keyboard(&session->terminal);
        if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE) ||
           IsKeyPressed(KEY_TAB))
            session->scroll_offset = 0;
    }
}

int main(int argc, char **argv)
{
    State app;
    int i;

    memset(&app, 0, sizeof(app));
    config_defaults(&app.config);
    config_load(&app.config);
    parse_args(&app.config, argc, argv);
    palette_default(&app.palette);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(980, 660, "Kapsule");
    InitUI(GetScreenWidth(), GetScreenHeight(), GetWindowScaleDPI().x);
    load_kryon_font();
    palette_apply_system_theme(&app.palette);
    SetTargetFPS(60);
    for(i = 0; i < MAX_SESSIONS; i++)
        session_init(&app.sessions[i]);
    BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetWindowScaleDPI().x);
    BeginDrawing();
    ClearBackground(app.palette.background);
    draw_starting_frame(&app);
    EndDrawing();
    EndUIFrame();

    while(!WindowShouldClose()) {
        Session *session = active_session(&app);

        if(session == NULL && app.session_count == 0) {
            open_session(&app, NULL);
            session = active_session(&app);
        }
        if(session != NULL)
            terminal_poll(&session->terminal);
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetWindowScaleDPI().x);
        BeginDrawing();
        ClearBackground(app.palette.background);
        if(session != NULL) {
            draw_terminal_view(&app, session,
                               (Rectangle){0, 0, (float)GetScreenWidth(),
                                           (float)GetScreenHeight()});
            handle_input(&app);
        } else {
            draw_starting_frame(&app);
        }
        EndDrawing();
        EndUIFrame();
    }

    for(i = 0; i < app.session_count; i++)
        session_close(&app.sessions[i]);
    CloseWindow();
    return 0;
}
