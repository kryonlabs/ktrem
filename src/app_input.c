#include "app_input.h"

#include "app_clipboard.h"
#include "app_commands.h"
#include "app_profile.h"
#include "app_sessions.h"
#include "input.h"
#include "selection.h"
#include "terminal.h"

#include <string.h>

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

static int viewport_col_from_mouse(const State *app,
                                   const TerminalState *terminal,
                                   Vector2 mouse)
{
    int col;

    if(app == NULL || terminal == NULL ||
       !CheckCollisionPointRec(mouse, app->viewport))
        return -1;
    col = (int)((mouse.x - app->viewport.x) / (float)app->cell_w);
    return clamp_int(col, 0, max_int(0, terminal->cols - 1));
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
            selection_select_line(&app->selection, &session->terminal, row);
            return;
        }
        if(app->selection_click_count == 2) {
            selection_select_word(&app->selection, &session->terminal, row,
                                  col);
            return;
        }
        selection_begin_char(&app->selection, row, col);
    }
    if(app->selection.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int scroll_delta = selection_edge_scroll_delta(
            mouse.y, app->viewport.y, app->viewport.height, ScaleUIPx(24));

        if(row < 0 || scroll_delta != 0) {
            int total = terminal_visible_line_count(&session->terminal);
            int max_scroll = max_int(0, total - app->visible_rows);
            int first_visible_row = app->first_visible_row;

            if(scroll_delta != 0) {
                session->scroll_offset = clamp_int(
                    session->scroll_offset + scroll_delta, 0, max_scroll);
                first_visible_row = selection_first_visible_row(
                    total, app->visible_rows, session->scroll_offset);
                app->first_visible_row = first_visible_row;
            }
            row = selection_edge_scroll_row(first_visible_row,
                                            app->visible_rows, scroll_delta);
            if(row < 0)
                row = mouse.y < app->viewport.y
                         ? first_visible_row
                         : first_visible_row + app->visible_rows - 1;
            col = visible_col_from_mouse(app, (Vector2){
                clamp_int((int)mouse.x, (int)app->viewport.x,
                          (int)(app->viewport.x + app->viewport.width - 1)),
                clamp_int((int)mouse.y, (int)app->viewport.y,
                          (int)(app->viewport.y + app->viewport.height - 1))
            });
        }
        if(row >= 0)
            selection_update_end(&app->selection, &session->terminal, row,
                                 col);
    }
    if(app->selection.dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->selection.dragging = 0;
        update_primary_selection(app);
    }
}

static int send_mouse_button(TerminalState *terminal, int ray_button,
                             int terminal_button, int col, int row,
                             int pixel_x, int pixel_y, int mods, State *app)
{
    if(IsMouseButtonPressed(ray_button)) {
        app->mouse_report_button = terminal_button;
        app->mouse_report_col = col;
        app->mouse_report_row = row;
        app->mouse_report_x = pixel_x;
        app->mouse_report_y = pixel_y;
        return terminal_send_mouse_pixels(terminal, terminal_button, col, row,
                                          pixel_x, pixel_y, 1, 0, mods);
    }
    if(IsMouseButtonReleased(ray_button)) {
        int result = terminal_send_mouse_pixels(terminal, terminal_button, col,
                                                row, pixel_x, pixel_y, 0, 0,
                                                mods);

        if(app->mouse_report_button == terminal_button)
            app->mouse_report_button = TERMINAL_MOUSE_RELEASE;
        app->mouse_report_col = col;
        app->mouse_report_row = row;
        app->mouse_report_x = pixel_x;
        app->mouse_report_y = pixel_y;
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
    int pixel_x;
    int pixel_y;
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
    pixel_x = clamp_int((int)(mouse.x - app->viewport.x), 0,
                        max_int(0, (int)app->viewport.width - 1));
    pixel_y = clamp_int((int)(mouse.y - app->viewport.y), 0,
                        max_int(0, (int)app->viewport.height - 1));
    mods = input_mods();

    if(wheel != 0.0f) {
        int button = wheel > 0.0f ? TERMINAL_MOUSE_WHEEL_UP
                                  : TERMINAL_MOUSE_WHEEL_DOWN;

        steps = wheel > 0.0f ? (int)(wheel + 0.5f) : (int)((-wheel) + 0.5f);
        if(steps < 1)
            steps = 1;
        for(i = 0; i < steps; i++) {
            if(terminal_send_mouse_pixels(terminal, button, col, row, pixel_x,
                                          pixel_y, 1, 0, mods))
                consumed = 1;
        }
    }

    if(send_mouse_button(terminal, MOUSE_BUTTON_LEFT, TERMINAL_MOUSE_LEFT, col,
                         row, pixel_x, pixel_y, mods, app))
        consumed = 1;
    if(send_mouse_button(terminal, MOUSE_BUTTON_MIDDLE, TERMINAL_MOUSE_MIDDLE,
                         col, row, pixel_x, pixel_y, mods, app))
        consumed = 1;
    if(send_mouse_button(terminal, MOUSE_BUTTON_RIGHT, TERMINAL_MOUSE_RIGHT,
                         col, row, pixel_x, pixel_y, mods, app))
        consumed = 1;

    if((terminal->mouse_mode == 1002 &&
        app->mouse_report_button != TERMINAL_MOUSE_RELEASE) ||
       terminal->mouse_mode == 1003) {
        if(col != app->mouse_report_col || row != app->mouse_report_row ||
           (terminal->mouse_pixels &&
            (pixel_x != app->mouse_report_x ||
             pixel_y != app->mouse_report_y))) {
            int button = app->mouse_report_button;

            if(button == TERMINAL_MOUSE_RELEASE)
                button = TERMINAL_MOUSE_RELEASE;
            if(terminal_send_mouse_pixels(terminal, button, col, row, pixel_x,
                                          pixel_y, 1, 1, mods))
                consumed = 1;
            app->mouse_report_col = col;
            app->mouse_report_row = row;
            app->mouse_report_x = pixel_x;
            app->mouse_report_y = pixel_y;
        }
    }
    return consumed;
}

static int handle_terminal_keyboard_shortcut(void *userdata, const char *text,
                                             int length)
{
    State *app = userdata;
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if(app == NULL || text == NULL)
        return 0;
    if((length == 6 && memcmp(text, "\x1b[2;2~", 6) == 0) ||
       (length == 4 && shift && memcmp(text, "\x1b[2~", 4) == 0)) {
        app_execute_command(app, APP_COMMAND_PASTE);
        return 1;
    }
    if(length == 6 && memcmp(text, "\x1b[2;5~", 6) == 0) {
        app_execute_command(app, APP_COMMAND_COPY);
        return 1;
    }
    return 0;
}

static int handle_terminal_key_shortcut(void *userdata, int platform_key,
                                        int mods)
{
    State *app = userdata;
    int ctrl = (mods & TERMINAL_PANE_MOD_CTRL) != 0;
    int shift = (mods & TERMINAL_PANE_MOD_SHIFT) != 0;

    if(app == NULL)
        return 0;
    if(ctrl && shift && platform_key == KEY_V) {
        app_execute_command(app, APP_COMMAND_PASTE);
        return 1;
    }
    if((ctrl && shift && platform_key == KEY_C) ||
       (ctrl && platform_key == KEY_INSERT)) {
        app_execute_command(app, APP_COMMAND_COPY);
        return 1;
    }
    return 0;
}

static int handle_down_edge_shortcuts(State *app, int ctrl, int shift)
{
    int paste_down;
    int copy_down;

    if(app == NULL)
        return 0;
    paste_down = (shift && IsKeyDown(KEY_INSERT)) ||
                 (ctrl && shift && IsKeyDown(KEY_V));
    copy_down = (ctrl && IsKeyDown(KEY_INSERT)) ||
                (ctrl && shift && IsKeyDown(KEY_C));
    if(paste_down) {
        if(!app->paste_shortcut_down)
            app_execute_command(app, APP_COMMAND_PASTE);
        app->paste_shortcut_down = 1;
        app->copy_shortcut_down = copy_down;
        return 1;
    }
    if(copy_down) {
        if(!app->copy_shortcut_down)
            app_execute_command(app, APP_COMMAND_COPY);
        app->copy_shortcut_down = 1;
        app->paste_shortcut_down = 0;
        return 1;
    }
    app->paste_shortcut_down = 0;
    app->copy_shortcut_down = 0;
    return 0;
}

void app_handle_input(State *app)
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
            int direction = wheel > 0.0f ? 1 : -1;
            int steps = wheel > 0.0f ? (int)(wheel + 0.5f) :
                                      (int)((-wheel) + 0.5f);
            int consumed = 0;
            int i;

            if(steps < 1)
                steps = 1;
            for(i = 0; i < steps; i++) {
                if(terminal_send_alternate_scroll(&session->terminal,
                                                  direction, input_mods()))
                    consumed = 1;
            }
            if(consumed) {
                session->scroll_offset = 0;
            } else {
                session->scroll_offset += (int)(wheel * 3.0f);
                if(session->scroll_offset < 0)
                    session->scroll_offset = 0;
            }
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
            paste_primary_or_clipboard_to_session(app, session);
        }
        if(!opened_link)
            handle_selection(app, session);
    } else {
        app->selection.active = 0;
        session->scroll_offset = 0;
        app->context_menu_open = 0;
    }
    if(handle_down_edge_shortcuts(app, ctrl, shift))
        return;
    if(app_handle_shortcuts(app))
        return;

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
        input_send_keyboard_filtered(&session->terminal,
                                     handle_terminal_key_shortcut,
                                     handle_terminal_keyboard_shortcut, app);
        if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE) ||
           IsKeyPressed(KEY_TAB))
            session->scroll_offset = 0;
    }
}
