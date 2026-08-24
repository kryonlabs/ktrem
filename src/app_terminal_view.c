#include "app_terminal_view.h"

#include "app_context_menu.h"
#include "app_menu.h"
#include "app_profile.h"
#include "app_search.h"
#include "app_sessions.h"
#include "selection.h"
#include "terminal.h"

#include <stdio.h>
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

static Color blend_color(Color a, Color b, float t)
{
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    return (Color){(unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
                   (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                   (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
                   a.a};
}

static void draw_tabs(State *app, Rectangle bounds)
{
    Tab tabs[MAX_SESSIONS + 1];
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
        ScaleUIPx(72),
        ScaleUIPx(152),
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
        set_active_session(app, double_clicked);
        return;
    }
    if(clicked >= 0) {
        if(clicked < app->session_count)
            set_active_session(app, clicked);
        else
            open_session(app, NULL);
    }
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
    if(!AppendTerminalPaneUTF8Codepoint(text, text_size, &len, codepoint))
        return 0;
    if(cell->combining != 0)
        AppendTerminalPaneUTF8Codepoint(text, text_size, &len,
                                        cell->combining);
    text[len] = '\0';
    return len;
}

static Color resolve_terminal_color(const State *app,
                                    const TerminalState *terminal, int value,
                                    Color fallback)
{
    if(app == NULL)
        return fallback;
    return ResolveTerminalPaneColorWithOverrides(
        &app->palette.terminal_palette,
        terminal != NULL ? terminal->palette_overrides : NULL, value,
        fallback);
}

static TerminalPaneColors terminal_theme_tokens(void)
{
    return ResolveTerminalPaneThemeColors(GetTerminalPaneThemeColors());
}

static TerminalPaneViewColors terminal_view_colors(
    const State *app, const TerminalState *terminal,
    TerminalPaneColors theme_colors)
{
    TerminalPaneProfileColors colors = {
        COLOR_DEFAULT,
        COLOR_DEFAULT,
        COLOR_DEFAULT,
        COLOR_DEFAULT,
        COLOR_DEFAULT
    };

    if(terminal != NULL) {
        colors.foreground = terminal->default_fg;
        colors.background = terminal->default_bg;
        colors.cursor = terminal->cursor_color;
        colors.selection_foreground = terminal->selection_fg;
        colors.selection_background = terminal->selection_bg;
    }
    return ResolveTerminalPaneViewColors(
        app != NULL ? &app->palette.terminal_palette : NULL,
        terminal != NULL ? terminal->palette_overrides : NULL, colors,
        theme_colors);
}

static void draw_line_cells(State *app, const TerminalState *terminal,
                            TerminalPaneViewColors view_colors,
                            TerminalPaneColors theme_colors,
                            int visible_row, int y)
{
    int col;

    if(terminal == NULL || visible_row < 0 ||
       visible_row >= terminal_visible_line_count(terminal))
        return;
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
        fg = resolve_terminal_color(app, terminal, cell->fg,
                                    view_colors.foreground);
        bg = resolve_terminal_color(app, terminal, cell->bg,
                                    view_colors.background);
        underline = resolve_terminal_color(app, terminal, cell->underline, fg);
        linked = cell->hyperlink > 0;
        if(linked)
            fg = theme_colors.link;
        if((cell->style & STYLE_INVERSE) != 0) {
            Color tmp = fg;

            fg = bg;
            bg = tmp;
        }
        if((cell->style & STYLE_FAINT) != 0)
            fg = blend_color(fg, bg, 0.45f);
        if(selected) {
            DrawRectangle((int)app->viewport.x + col * app->cell_w, y,
                          app->cell_w, app->line_h,
                          view_colors.selection_background);
            fg = view_colors.selection_foreground;
            underline = view_colors.selection_foreground;
        } else if(cell->bg != COLOR_DEFAULT ||
                  (cell->style & STYLE_INVERSE) != 0) {
            DrawRectangle((int)app->viewport.x + col * app->cell_w, y,
                          app->cell_w, app->line_h, bg);
        }
        if(cell->codepoint == 0 || cell->codepoint == ' ')
            continue;
        if((cell->style & STYLE_CONCEAL) != 0)
            continue;
        if((cell->style & STYLE_BLINK) != 0 &&
           ((int)(GetTime() * 2.0) & 1) != 0)
            continue;
        len = cell_text(cell, text, sizeof(text));
        if(len <= 0)
            continue;
        Text(text, (int)app->viewport.x + col * app->cell_w, y,
             app->config.font_size, fg);
        if(linked && !selected)
            underline = theme_colors.link;
        if((cell->style & STYLE_UNDERLINE) != 0 || linked)
            DrawRectangle((int)app->viewport.x + col * app->cell_w,
                          y + app->line_h - 3, app->cell_w, 1, underline);
        if((cell->style & STYLE_OVERLINE) != 0)
            DrawRectangle((int)app->viewport.x + col * app->cell_w,
                          y + 1, app->cell_w, 1, underline);
    }
}

static void draw_sixel_images(State *app, const TerminalState *terminal,
                              TerminalPaneViewColors view_colors)
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
        float pixel_w;
        float pixel_h;

        if(image == NULL || image->pixels == NULL ||
           image->alternate_screen != terminal->alternate_screen)
            continue;
        pixel_h = (float)app->line_h / 6.0f;
        pixel_w = (float)app->cell_w / 6.0f;
        if(image->pixel_aspect_num > 0 && image->pixel_aspect_den > 0)
            pixel_w *= (float)image->pixel_aspect_num /
                       (float)image->pixel_aspect_den;
        if(pixel_w <= 0.0f)
            pixel_w = (float)app->cell_w / 6.0f;
        if(pixel_h <= 0.0f)
            pixel_h = 1.0f;
        visible_row =
            terminal->alternate_screen ? image->row
                                       : terminal->scrollback_count + image->row;
        origin_x = (int)app->viewport.x + image->col * app->cell_w;
        origin_y = (int)app->viewport.y +
                   (visible_row - app->first_visible_row) * app->line_h;
        for(y = 0; y < image->height; y++) {
            float dest_y = (float)origin_y + (float)y * pixel_h;
            int x = 0;

            if(dest_y + pixel_h <= app->viewport.y ||
               dest_y >= app->viewport.y + app->viewport.height)
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
                if((float)origin_x + ((float)x + (float)run) * pixel_w >
                       app->viewport.x &&
                   (float)origin_x + (float)x * pixel_w <
                       app->viewport.x + app->viewport.width) {
                    Rectangle pixel_run;

                    color = resolve_terminal_color(app, terminal, pixel,
                                                   view_colors.foreground);
                    pixel_run = (Rectangle){
                        (float)origin_x + (float)x * pixel_w,
                        dest_y,
                        (float)run * pixel_w,
                        pixel_h
                    };
                    DrawRectangleRec(pixel_run, color);
                }
                x += run;
            }
        }
    }
}

void draw_terminal_view(State *app, Session *session, Rectangle bounds)
{
    TerminalState *terminal = &session->terminal;
    int menu_h = app->launch.show_menubar ? ScaleUIPx(34) : 0;
    int tab_h = app->launch.show_toolbar ? TabBarHeight() : 0;
    int chrome_h = menu_h + tab_h;
    TerminalPaneMetrics metrics;
    TerminalPaneColors theme_colors;
    TerminalPaneViewColors view_colors;
    int total_rows;
    int row;
    int max_scroll;

    seed_theme_defaults_to_terminal(app, terminal);
    UseUIFont("kapsule-terminal");
    metrics = MeasureTerminalPaneContent(
        TerminalPaneContentBounds(bounds, chrome_h, 0), app->config.font_size);
    app->cell_w = metrics.cell_width;
    app->line_h = metrics.line_height;
    app->viewport = metrics.content;
    app->visible_rows = metrics.rows;
    UseUIFont("kapsule-ui");
    terminal_resize(terminal, metrics.cols, metrics.rows);

    total_rows = terminal_visible_line_count(terminal);
    max_scroll = max_int(0, total_rows - app->visible_rows);
    session->scroll_offset = clamp_int(session->scroll_offset, 0, max_scroll);
    app->first_visible_row = selection_first_visible_row(
        total_rows, app->visible_rows, session->scroll_offset);
    theme_colors = terminal_theme_tokens();
    view_colors = terminal_view_colors(app, terminal, theme_colors);

    DrawRectangleRec(bounds, app->palette.background);
    if(menu_h > 0) {
        draw_app_menu_bar(app, (Rectangle){bounds.x, bounds.y, bounds.width,
                                           (float)menu_h});
    }
    if(tab_h > 0) {
        draw_tabs(app, (Rectangle){bounds.x, bounds.y + (float)menu_h,
                                   bounds.width, (float)tab_h});
    }
    DrawRectangleRec(app->viewport, view_colors.background);

    BeginScissorMode((int)app->viewport.x, (int)app->viewport.y,
                     (int)app->viewport.width, (int)app->viewport.height);
    draw_sixel_images(app, terminal, view_colors);
    UseUIFont("kapsule-terminal");
    for(row = 0; row < app->visible_rows; row++) {
        int visible_row = app->first_visible_row + row;
        int y = (int)app->viewport.y + row * app->line_h;

        draw_line_cells(app, terminal, view_colors, theme_colors, visible_row,
                        y);
    }
    if(terminal->cursor_visible && session->scroll_offset == 0 &&
       (!terminal->cursor_blink || ((int)(GetTime() * 2.0) & 1) == 0)) {
        int cursor_visible_row =
            terminal->alternate_screen
                ? terminal->cursor_row
                : terminal->scrollback_count + terminal->cursor_row;
        int cursor_y = cursor_visible_row - app->first_visible_row;

        if(cursor_y >= 0 && cursor_y < app->visible_rows) {
            int x = (int)app->viewport.x + terminal->cursor_col * app->cell_w;
            int y = (int)app->viewport.y + cursor_y * app->line_h;
            int style = terminal->cursor_style != TERMINAL_CURSOR_DEFAULT
                            ? terminal->cursor_style
                            : app->config.cursor_style;

            if(style == TERMINAL_CURSOR_BAR) {
                DrawRectangle(x, y, 2, app->line_h, view_colors.cursor);
            } else if(style == TERMINAL_CURSOR_UNDERLINE) {
                DrawRectangle(x, y + app->line_h - 3, app->cell_w, 2,
                              view_colors.cursor);
            } else {
                const Cell *cell =
                    terminal_cell(terminal, terminal->cursor_col,
                                  terminal->cursor_row);
                char text[16];

                DrawRectangle(x, y, app->cell_w, app->line_h,
                              view_colors.cursor);
                if(cell != NULL && cell_text(cell, text, sizeof(text)) > 0)
                    Text(text, x, y, app->config.font_size,
                         view_colors.background);
            }
        }
    }
    UseUIFont("kapsule-ui");
    EndScissorMode();

    if(app->bell_until > GetTime()) {
        float alpha = (float)((app->bell_until - GetTime()) / 0.18);
        Color overlay = theme_colors.bell_overlay;
        Color border = theme_colors.bell_border;

        alpha = alpha < 0.0f ? 0.0f : alpha;
        alpha = alpha > 1.0f ? 1.0f : alpha;
        overlay.a = (unsigned char)((float)overlay.a * alpha);
        border.a = (unsigned char)((float)border.a * alpha);
        DrawRectangleRec(app->viewport, overlay);
        DrawRectangleLines((int)app->viewport.x, (int)app->viewport.y,
                           (int)app->viewport.width, (int)app->viewport.height,
                           border);
    }

    if(session->scroll_offset > 0) {
        DrawTerminalPaneScrollIndicator((TerminalPaneScrollIndicator){
            app->viewport,
            session->scroll_offset,
            ScaleUIPx(13),
            theme_colors
        });
    }
    draw_context_menu(app, session);
    if(app->about_visible &&
       MessageDialog((MessageDialogProps){
           "Kapsule",
           "A Kryon terminal application.",
           "OK"
       }))
        app->about_visible = 0;
    draw_search_prompt(app);
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

void draw_starting_frame(State *app)
{
    Rectangle bounds = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    int menu_h = app->launch.show_menubar ? ScaleUIPx(34) : 0;
    int tab_h = app->launch.show_toolbar ? TabBarHeight() : 0;
    Rectangle viewport = TerminalPaneContentBounds(bounds, menu_h + tab_h, 0);
    TerminalPaneColors theme_colors = terminal_theme_tokens();

    UseUIFont("kapsule-ui");
    DrawRectangleRec(bounds, app->palette.background);
    if(menu_h > 0) {
        draw_app_menu_bar(app, (Rectangle){bounds.x, bounds.y, bounds.width,
                                           (float)menu_h});
    }
    if(tab_h > 0) {
        draw_tabs(app, (Rectangle){bounds.x, bounds.y + (float)menu_h,
                                   bounds.width, (float)tab_h});
    }
    DrawRectangleRec(viewport, theme_colors.background);
    Text("Starting terminal...", (int)viewport.x + 10,
         (int)viewport.y + 10, app->config.font_size, theme_colors.text);
}
