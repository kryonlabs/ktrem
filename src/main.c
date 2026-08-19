#include "config.h"
#include "input.h"
#include "palette.h"
#include "session.h"
#include "terminal.h"

#include "kryon.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_SESSIONS 8
#define COPY_BUFFER_SIZE 262144

enum {
    MENU_NEW_TAB = 1001,
    MENU_CLOSE_TAB,
    MENU_QUIT,
    MENU_COPY,
    MENU_PASTE,
    MENU_SELECT_ALL,
    MENU_FONT_INCREASE,
    MENU_FONT_DECREASE,
    MENU_FONT_RESET,
    MENU_NEXT_TAB,
    MENU_PREVIOUS_TAB,
    MENU_FIND,
    MENU_FIND_NEXT,
    MENU_FIND_PREVIOUS,
    MENU_CONTEXT_COPY,
    MENU_CONTEXT_PASTE,
    MENU_CONTEXT_FIND,
    MENU_CONTEXT_FIND_NEXT,
    MENU_CONTEXT_FIND_PREVIOUS,
    MENU_CONTEXT_SELECT_ALL,
    MENU_PROFILE_SHELL,
    MENU_PROFILE_CWD,
    MENU_PROFILE_FONT_FILE,
    MENU_PROFILE_FONT_SIZE,
    MENU_PROFILE_SCROLLBACK,
    MENU_PROFILE_FOREGROUND,
    MENU_PROFILE_BACKGROUND,
    MENU_CURSOR_BLOCK,
    MENU_CURSOR_UNDERLINE,
    MENU_CURSOR_BAR,
    MENU_ABOUT
};

enum {
    PROFILE_PROMPT_NONE,
    PROFILE_PROMPT_SHELL,
    PROFILE_PROMPT_CWD,
    PROFILE_PROMPT_FONT_FILE,
    PROFILE_PROMPT_FONT_SIZE,
    PROFILE_PROMPT_SCROLLBACK,
    PROFILE_PROMPT_FOREGROUND,
    PROFILE_PROMPT_BACKGROUND
};

typedef struct Selection {
    int active;
    int dragging;
    int mode;
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
    int top_menu_index;
    int tab_scroll;
    int quit_requested;
    int window_focused;
    int about_visible;
    int rename_index;
    int rename_cursor;
    int rename_focused;
    char rename_text[128];
    int search_visible;
    int search_cursor;
    int search_focused;
    char search_text[128];
    int mouse_report_col;
    int mouse_report_row;
    int mouse_report_button;
    double selection_click_time;
    int selection_click_row;
    int selection_click_col;
    int selection_click_count;
    int context_menu_open;
    int context_menu_x;
    int context_menu_y;
    char primary_selection[COPY_BUFFER_SIZE];
    int profile_prompt;
    int profile_cursor;
    int profile_focused;
    char profile_text[1024];
    char window_title[160];
} State;

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

static const char *initial_cwd(const Config *config)
{
    if(config != NULL && config->working_directory[0] != '\0')
        return config->working_directory;
    return GetWorkingDirectory();
}

static void color_text(int color, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return;
    if(color == COLOR_DEFAULT)
        snprintf(out, (size_t)out_size, "default");
    else if((color & COLOR_TRUE_RGB) != 0)
        snprintf(out, (size_t)out_size, "#%06x", color & 0xffffff);
    else
        snprintf(out, (size_t)out_size, "default");
}

static int color_to_terminal_rgb(Color color)
{
    return COLOR_TRUE_RGB | ((int)color.r << 16) | ((int)color.g << 8) |
           (int)color.b;
}

static int effective_terminal_foreground(const State *app)
{
    if(app == NULL)
        return COLOR_DEFAULT;
    if(app->config.terminal_foreground != COLOR_DEFAULT)
        return app->config.terminal_foreground;
    return color_to_terminal_rgb(app->palette.foreground);
}

static int effective_terminal_background(const State *app)
{
    if(app == NULL)
        return COLOR_DEFAULT;
    if(app->config.terminal_background != COLOR_DEFAULT)
        return app->config.terminal_background;
    return color_to_terminal_rgb(app->palette.terminal_background);
}

static void apply_profile_defaults_to_terminal(State *app,
                                               TerminalState *terminal)
{
    if(app == NULL || terminal == NULL)
        return;
    terminal->base_fg = effective_terminal_foreground(app);
    terminal->base_bg = effective_terminal_background(app);
    terminal->base_cursor_color = terminal->base_fg;
    terminal->default_fg = effective_terminal_foreground(app);
    terminal->default_bg = effective_terminal_background(app);
    if(terminal->cursor_color == COLOR_DEFAULT)
        terminal->cursor_color = terminal->base_cursor_color;
}

static void seed_theme_defaults_to_terminal(State *app, TerminalState *terminal)
{
    if(app == NULL || terminal == NULL)
        return;
    terminal->base_fg = effective_terminal_foreground(app);
    terminal->base_bg = effective_terminal_background(app);
    terminal->base_cursor_color = terminal->base_fg;
    if(terminal->default_fg == COLOR_DEFAULT)
        terminal->default_fg = terminal->base_fg;
    if(terminal->default_bg == COLOR_DEFAULT)
        terminal->default_bg = terminal->base_bg;
    if(terminal->cursor_color == COLOR_DEFAULT)
        terminal->cursor_color = terminal->base_cursor_color;
}

static int state_dir(char *path, int path_size)
{
    const char *xdg = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");

    if(path == NULL || path_size <= 0)
        return 0;
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/kapsule", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/.local/state/kapsule", home);
    else
        return 0;
    return 1;
}

static int session_state_path(char *path, int path_size)
{
    char dir[1024];
    int len;

    if(path == NULL || path_size <= 0 || !state_dir(dir, sizeof(dir)))
        return 0;
    len = (int)strlen(dir);
    if(len <= 0 || len + 8 >= path_size)
        return 0;
    snprintf(path, (size_t)path_size, "%s/session", dir);
    return 1;
}

static void ensure_state_dir(void)
{
    char dir[1024];
    char partial[1024];
    int i;

    if(state_dir(dir, sizeof(dir)))
        snprintf(partial, sizeof(partial), "%s", dir);
    else
        return;
    for(i = 1; partial[i] != '\0'; i++) {
        if(partial[i] == '/') {
            partial[i] = '\0';
            mkdir(partial, 0700);
            partial[i] = '/';
        }
    }
    mkdir(partial, 0700);
}

static void write_escaped(FILE *file, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if(file == NULL)
        return;
    if(cursor == NULL)
        cursor = (const unsigned char *)"";
    while(*cursor != '\0') {
        if(*cursor == '\\')
            fputs("\\\\", file);
        else if(*cursor == '\t')
            fputs("\\t", file);
        else if(*cursor == '\n')
            fputs("\\n", file);
        else if(*cursor == '\r')
            fputs("\\r", file);
        else
            fputc(*cursor, file);
        cursor++;
    }
}

static void read_escaped(char *dst, int dst_size, const char *src)
{
    int used = 0;

    if(dst == NULL || dst_size <= 0)
        return;
    dst[0] = '\0';
    if(src == NULL)
        return;
    while(*src != '\0' && used < dst_size - 1) {
        if(*src == '\\' && src[1] != '\0') {
            src++;
            if(*src == 't')
                dst[used++] = '\t';
            else if(*src == 'n')
                dst[used++] = '\n';
            else if(*src == 'r')
                dst[used++] = '\r';
            else
                dst[used++] = *src;
        } else {
            dst[used++] = *src;
        }
        src++;
    }
    dst[used] = '\0';
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
    apply_profile_defaults_to_terminal(app, &session->terminal);
    app->active = app->session_count;
    app->session_count++;
    app->selection.active = 0;
    app->rename_index = -1;
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
    app->rename_index = -1;
}

static void save_sessions(State *app)
{
    char path[1024];
    FILE *file;
    int i;

    if(app == NULL || app->session_count <= 0 || !session_state_path(path, sizeof(path)))
        return;
    ensure_state_dir();
    file = fopen(path, "w");
    if(file == NULL)
        return;
    fprintf(file, "active=%d\n", app->active);
    for(i = 0; i < app->session_count; i++) {
        char cwd[1024];
        const Session *session = &app->sessions[i];

        session_current_cwd(session, cwd, sizeof(cwd));
        fputs("tab\t", file);
        write_escaped(file, cwd);
        fputc('\t', file);
        write_escaped(file, session->shell);
        fputc('\t', file);
        write_escaped(file, session_title(session));
        fputc('\t', file);
        fprintf(file, "%d", session->title_override ? 1 : 0);
        fputc('\n', file);
    }
    fclose(file);
}

static int restore_sessions(State *app)
{
    char path[1024];
    char line[4096];
    FILE *file;
    int restored = 0;
    int active = 0;

    if(app == NULL || app->session_count > 0 || app->config.command[0] != '\0' ||
       !session_state_path(path, sizeof(path)))
        return 0;
    file = fopen(path, "r");
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL && restored < MAX_SESSIONS) {
        char *field1;
        char *field2;
        char *field3;
        char cwd[1024];
        char shell[512];
        char title[128];
        int title_override = 0;
        size_t len = strlen(line);

        while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if(strncmp(line, "active=", 7) == 0) {
            active = atoi(line + 7);
            continue;
        }
        if(strncmp(line, "tab\t", 4) != 0)
            continue;
        field1 = line + 4;
        field2 = strchr(field1, '\t');
        if(field2 == NULL)
            continue;
        *field2++ = '\0';
        field3 = strchr(field2, '\t');
        if(field3 == NULL)
            continue;
        *field3++ = '\0';
        {
            char *field4 = strchr(field3, '\t');

            if(field4 != NULL) {
                *field4++ = '\0';
                title_override = atoi(field4) != 0;
            }
        }
        read_escaped(cwd, sizeof(cwd), field1);
        read_escaped(shell, sizeof(shell), field2);
        read_escaped(title, sizeof(title), field3);
        if(cwd[0] == '\0')
            snprintf(cwd, sizeof(cwd), "%s", initial_cwd(&app->config));
        if(shell[0] == '\0')
            snprintf(shell, sizeof(shell), "%s", app->config.shell);
        session_init(&app->sessions[restored]);
        session_open(&app->sessions[restored], cwd, shell, "", 100, 30,
                     app->config.scrollback_limit);
        apply_profile_defaults_to_terminal(app, &app->sessions[restored].terminal);
        if(title[0] != '\0')
            session_restore_title(&app->sessions[restored], title,
                                  title_override);
        restored++;
    }
    fclose(file);
    if(restored <= 0)
        return 0;
    app->session_count = restored;
    app->active = clamp_int(active, 0, restored - 1);
    app->selection.active = 0;
    app->rename_index = -1;
    return 1;
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
        else if(strcmp(argv[i], "--help") == 0) {
            printf("usage: kapsule [--working-directory PATH] [--shell PATH] "
                   "[--command CMD] [--font-size N] [--scrollback N] "
                   "[--cursor-style block|underline|bar] "
                   "[--terminal-font PATH] "
                   "[--terminal-foreground #rrggbb|default] "
                   "[--terminal-background #rrggbb|default]\n");
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

static int viewport_row_from_mouse(const State *app, Vector2 mouse)
{
    int row;

    if(app == NULL || !CheckCollisionPointRec(mouse, app->viewport))
        return -1;
    row = (int)((mouse.y - app->viewport.y) / (float)app->line_h);
    if(row < 0 || row >= app->visible_rows)
        return -1;
    return row;
}

static int viewport_col_from_mouse(const State *app, const TerminalState *terminal,
                                   Vector2 mouse)
{
    int col;

    if(app == NULL || terminal == NULL ||
       !CheckCollisionPointRec(mouse, app->viewport))
        return -1;
    col = (int)((mouse.x - app->viewport.x) / (float)app->cell_w);
    return clamp_int(col, 0, max_int(0, terminal->cols - 1));
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

static int word_char(int ch)
{
    return isalnum((unsigned char)ch) || ch == '_' || ch == '-' || ch == '.' ||
           ch == '/' || ch == ':' || ch == '~';
}

static void select_line(State *app, Session *session, int row)
{
    char line[4096];
    int len;

    if(app == NULL || session == NULL || row < 0)
        return;
    terminal_visible_line(&session->terminal, row, line, sizeof(line));
    len = (int)strlen(line);
    app->selection.active = 1;
    app->selection.dragging = 1;
    app->selection.mode = 2;
    app->selection.start_row = row;
    app->selection.end_row = row;
    app->selection.start_col = 0;
    app->selection.end_col = len;
}

static void select_word(State *app, Session *session, int row, int col)
{
    char line[4096];
    int len;
    int start;
    int end;

    if(app == NULL || session == NULL || row < 0)
        return;
    terminal_visible_line(&session->terminal, row, line, sizeof(line));
    len = (int)strlen(line);
    col = clamp_int(col, 0, max_int(0, len));
    if(len == 0) {
        select_line(app, session, row);
        app->selection.mode = 1;
        return;
    }
    if(col >= len)
        col = len - 1;
    if(!word_char((unsigned char)line[col])) {
        app->selection.active = 1;
        app->selection.dragging = 1;
        app->selection.mode = 1;
        app->selection.start_row = row;
        app->selection.end_row = row;
        app->selection.start_col = col;
        app->selection.end_col = col + 1;
        return;
    }
    start = col;
    end = col + 1;
    while(start > 0 && word_char((unsigned char)line[start - 1]))
        start--;
    while(end < len && word_char((unsigned char)line[end]))
        end++;
    app->selection.active = 1;
    app->selection.dragging = 1;
    app->selection.mode = 1;
    app->selection.start_row = row;
    app->selection.end_row = row;
    app->selection.start_col = start;
    app->selection.end_col = end;
}

static void update_selection_end(State *app, Session *session, int row, int col)
{
    char line[4096];
    int len;

    if(app == NULL || session == NULL || row < 0)
        return;
    if(app->selection.mode == 2) {
        terminal_visible_line(&session->terminal, row, line, sizeof(line));
        len = (int)strlen(line);
        if(row < app->selection.start_row) {
            char start_line[4096];

            terminal_visible_line(&session->terminal, app->selection.start_row,
                                  start_line, sizeof(start_line));
            app->selection.start_col = (int)strlen(start_line);
            app->selection.end_col = 0;
        } else {
            app->selection.start_col = 0;
            app->selection.end_col = len;
        }
        app->selection.end_row = row;
        return;
    }
    if(app->selection.mode == 1) {
        terminal_visible_line(&session->terminal, row, line, sizeof(line));
        len = (int)strlen(line);
        col = clamp_int(col, 0, max_int(0, len));
        if(len > 0 && col >= len)
            col = len - 1;
        if(len > 0 && col >= 0 && word_char((unsigned char)line[col])) {
            int end = col + 1;

            while(end < len && word_char((unsigned char)line[end]))
                end++;
            app->selection.end_row = row;
            app->selection.end_col = end;
            return;
        }
    }
    app->selection.end_row = row;
    app->selection.end_col = col + 1;
}

static int collect_selection_text(State *app, char *buffer, size_t buffer_size)
{
    Session *session = active_session(app);
    Selection *selection = &app->selection;
    int sr;
    int er;
    int sc;
    int ec;
    int row;
    size_t used = 0;

    if(buffer == NULL || buffer_size == 0)
        return 0;
    buffer[0] = '\0';
    if(session == NULL || !selection->active)
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
    for(row = sr; row <= er && used < buffer_size - 1; row++) {
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
        for(i = start; i < end && used < buffer_size - 1; i++)
            buffer[used++] = line[i];
        if(row != er && used < buffer_size - 1)
            buffer[used++] = '\n';
    }
    buffer[used] = '\0';
    return used > 0;
}

static void update_primary_selection(State *app)
{
    if(app == NULL)
        return;
    if(!collect_selection_text(app, app->primary_selection,
                               sizeof(app->primary_selection)))
        app->primary_selection[0] = '\0';
}

static void copy_selection(State *app)
{
    static char buffer[COPY_BUFFER_SIZE];

    if(!collect_selection_text(app, buffer, sizeof(buffer)))
        return;
    if(buffer[0] != '\0')
        SetClipboardText(buffer);
    if(app != NULL)
        snprintf(app->primary_selection, sizeof(app->primary_selection), "%s",
                 buffer);
}

static void select_all(State *app)
{
    Session *session = active_session(app);
    int total;

    if(app == NULL || session == NULL)
        return;
    total = terminal_visible_line_count(&session->terminal);
    if(total <= 0)
        return;
    app->selection.active = 1;
    app->selection.dragging = 0;
    app->selection.mode = 0;
    app->selection.start_row = 0;
    app->selection.start_col = 0;
    app->selection.end_row = total - 1;
    app->selection.end_col = session->terminal.cols;
}

static void focus_search(State *app)
{
    if(app == NULL)
        return;
    app->search_visible = 1;
    app->search_focused = 1;
    app->search_cursor = (int)strlen(app->search_text);
}

static void open_profile_prompt(State *app, int kind)
{
    Session *session = active_session(app);

    if(app == NULL)
        return;
    app->profile_prompt = kind;
    app->profile_focused = 1;
    app->profile_text[0] = '\0';
    if(kind == PROFILE_PROMPT_SHELL) {
        snprintf(app->profile_text, sizeof(app->profile_text), "%s",
                 app->config.shell[0] != '\0' ? app->config.shell :
                 session != NULL ? session->shell : "");
    } else if(kind == PROFILE_PROMPT_CWD) {
        if(session != NULL)
            session_current_cwd(session, app->profile_text,
                                (int)sizeof(app->profile_text));
        if(app->profile_text[0] == '\0')
            snprintf(app->profile_text, sizeof(app->profile_text), "%s",
                     app->config.working_directory);
    } else if(kind == PROFILE_PROMPT_FONT_FILE) {
        snprintf(app->profile_text, sizeof(app->profile_text), "%s",
                 app->config.terminal_font);
    } else if(kind == PROFILE_PROMPT_FONT_SIZE) {
        snprintf(app->profile_text, sizeof(app->profile_text), "%d",
                 app->config.font_size);
    } else if(kind == PROFILE_PROMPT_SCROLLBACK) {
        snprintf(app->profile_text, sizeof(app->profile_text), "%d",
                 app->config.scrollback_limit);
    } else if(kind == PROFILE_PROMPT_FOREGROUND) {
        color_text(app->config.terminal_foreground, app->profile_text,
                   (int)sizeof(app->profile_text));
    } else if(kind == PROFILE_PROMPT_BACKGROUND) {
        color_text(app->config.terminal_background, app->profile_text,
                   (int)sizeof(app->profile_text));
    }
    app->profile_cursor = (int)strlen(app->profile_text);
}

static const char *profile_prompt_title(int kind)
{
    if(kind == PROFILE_PROMPT_SHELL)
        return "Shell";
    if(kind == PROFILE_PROMPT_CWD)
        return "Working Directory";
    if(kind == PROFILE_PROMPT_FONT_SIZE)
        return "Font Size";
    if(kind == PROFILE_PROMPT_FONT_FILE)
        return "Terminal Font";
    if(kind == PROFILE_PROMPT_SCROLLBACK)
        return "Scrollback Lines";
    if(kind == PROFILE_PROMPT_FOREGROUND)
        return "Terminal Foreground";
    if(kind == PROFILE_PROMPT_BACKGROUND)
        return "Terminal Background";
    return "Profile";
}

static void apply_profile_prompt(State *app)
{
    int value;
    int i;

    if(app == NULL)
        return;
    if(app->profile_prompt == PROFILE_PROMPT_SHELL) {
        config_apply_arg(&app->config, "shell", app->profile_text);
    } else if(app->profile_prompt == PROFILE_PROMPT_CWD) {
        config_apply_arg(&app->config, "working-directory", app->profile_text);
    } else if(app->profile_prompt == PROFILE_PROMPT_FONT_FILE) {
        config_apply_arg(&app->config, "terminal-font", app->profile_text);
        if(app->config.terminal_font[0] != '\0')
            RegisterUIFontFileSource("kapsule-terminal",
                                     app->config.terminal_font, NULL, 0);
    } else if(app->profile_prompt == PROFILE_PROMPT_FONT_SIZE) {
        value = atoi(app->profile_text);
        if(value >= 10 && value <= 48)
            config_apply_arg(&app->config, "font-size", app->profile_text);
    } else if(app->profile_prompt == PROFILE_PROMPT_SCROLLBACK) {
        value = atoi(app->profile_text);
        if(value >= 100 && value <= 100000) {
            config_apply_arg(&app->config, "scrollback", app->profile_text);
            for(i = 0; i < app->session_count; i++)
                terminal_set_scrollback_limit(&app->sessions[i].terminal,
                                              app->config.scrollback_limit);
        }
    } else if(app->profile_prompt == PROFILE_PROMPT_FOREGROUND) {
        config_apply_arg(&app->config, "terminal-foreground",
                         app->profile_text);
        for(i = 0; i < app->session_count; i++) {
            app->sessions[i].terminal.base_fg =
                effective_terminal_foreground(app);
            app->sessions[i].terminal.base_cursor_color =
                app->sessions[i].terminal.base_fg;
            app->sessions[i].terminal.default_fg =
                effective_terminal_foreground(app);
            if(app->sessions[i].terminal.cursor_color == COLOR_DEFAULT)
                app->sessions[i].terminal.cursor_color =
                    app->sessions[i].terminal.base_cursor_color;
        }
    } else if(app->profile_prompt == PROFILE_PROMPT_BACKGROUND) {
        config_apply_arg(&app->config, "terminal-background",
                         app->profile_text);
        for(i = 0; i < app->session_count; i++) {
            app->sessions[i].terminal.base_bg =
                effective_terminal_background(app);
            app->sessions[i].terminal.default_bg =
                effective_terminal_background(app);
        }
    }
    config_save(&app->config);
}

static void select_search_match(State *app, Session *session, int row, int start_col,
                                int needle_len)
{
    int total;
    int max_scroll;

    if(app == NULL || session == NULL)
        return;
    total = terminal_visible_line_count(&session->terminal);
    max_scroll = max_int(0, total - app->visible_rows);
    session->scroll_offset =
        clamp_int(total - app->visible_rows - row, 0, max_scroll);
    app->selection.active = 1;
    app->selection.dragging = 0;
    app->selection.mode = 0;
    app->selection.start_row = row;
    app->selection.end_row = row;
    app->selection.start_col = start_col;
    app->selection.end_col = start_col + needle_len;
}

static char *last_match_before(char *line, const char *needle, int limit)
{
    char *cursor;
    char *last = NULL;
    int needle_len;

    if(line == NULL || needle == NULL)
        return NULL;
    needle_len = (int)strlen(needle);
    if(needle_len <= 0)
        return NULL;
    cursor = line;
    while((cursor = strstr(cursor, needle)) != NULL) {
        if((int)(cursor - line) + needle_len > limit)
            break;
        last = cursor;
        cursor++;
    }
    return last;
}

static int find_scrollback_direction(State *app, int direction)
{
    Session *session = active_session(app);
    TerminalState *terminal;
    int total;
    int row;
    int needle_len;
    int start_row;
    int start_col;
    int pass;

    if(app == NULL || session == NULL || app->search_text[0] == '\0')
        return 0;
    terminal = &session->terminal;
    total = terminal_visible_line_count(terminal);
    if(total <= 0)
        return 0;
    needle_len = (int)strlen(app->search_text);
    direction = direction >= 0 ? 1 : -1;
    if(app->selection.active) {
        start_row = direction > 0 ? app->selection.end_row :
                                    app->selection.start_row;
        start_col = direction > 0 ? app->selection.end_col :
                                    app->selection.start_col - 1;
    } else {
        start_row = direction > 0 ? app->first_visible_row :
                                    total - 1;
        start_col = direction > 0 ? 0 : 4096;
    }
    start_row = clamp_int(start_row, 0, total - 1);
    for(pass = 0; pass < 2; pass++) {
        int end_row = direction > 0 ? total : -1;

        for(row = start_row; row != end_row; row += direction) {
            char line[4096];
            char *match = NULL;
            int line_len;
            int col_limit;

            terminal_visible_line(terminal, row, line, sizeof(line));
            line_len = (int)strlen(line);
            if(direction > 0) {
                col_limit = row == start_row ? clamp_int(start_col, 0, line_len)
                                             : 0;
                match = strstr(line + col_limit, app->search_text);
            } else {
                col_limit = row == start_row ? clamp_int(start_col, 0, line_len)
                                             : line_len;
                match = last_match_before(line, app->search_text, col_limit);
            }
            if(match != NULL) {
                select_search_match(app, session, row, (int)(match - line),
                                    needle_len);
                return 1;
            }
        }
        start_row = direction > 0 ? 0 : total - 1;
        start_col = direction > 0 ? 0 : 4096;
    }
    return 0;
}

static int find_scrollback(State *app)
{
    Session *session = active_session(app);
    TerminalState *terminal;
    int total;
    int row;
    int needle_len;

    if(app == NULL || session == NULL || app->search_text[0] == '\0')
        return 0;
    terminal = &session->terminal;
    total = terminal_visible_line_count(terminal);
    needle_len = (int)strlen(app->search_text);
    for(row = total - 1; row >= 0; row--) {
        char line[4096];
        char *match;

        terminal_visible_line(terminal, row, line, sizeof(line));
        match = strstr(line, app->search_text);
        if(match != NULL) {
            int start_col = (int)(match - line);

            select_search_match(app, session, row, start_col, needle_len);
            return 1;
        }
    }
    return 0;
}

static int open_hyperlink_at_mouse(State *app, Session *session)
{
    Vector2 mouse = GetMousePosition();
    int viewport_row;
    int visible_row;
    int col;
    const Cell *cell;
    const char *url;

    if(app == NULL || session == NULL ||
       !CheckCollisionPointRec(mouse, app->viewport))
        return 0;
    viewport_row = viewport_row_from_mouse(app, mouse);
    col = viewport_col_from_mouse(app, &session->terminal, mouse);
    if(viewport_row < 0 || col < 0)
        return 0;
    visible_row = app->first_visible_row + viewport_row;
    cell = terminal_visible_cell(&session->terminal, col, visible_row);
    if(cell == NULL || cell->hyperlink <= 0)
        return 0;
    url = terminal_hyperlink(&session->terminal, cell->hyperlink);
    if(url == NULL || url[0] == '\0')
        return 0;
    OpenURL(url);
    return 1;
}

static void handle_selection(State *app, Session *session)
{
    Vector2 mouse = GetMousePosition();
    int row = visible_row_from_mouse(app, mouse);
    int col = visible_col_from_mouse(app, mouse);

    if(app == NULL || session == NULL)
        return;
    if(row >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        double now = GetTime();

        if(row == app->selection_click_row && col == app->selection_click_col &&
           now - app->selection_click_time < 0.45) {
            app->selection_click_count++;
        } else {
            app->selection_click_count = 1;
        }
        app->selection_click_time = now;
        app->selection_click_row = row;
        app->selection_click_col = col;
        if(app->selection_click_count >= 3) {
            select_line(app, session, row);
            return;
        }
        if(app->selection_click_count == 2) {
            select_word(app, session, row, col);
            return;
        }
        app->selection.active = 1;
        app->selection.dragging = 1;
        app->selection.mode = 0;
        app->selection.start_row = row;
        app->selection.end_row = row;
        app->selection.start_col = col;
        app->selection.end_col = col + 1;
    }
    if(app->selection.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if(row < 0) {
            int total = terminal_visible_line_count(&session->terminal);
            int max_scroll = max_int(0, total - app->visible_rows);

            if(mouse.y < app->viewport.y) {
                session->scroll_offset =
                    clamp_int(session->scroll_offset + 1, 0, max_scroll);
                row = app->first_visible_row;
            } else if(mouse.y > app->viewport.y + app->viewport.height) {
                session->scroll_offset =
                    clamp_int(session->scroll_offset - 1, 0, max_scroll);
                row = app->first_visible_row + app->visible_rows - 1;
            }
            col = visible_col_from_mouse(app, (Vector2){
                clamp_int((int)mouse.x, (int)app->viewport.x,
                          (int)(app->viewport.x + app->viewport.width - 1)),
                clamp_int((int)mouse.y, (int)app->viewport.y,
                          (int)(app->viewport.y + app->viewport.height - 1))
            });
        }
        if(row >= 0)
            update_selection_end(app, session, row, col);
    }
    if(app->selection.dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->selection.dragging = 0;
        update_primary_selection(app);
    }
}

static int send_mouse_button(TerminalState *terminal, int ray_button,
                             int terminal_button, int col, int row, int mods,
                             State *app)
{
    if(IsMouseButtonPressed(ray_button)) {
        app->mouse_report_button = terminal_button;
        app->mouse_report_col = col;
        app->mouse_report_row = row;
        return terminal_send_mouse(terminal, terminal_button, col, row, 1, 0,
                                   mods);
    }
    if(IsMouseButtonReleased(ray_button)) {
        int result = terminal_send_mouse(terminal, terminal_button, col, row, 0,
                                         0, mods);

        if(app->mouse_report_button == terminal_button)
            app->mouse_report_button = TERMINAL_MOUSE_RELEASE;
        app->mouse_report_col = col;
        app->mouse_report_row = row;
        return result;
    }
    return 0;
}

static int handle_terminal_mouse(State *app, Session *session, float wheel)
{
    TerminalState *terminal;
    Vector2 mouse;
    int row;
    int col;
    int mods;
    int consumed = 0;
    int steps;
    int i;
    int shift;

    if(app == NULL || session == NULL)
        return 0;
    terminal = &session->terminal;
    shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    mouse = GetMousePosition();
    if(terminal->mouse_mode == 0 || shift ||
       !CheckCollisionPointRec(mouse, app->viewport))
        return 0;
    row = viewport_row_from_mouse(app, mouse);
    col = viewport_col_from_mouse(app, terminal, mouse);
    if(row < 0 || col < 0)
        return 0;
    row = clamp_int(row, 0, max_int(0, terminal->rows - 1));
    mods = input_mods();

    if(wheel != 0.0f) {
        int button = wheel > 0.0f ? TERMINAL_MOUSE_WHEEL_UP
                                  : TERMINAL_MOUSE_WHEEL_DOWN;

        steps = wheel > 0.0f ? (int)(wheel + 0.5f) : (int)((-wheel) + 0.5f);
        if(steps < 1)
            steps = 1;
        for(i = 0; i < steps; i++)
            terminal_send_mouse(terminal, button, col, row, 1, 0, mods);
        consumed = 1;
    }

    if(send_mouse_button(terminal, MOUSE_BUTTON_LEFT, TERMINAL_MOUSE_LEFT, col,
                         row, mods, app))
        consumed = 1;
    if(send_mouse_button(terminal, MOUSE_BUTTON_MIDDLE, TERMINAL_MOUSE_MIDDLE,
                         col, row, mods, app))
        consumed = 1;
    if(send_mouse_button(terminal, MOUSE_BUTTON_RIGHT, TERMINAL_MOUSE_RIGHT, col,
                         row, mods, app))
        consumed = 1;

    if((terminal->mouse_mode == 1002 &&
        app->mouse_report_button != TERMINAL_MOUSE_RELEASE) ||
       terminal->mouse_mode == 1003) {
        if(col != app->mouse_report_col || row != app->mouse_report_row) {
            int button = app->mouse_report_button;

            if(button == TERMINAL_MOUSE_RELEASE)
                button = TERMINAL_MOUSE_RELEASE;
            if(terminal_send_mouse(terminal, button, col, row, 1, 1, mods))
                consumed = 1;
            app->mouse_report_col = col;
            app->mouse_report_row = row;
        }
    }
    return consumed;
}

static void draw_menu_bar(State *app, Rectangle bounds)
{
    static const UIMenuItem file_items[] = {
        {UI_MENU_COMMAND, "New Tab", "Ctrl+Shift+T", MENU_NEW_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Close Tab", "Ctrl+Shift+W", MENU_CLOSE_TAB, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Quit", "Ctrl+Shift+Q", MENU_QUIT, 0, 0, NULL, 0}
    };
    static const UIMenuItem edit_items[] = {
        {UI_MENU_COMMAND, "Copy", "Ctrl+Shift+C", MENU_COPY, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V", MENU_PASTE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Select All", "Ctrl+Shift+A", MENU_SELECT_ALL, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find", "Ctrl+Shift+F", MENU_FIND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Next", "Ctrl+Shift+G", MENU_FIND_NEXT, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Previous", "Ctrl+Shift+B", MENU_FIND_PREVIOUS, 0, 0, NULL, 0}
    };
    static const UIMenuItem view_items[] = {
        {UI_MENU_COMMAND, "Zoom In", "Ctrl++", MENU_FONT_INCREASE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Zoom Out", "Ctrl+-", MENU_FONT_DECREASE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Normal Size", "Ctrl+0", MENU_FONT_RESET, 0, 0, NULL, 0}
    };
    static const UIMenuItem cursor_items[] = {
        {UI_MENU_COMMAND, "Block", NULL, MENU_CURSOR_BLOCK, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Underline", NULL, MENU_CURSOR_UNDERLINE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Bar", NULL, MENU_CURSOR_BAR, 0, 0, NULL, 0}
    };
    static const UIMenuItem terminal_items[] = {
        {UI_MENU_COMMAND, "New Terminal", "Ctrl+Shift+T", MENU_NEW_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V", MENU_PASTE, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Shell...", NULL, MENU_PROFILE_SHELL, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Working Directory...", NULL, MENU_PROFILE_CWD, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Terminal Font...", NULL, MENU_PROFILE_FONT_FILE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Font Size...", NULL, MENU_PROFILE_FONT_SIZE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Scrollback...", NULL, MENU_PROFILE_SCROLLBACK, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Foreground Color...", NULL, MENU_PROFILE_FOREGROUND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Background Color...", NULL, MENU_PROFILE_BACKGROUND, 0, 0, NULL, 0},
        {UI_MENU_SUBMENU, "Cursor Style", NULL, 2500, 0, 0, cursor_items,
         (int)(sizeof(cursor_items) / sizeof(cursor_items[0]))}
    };
    static const UIMenuItem tab_items[] = {
        {UI_MENU_COMMAND, "Next Tab", "Ctrl+Tab", MENU_NEXT_TAB, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Previous Tab", "Ctrl+Shift+Tab", MENU_PREVIOUS_TAB, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Close Tab", "Ctrl+Shift+W", MENU_CLOSE_TAB, 0, 0, NULL, 0}
    };
    static const UIMenuItem help_items[] = {
        {UI_MENU_COMMAND, "About Kapsule", NULL, MENU_ABOUT, 0, 0, NULL, 0}
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
    Session *session;
    const char *text;

    if(app == NULL)
        return;
    result = MenuBar(1200, bounds, menus, (int)(sizeof(menus) / sizeof(menus[0])),
                     &app->top_menu_index);
    session = active_session(app);
    switch(result.activated_id) {
    case MENU_NEW_TAB:
        open_session(app, NULL);
        break;
    case MENU_CLOSE_TAB:
        close_session(app, app->active);
        break;
    case MENU_QUIT:
        app->quit_requested = 1;
        break;
    case MENU_COPY:
        copy_selection(app);
        break;
    case MENU_PASTE:
        text = GetClipboardText();
        if(session != NULL)
            terminal_send_paste(&session->terminal, text);
        break;
    case MENU_SELECT_ALL:
        select_all(app);
        break;
    case MENU_FONT_INCREASE:
        app->config.font_size = clamp_int(app->config.font_size + 1, 8, 28);
        config_save(&app->config);
        break;
    case MENU_FONT_DECREASE:
        app->config.font_size = clamp_int(app->config.font_size - 1, 8, 28);
        config_save(&app->config);
        break;
    case MENU_FONT_RESET:
        app->config.font_size = 14;
        config_save(&app->config);
        break;
    case MENU_NEXT_TAB:
        if(app->session_count > 0)
            app->active = (app->active + 1) % app->session_count;
        break;
    case MENU_PREVIOUS_TAB:
        if(app->session_count > 0)
            app->active =
                (app->active + app->session_count - 1) % app->session_count;
        break;
    case MENU_FIND:
        focus_search(app);
        break;
    case MENU_FIND_NEXT:
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, 1);
        break;
    case MENU_FIND_PREVIOUS:
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, -1);
        break;
    case MENU_PROFILE_SHELL:
        open_profile_prompt(app, PROFILE_PROMPT_SHELL);
        break;
    case MENU_PROFILE_CWD:
        open_profile_prompt(app, PROFILE_PROMPT_CWD);
        break;
    case MENU_PROFILE_FONT_FILE:
        open_profile_prompt(app, PROFILE_PROMPT_FONT_FILE);
        break;
    case MENU_PROFILE_FONT_SIZE:
        open_profile_prompt(app, PROFILE_PROMPT_FONT_SIZE);
        break;
    case MENU_PROFILE_SCROLLBACK:
        open_profile_prompt(app, PROFILE_PROMPT_SCROLLBACK);
        break;
    case MENU_PROFILE_FOREGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_FOREGROUND);
        break;
    case MENU_PROFILE_BACKGROUND:
        open_profile_prompt(app, PROFILE_PROMPT_BACKGROUND);
        break;
    case MENU_CURSOR_BLOCK:
        app->config.cursor_style = TERMINAL_CURSOR_BLOCK;
        config_save(&app->config);
        break;
    case MENU_CURSOR_UNDERLINE:
        app->config.cursor_style = TERMINAL_CURSOR_UNDERLINE;
        config_save(&app->config);
        break;
    case MENU_CURSOR_BAR:
        app->config.cursor_style = TERMINAL_CURSOR_BAR;
        config_save(&app->config);
        break;
    case MENU_ABOUT:
        app->about_visible = 1;
        break;
    default:
        break;
    }
}

static void draw_tabs(State *app, Rectangle bounds)
{
    UITab tabs[MAX_SESSIONS + 1];
    int i;
    int count;
    int clicked;
    int closed = -1;
    int double_clicked = -1;

    if(app == NULL)
        return;
    memset(tabs, 0, sizeof(tabs));
    for(i = 0; i < app->session_count; i++) {
        tabs[i].label = session_title(&app->sessions[i]);
        tabs[i].closeable = app->session_count > 1;
    }
    count = app->session_count;
    if(count < MAX_SESSIONS) {
        tabs[count].label = "+";
        count++;
    }
    clicked = TabBar((TabBarProps){
        bounds,
        tabs,
        count,
        app->active,
        ScaleUIPx(13),
        ScaleUIPx(96),
        ScaleUIPx(220),
        &app->tab_scroll,
        1,
        &closed,
        &double_clicked
    });
    if(closed >= 0 && closed < app->session_count) {
        close_session(app, closed);
        return;
    }
    if(double_clicked >= 0 && double_clicked < app->session_count) {
        const char *title = session_title(&app->sessions[double_clicked]);

        app->rename_index = double_clicked;
        snprintf(app->rename_text, sizeof(app->rename_text), "%s", title);
        app->rename_cursor = (int)strlen(app->rename_text);
        app->rename_focused = 1;
        app->active = double_clicked;
        app->selection.active = 0;
        return;
    }
    if(clicked >= 0) {
        if(clicked < app->session_count) {
            app->active = clicked;
            app->selection.active = 0;
        } else {
            open_session(app, NULL);
        }
    }
}

static void draw_context_menu(State *app, Session *session)
{
    UIMenuItem items[] = {
        {UI_MENU_COMMAND, "Copy", "Ctrl+Shift+C", MENU_CONTEXT_COPY, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Paste", "Ctrl+Shift+V", MENU_CONTEXT_PASTE, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Select All", "Ctrl+Shift+A", MENU_CONTEXT_SELECT_ALL, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find", "Ctrl+Shift+F", MENU_CONTEXT_FIND, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Next", "Ctrl+Shift+G", MENU_CONTEXT_FIND_NEXT, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Find Previous", "Ctrl+Shift+B", MENU_CONTEXT_FIND_PREVIOUS, 0, 0, NULL, 0}
    };
    int command;

    if(app == NULL || session == NULL || !app->context_menu_open)
        return;
    items[0].disabled = !app->selection.active;
    command = PopupMenu(1300, app->context_menu_x, app->context_menu_y, items,
                        (int)(sizeof(items) / sizeof(items[0])));
    if(command == MENU_CONTEXT_COPY) {
        copy_selection(app);
        app->context_menu_open = 0;
    } else if(command == MENU_CONTEXT_PASTE) {
        terminal_send_paste(&session->terminal, GetClipboardText());
        session->scroll_offset = 0;
        app->context_menu_open = 0;
    } else if(command == MENU_CONTEXT_SELECT_ALL) {
        select_all(app);
        app->context_menu_open = 0;
    } else if(command == MENU_CONTEXT_FIND) {
        focus_search(app);
        app->context_menu_open = 0;
    } else if(command == MENU_CONTEXT_FIND_NEXT) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, 1);
        app->context_menu_open = 0;
    } else if(command == MENU_CONTEXT_FIND_PREVIOUS) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, -1);
        app->context_menu_open = 0;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(GetMousePosition(),
                               (Rectangle){(float)app->context_menu_x,
                                           (float)app->context_menu_y,
                                           220.0f, 220.0f}))
        app->context_menu_open = 0;
}

static int append_cell_codepoint(unsigned int codepoint, char *text, int text_size,
                                 int *len)
{
    if(text == NULL || len == NULL || *len >= text_size - 1)
        return 0;
    if(codepoint < 0x80) {
        if(*len + 1 >= text_size)
            return 0;
        text[(*len)++] = (char)codepoint;
    } else if(codepoint < 0x800) {
        if(*len + 2 >= text_size)
            return 0;
        text[(*len)++] = (char)(0xc0 | (codepoint >> 6));
        text[(*len)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if(codepoint < 0x10000) {
        if(*len + 3 >= text_size)
            return 0;
        text[(*len)++] = (char)(0xe0 | (codepoint >> 12));
        text[(*len)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        text[(*len)++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        if(*len + 1 >= text_size)
            return 0;
        text[(*len)++] = '?';
    }
    text[*len] = '\0';
    return 1;
}

static int cell_text(const Cell *cell, char *text, int text_size)
{
    int len = 0;
    unsigned int codepoint;

    if(cell == NULL)
        return 0;
    codepoint = cell->codepoint;
    if(text == NULL || text_size < 2 || codepoint == 0 || codepoint == ' ') {
        if(text != NULL && text_size > 0)
            text[0] = '\0';
        return 0;
    }
    if(!append_cell_codepoint(codepoint, text, text_size, &len))
        return 0;
    if(cell->combining != 0)
        append_cell_codepoint(cell->combining, text, text_size, &len);
    text[len] = '\0';
    return len;
}

static Color resolve_terminal_color(const State *app,
                                    const TerminalState *terminal, int value,
                                    Color fallback)
{
    if(app == NULL)
        return fallback;
    if(terminal != NULL && value >= 0 && value < 256 &&
       terminal->palette_overrides[value] != COLOR_DEFAULT)
        value = terminal->palette_overrides[value];
    return palette_resolve(&app->palette, value, fallback);
}

static void draw_line_cells(State *app, const TerminalState *terminal, int visible_row,
                            int y)
{
    int col;
    Color default_fg;
    Color default_bg;

    if(terminal == NULL || visible_row < 0 ||
       visible_row >= terminal_visible_line_count(terminal))
        return;
    default_fg = resolve_terminal_color(app, terminal, terminal->default_fg,
                                        app->palette.foreground);
    default_bg = resolve_terminal_color(app, terminal, terminal->default_bg,
                                        app->palette.terminal_background);
    for(col = 0; col < terminal->cols; col++) {
        const Cell *cell = terminal_visible_cell(terminal, col, visible_row);
        char text[16];
        int len = 0;
        Color fg;
        Color bg;
        Color underline;
        int linked;
        int selected = selection_contains(&app->selection, visible_row, col);

        if(cell == NULL)
            continue;
        if((cell->style & STYLE_WIDE_CONT) != 0)
            continue;
        fg = resolve_terminal_color(app, terminal, cell->fg, default_fg);
        bg = resolve_terminal_color(app, terminal, cell->bg, default_bg);
        underline = resolve_terminal_color(app, terminal, cell->underline, fg);
        linked = cell->hyperlink > 0;
        if(linked)
            fg = app->palette.link;
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
        len = cell_text(cell, text, sizeof(text));
        if(len <= 0)
            continue;
        DrawUIText(text, (int)app->viewport.x + col * app->cell_w, y,
                   app->config.font_size, fg);
        if(linked)
            underline = app->palette.link;
        if((cell->style & STYLE_UNDERLINE) != 0 || linked)
            DrawRectangle((int)app->viewport.x + col * app->cell_w,
                          y + app->line_h - 3, app->cell_w, 1, underline);
    }
}

static void draw_sixel_images(State *app, const TerminalState *terminal)
{
    int i;

    if(app == NULL || terminal == NULL)
        return;
    for(i = 0; i < terminal_sixel_count(terminal); i++) {
        const SixelImage *image = terminal_sixel_image(terminal, i);
        int visible_row;
        int origin_x;
        int origin_y;
        int y;

        if(image == NULL || image->pixels == NULL ||
           image->alternate_screen != terminal->alternate_screen)
            continue;
        visible_row = terminal->scrollback_count + image->row;
        origin_x = (int)app->viewport.x + image->col * app->cell_w;
        origin_y = (int)app->viewport.y +
                   (visible_row - app->first_visible_row) * app->line_h;
        for(y = 0; y < image->height; y++) {
            int dest_y = origin_y + y;
            int x = 0;

            if(dest_y < (int)app->viewport.y ||
               dest_y >= (int)(app->viewport.y + app->viewport.height))
                continue;
            while(x < image->width) {
                int pixel = image->pixels[y * image->width + x];
                int run = 1;
                Color color;

                if(pixel == COLOR_DEFAULT) {
                    x++;
                    continue;
                }
                while(x + run < image->width &&
                      image->pixels[y * image->width + x + run] == pixel)
                    run++;
                if(origin_x + x + run > (int)app->viewport.x &&
                   origin_x + x < (int)(app->viewport.x + app->viewport.width)) {
                    color = resolve_terminal_color(app, terminal, pixel,
                                                   app->palette.foreground);
                    DrawRectangle(origin_x + x, dest_y, run, 1, color);
                }
                x += run;
            }
        }
    }
}

static void draw_terminal_view(State *app, Session *session, Rectangle bounds)
{
    TerminalState *terminal = &session->terminal;
    int menu_h = ScaleUIPx(34);
    int tab_h = TabBarHeight();
    int total_rows;
    int row;
    int cols;
    int max_scroll;

    seed_theme_defaults_to_terminal(app, terminal);
    UseUIFont("kapsule-terminal");
    app->cell_w = MeasureUIText("M", app->config.font_size);
    if(app->cell_w < 8)
        app->cell_w = app->config.font_size * 6 / 10;
    if(app->cell_w < 8)
        app->cell_w = 8;
    app->line_h = GetUITextLineHeight(app->config.font_size) + 1;
    UseUIFont("kapsule-ui");
    app->viewport =
        (Rectangle){bounds.x + (float)app->config.padding,
                    bounds.y + (float)(menu_h + app->config.padding),
                    bounds.width - (float)(app->config.padding * 2),
                    bounds.height -
                        (float)(menu_h + tab_h + app->config.padding * 2)};
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
    DrawRectangleRec(app->viewport,
                     resolve_terminal_color(app, terminal, terminal->default_bg,
                                            app->palette.terminal_background));
    DrawRectangleLines((int)app->viewport.x, (int)app->viewport.y,
                       (int)app->viewport.width, (int)app->viewport.height,
                       app->palette.chrome_border);

    BeginScissorMode((int)app->viewport.x, (int)app->viewport.y,
                     (int)app->viewport.width, (int)app->viewport.height);
    draw_sixel_images(app, terminal);
    UseUIFont("kapsule-terminal");
    for(row = 0; row < app->visible_rows; row++) {
        int visible_row = app->first_visible_row + row;
        int y = (int)app->viewport.y + row * app->line_h;

        draw_line_cells(app, terminal, visible_row, y);
    }
    if(terminal->cursor_visible && session->scroll_offset == 0 &&
       ((int)(GetTime() * 2.0) & 1) == 0) {
        int cursor_visible_row = terminal->scrollback_count + terminal->cursor_row;
        int cursor_y = cursor_visible_row - app->first_visible_row;

        if(cursor_y >= 0 && cursor_y < app->visible_rows) {
            int x = (int)app->viewport.x + terminal->cursor_col * app->cell_w;
            int y = (int)app->viewport.y + cursor_y * app->line_h;
            int style = terminal->cursor_style != TERMINAL_CURSOR_DEFAULT
                            ? terminal->cursor_style
                            : app->config.cursor_style;
            Color cursor_color =
                resolve_terminal_color(app, terminal, terminal->cursor_color,
                                       resolve_terminal_color(
                                           app, terminal, terminal->default_fg,
                                           app->palette.foreground));
            Color default_bg =
                resolve_terminal_color(app, terminal, terminal->default_bg,
                                       app->palette.terminal_background);

            if(style == TERMINAL_CURSOR_BAR) {
                DrawRectangle(x, y, 2, app->line_h, cursor_color);
            } else if(style == TERMINAL_CURSOR_UNDERLINE) {
                DrawRectangle(x, y + app->line_h - 3, app->cell_w, 2,
                              cursor_color);
            } else {
                const Cell *cell =
                    terminal_cell(terminal, terminal->cursor_col,
                                  terminal->cursor_row);
                char text[16];

                DrawRectangle(x, y, app->cell_w, app->line_h, cursor_color);
                if(cell != NULL && cell_text(cell, text, sizeof(text)) > 0)
                    DrawUIText(text, x, y, app->config.font_size, default_bg);
            }
        }
    }
    UseUIFont("kapsule-ui");
    EndScissorMode();

    if(session->scroll_offset > 0) {
        char label[64];

        snprintf(label, sizeof(label), "%d lines", session->scroll_offset);
        DrawUIText(label,
                   (int)(app->viewport.x + app->viewport.width -
                         MeasureUIText(label, 13) - 8),
                   (int)app->viewport.y + 8, 13, app->palette.muted);
    }
    draw_tabs(app, (Rectangle){bounds.x,
                               bounds.y + bounds.height - (float)tab_h,
                               bounds.width, (float)tab_h});
    draw_context_menu(app, session);
    if(app->about_visible &&
       MessageDialog((MessageDialogProps){
           "Kapsule",
           "A Kryon terminal application.",
           "OK"
       }))
        app->about_visible = 0;
    if(app->search_visible) {
        int result = PromptDialog((PromptDialogProps){
            "Find",
            app->search_text,
            (int)sizeof(app->search_text),
            &app->search_cursor,
            &app->search_focused,
            "Cancel",
            "Find"
        });

        if(result == 1) {
            app->search_visible = 0;
            app->search_focused = 0;
        } else if(result == 2) {
            find_scrollback(app);
            app->search_visible = 0;
            app->search_focused = 0;
        }
    }
    if(app->profile_prompt != PROFILE_PROMPT_NONE) {
        int result = PromptDialog((PromptDialogProps){
            profile_prompt_title(app->profile_prompt),
            app->profile_text,
            (int)sizeof(app->profile_text),
            &app->profile_cursor,
            &app->profile_focused,
            "Cancel",
            "Save"
        });

        if(result == 1) {
            app->profile_prompt = PROFILE_PROMPT_NONE;
            app->profile_focused = 0;
        } else if(result == 2) {
            apply_profile_prompt(app);
            app->profile_prompt = PROFILE_PROMPT_NONE;
            app->profile_focused = 0;
        }
    }
    if(app->rename_index >= 0 && app->rename_index < app->session_count) {
        int result = PromptDialog((PromptDialogProps){
            "Rename Tab",
            app->rename_text,
            (int)sizeof(app->rename_text),
            &app->rename_cursor,
            &app->rename_focused,
            "Cancel",
            "Rename"
        });

        if(result == 1) {
            app->rename_index = -1;
            app->rename_focused = 0;
        } else if(result == 2) {
            session_set_title(&app->sessions[app->rename_index],
                              app->rename_text);
            app->rename_index = -1;
            app->rename_focused = 0;
        }
    } else if(app->rename_index >= 0) {
        app->rename_index = -1;
        app->rename_focused = 0;
    }
}

static void draw_starting_frame(State *app)
{
    Rectangle bounds = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    int menu_h = ScaleUIPx(34);
    int tab_h = TabBarHeight();
    int pad = app != NULL ? app->config.padding : 8;
    Rectangle viewport = {
        bounds.x + (float)pad,
        bounds.y + (float)(menu_h + pad),
        bounds.width - (float)(pad * 2),
        bounds.height - (float)(menu_h + tab_h + pad * 2)
    };

    DrawRectangleRec(bounds, app->palette.background);
    draw_menu_bar(app, (Rectangle){bounds.x, bounds.y, bounds.width,
                                   (float)menu_h});
    DrawRectangleRec(viewport, app->palette.terminal_background);
    DrawRectangleLines((int)viewport.x, (int)viewport.y,
                       (int)viewport.width, (int)viewport.height,
                       app->palette.chrome_border);
    DrawUIText("Starting terminal...", (int)viewport.x + 10,
               (int)viewport.y + 10, app->config.font_size,
               app->palette.foreground);
    draw_tabs(app, (Rectangle){bounds.x,
                               bounds.y + bounds.height - (float)tab_h,
                               bounds.width, (float)tab_h});
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
    if(ctrl && shift && IsKeyPressed(KEY_A)) {
        select_all(app);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_F)) {
        focus_search(app);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_G)) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, 1);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_B)) {
        if(app->search_text[0] == '\0')
            focus_search(app);
        else
            find_scrollback_direction(app, -1);
        return;
    }
    if(ctrl && shift && IsKeyPressed(KEY_Q)) {
        app->quit_requested = 1;
        return;
    }
}

static void handle_input(State *app)
{
    Session *session = active_session(app);
    float wheel;
    int ctrl;
    int shift;
    int opened_link = 0;

    if(session == NULL)
        return;
    if(app->about_visible || app->rename_index >= 0 || app->search_visible ||
       app->profile_prompt != PROFILE_PROMPT_NONE)
        return;
    ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    wheel = GetMouseWheelMove();
    if(!handle_terminal_mouse(app, session, wheel)) {
        if(CheckCollisionPointRec(GetMousePosition(), app->viewport) &&
           wheel != 0.0f) {
            session->scroll_offset += (int)(wheel * 3.0f);
            if(session->scroll_offset < 0)
                session->scroll_offset = 0;
        }
        if(session->terminal.mouse_mode == 0 && ctrl &&
           CheckCollisionPointRec(GetMousePosition(), app->viewport) &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           open_hyperlink_at_mouse(app, session)) {
            app->selection.active = 0;
            session->scroll_offset = 0;
            opened_link = 1;
        } else if(session->terminal.mouse_mode == 0 &&
           CheckCollisionPointRec(GetMousePosition(), app->viewport) &&
           IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            terminal_send_paste(&session->terminal,
                                app->primary_selection[0] != '\0'
                                    ? app->primary_selection
                                    : GetClipboardText());
            session->scroll_offset = 0;
        }
        if(session->terminal.mouse_mode == 0 &&
           CheckCollisionPointRec(GetMousePosition(), app->viewport) &&
           IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            Vector2 mouse = GetMousePosition();

            app->context_menu_open = 1;
            app->context_menu_x = (int)mouse.x;
            app->context_menu_y = (int)mouse.y;
        }
        if(!opened_link)
            handle_selection(app, session);
    } else {
        app->selection.active = 0;
        session->scroll_offset = 0;
        app->context_menu_open = 0;
    }
    handle_shortcuts(app);

    if(ctrl && shift && (IsKeyDown(KEY_A) || IsKeyDown(KEY_B) ||
                         IsKeyDown(KEY_C) || IsKeyDown(KEY_G) ||
                         IsKeyDown(KEY_V) || IsKeyDown(KEY_T) ||
                         IsKeyDown(KEY_W) || IsKeyDown(KEY_F) ||
                         IsKeyDown(KEY_Q)))
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
    app.top_menu_index = -1;
    app.rename_index = -1;
    app.mouse_report_col = -1;
    app.mouse_report_row = -1;
    app.mouse_report_button = TERMINAL_MOUSE_RELEASE;
    app.selection_click_row = -1;
    app.selection_click_col = -1;
    config_defaults(&app.config);
    config_load(&app.config);
    parse_args(&app.config, argc, argv);
    palette_default(&app.palette);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(980, 660, "Kapsule");
    snprintf(app.window_title, sizeof(app.window_title), "Kapsule");
    InitUI(frame_width(), frame_height(), 1.0f);
    app.window_focused = IsWindowFocused() ? 1 : 0;
    load_kryon_font(&app.config);
    palette_apply_system_theme(&app.palette);
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
            terminal_poll(&session->terminal);
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
        if(session != NULL && session->terminal.clipboard_pending) {
            SetClipboardText(session->terminal.clipboard);
            session->terminal.clipboard_pending = 0;
        }
        BeginDrawing();
        ClearBackground(app.palette.background);
        BeginUIFrame(frame_width(), frame_height(), 1.0f);
        if(session != NULL) {
            draw_terminal_view(&app, session,
                               (Rectangle){0, 0, (float)frame_width(),
                                           (float)frame_height()});
            handle_input(&app);
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
