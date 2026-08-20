#include "terminal_modes.h"

#include "terminal_screen.h"
#include "terminal_sixel.h"
#include "terminal_text.h"

static TerminalPaneModeState terminal_mode_state(const TerminalState *terminal)
{
    if(terminal == NULL)
        return (TerminalPaneModeState){0};
    return (TerminalPaneModeState){
        terminal->cursor_blink,
        terminal->cursor_visible,
        terminal->origin_mode,
        terminal->autowrap,
        terminal->application_cursor_keys,
        terminal->mouse_mode,
        terminal->focus_reporting,
        terminal->mouse_utf8,
        terminal->mouse_sgr,
        terminal->alternate_scroll,
        terminal->mouse_urxvt,
        terminal->mouse_pixels,
        terminal->bracketed_paste,
        terminal->alternate_screen,
        terminal->insert_mode,
        terminal->newline_mode,
    };
}

static void terminal_apply_mode_state(TerminalState *terminal,
                                      TerminalPaneModeState state)
{
    if(terminal == NULL)
        return;
    terminal->cursor_blink = state.cursor_blink;
    terminal->cursor_visible = state.cursor_visible;
    terminal->origin_mode = state.origin_mode;
    terminal->autowrap = state.autowrap;
    terminal->application_cursor_keys = state.application_cursor_keys;
    terminal->mouse_mode = state.mouse_mode;
    terminal->focus_reporting = state.focus_reporting;
    terminal->mouse_utf8 = state.mouse_utf8;
    terminal->mouse_sgr = state.mouse_sgr;
    terminal->alternate_scroll = state.alternate_scroll;
    terminal->mouse_urxvt = state.mouse_urxvt;
    terminal->mouse_pixels = state.mouse_pixels;
    terminal->bracketed_paste = state.bracketed_paste;
    terminal->alternate_screen = state.alternate_screen;
    terminal->insert_mode = state.insert_mode;
    terminal->newline_mode = state.newline_mode;
}

void terminal_reset_style(TerminalState *terminal)
{
    terminal->current_fg = COLOR_DEFAULT;
    terminal->current_bg = COLOR_DEFAULT;
    terminal->current_underline = COLOR_DEFAULT;
    terminal->current_style = 0;
}

void terminal_reset_charsets(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->g0_charset = TERMINAL_CHARSET_US_ASCII;
    terminal->g1_charset = TERMINAL_CHARSET_US_ASCII;
    terminal->active_charset = 0;
    terminal->pending_charset = 0;
}

void terminal_save_cursor_state(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->saved_col = terminal->cursor_col;
    terminal->saved_row = terminal->cursor_row;
    terminal->saved_fg = terminal->current_fg;
    terminal->saved_bg = terminal->current_bg;
    terminal->saved_underline = terminal->current_underline;
    terminal->saved_style = terminal->current_style;
    terminal->saved_hyperlink = terminal->current_hyperlink;
    terminal->saved_g0_charset = terminal->g0_charset;
    terminal->saved_g1_charset = terminal->g1_charset;
    terminal->saved_active_charset = terminal->active_charset;
    terminal->saved_origin_mode = terminal->origin_mode;
    terminal->saved_autowrap = terminal->autowrap;
    terminal->saved_insert_mode = terminal->insert_mode;
}

void terminal_restore_cursor_state(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->cursor_col = terminal->saved_col;
    terminal->cursor_row = terminal->saved_row;
    terminal->current_fg = terminal->saved_fg;
    terminal->current_bg = terminal->saved_bg;
    terminal->current_underline = terminal->saved_underline;
    terminal->current_style = terminal->saved_style;
    terminal->current_hyperlink = terminal->saved_hyperlink;
    terminal->g0_charset = terminal->saved_g0_charset;
    terminal->g1_charset = terminal->saved_g1_charset;
    terminal->active_charset = terminal->saved_active_charset;
    terminal->origin_mode = terminal->saved_origin_mode;
    terminal->autowrap = terminal->saved_autowrap;
    terminal->insert_mode = terminal->saved_insert_mode;
    terminal_clamp_cursor(terminal);
}

void terminal_soft_reset(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->cursor_col = 0;
    terminal->cursor_row = 0;
    terminal->scroll_top = 0;
    terminal->scroll_bottom = terminal->rows > 0 ? terminal->rows - 1 : 0;
    terminal->cursor_visible = 1;
    terminal->cursor_blink = 1;
    terminal->origin_mode = 0;
    terminal->autowrap = 1;
    terminal->application_cursor_keys = 0;
    terminal->application_keypad = 0;
    terminal->bracketed_paste = 0;
    terminal->modify_other_keys = 0;
    terminal->insert_mode = 0;
    terminal->newline_mode = 0;
    terminal->mouse_mode = 0;
    terminal->mouse_utf8 = 0;
    terminal->mouse_sgr = 0;
    terminal->mouse_urxvt = 0;
    terminal->mouse_pixels = 0;
    terminal->alternate_scroll = 0;
    terminal->focus_reporting = 0;
    terminal->current_hyperlink = 0;
    terminal_reset_style(terminal);
    terminal_reset_charsets(terminal);
    terminal_reset_tab_stops(terminal);
    terminal_save_cursor_state(terminal);
    terminal_clamp_cursor(terminal);
}

void terminal_reset_device(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->alternate_screen = 0;
    terminal_clear_screen(terminal);
    terminal_clear_alternate_screen(terminal);
    terminal_reset_style(terminal);
    terminal->scroll_top = 0;
    terminal->scroll_bottom = terminal->rows - 1;
    terminal->cursor_visible = 1;
    terminal->cursor_blink = 1;
    terminal->autowrap = 1;
    terminal->application_cursor_keys = 0;
    terminal->application_keypad = 0;
    terminal->bracketed_paste = 0;
    terminal->modify_other_keys = 0;
    terminal->insert_mode = 0;
    terminal->mouse_mode = 0;
    terminal->mouse_utf8 = 0;
    terminal->mouse_sgr = 0;
    terminal->mouse_urxvt = 0;
    terminal->mouse_pixels = 0;
    terminal->alternate_scroll = 0;
    terminal->focus_reporting = 0;
    terminal->origin_mode = 0;
    terminal->current_hyperlink = 0;
    terminal_reset_charsets(terminal);
    terminal_sixel_clear_images(terminal, -1);
    terminal_reset_tab_stops(terminal);
    terminal_save_cursor_state(terminal);
}

void terminal_set_private_mode(TerminalState *terminal, int mode, int enabled)
{
    TerminalPaneModeState state;
    int actions;

    if(terminal == NULL)
        return;
    state = terminal_mode_state(terminal);
    actions = TerminalPaneModeStateSetPrivateMode(&state, mode, enabled);
    terminal_apply_mode_state(terminal, state);
    if(actions & TERMINAL_PANE_MODE_ACTION_ORIGIN_CURSOR) {
        terminal->cursor_col = 0;
        terminal->cursor_row = terminal->origin_mode ? terminal->scroll_top : 0;
    }
    if(actions & TERMINAL_PANE_MODE_ACTION_SAVE_CURSOR)
        terminal_save_cursor_state(terminal);
    if(actions & TERMINAL_PANE_MODE_ACTION_CLEAR_SCREEN)
        terminal_clear_screen(terminal);
    if(actions & TERMINAL_PANE_MODE_ACTION_CLEAR_ALTERNATE)
        terminal_clear_alternate_screen(terminal);
    if(actions & TERMINAL_PANE_MODE_ACTION_RESTORE_CURSOR)
        terminal_restore_cursor_state(terminal);
}
