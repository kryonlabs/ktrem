#ifndef KTREM_APP_STATE_H
#define KTREM_APP_STATE_H

#include "config.h"
#include "launch_options.h"
#include "palette.h"
#include "selection.h"
#include "session.h"

#include "kryon.h"

#define MAX_SESSIONS 8

typedef struct State {
    Config config;
    LaunchOptions launch;
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
    Rectangle rename_anchor;
    char rename_text[128];
    int search_visible;
    int search_cursor;
    int search_focused;
    char search_text[128];
    int mouse_report_col;
    int mouse_report_row;
    int mouse_report_x;
    int mouse_report_y;
    int mouse_report_button;
    double selection_click_time;
    int selection_click_row;
    int selection_click_col;
    int selection_click_count;
    int context_menu_open;
    int context_menu_x;
    int context_menu_y;
    int paste_shortcut_down;
    int copy_shortcut_down;
    int new_tab_shortcut_down;
    int close_tab_shortcut_down;
    int mouse_hidden;
    int last_mouse_x;
    int last_mouse_y;
    int profile_prompt;
    int profile_cursor;
    int profile_focused;
    char profile_text[1024];
    char window_title[160];
    Texture2D background_texture;
    char background_texture_path[1024];
    double bell_until;
    double next_theme_refresh;
} State;

#endif
