#include "terminal.h"

#include "terminal_modes.h"

#include <string.h>

void terminal_init(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    memset(terminal, 0, sizeof(*terminal));
    terminal->fd = -1;
    terminal->cursor_visible = 1;
    terminal->cursor_blink = 1;
    terminal->autowrap = 1;
    terminal->insert_mode = 0;
    terminal->scrollback_limit = SCROLLBACK_LIMIT;
    terminal->default_fg = COLOR_DEFAULT;
    terminal->default_bg = COLOR_DEFAULT;
    terminal->cursor_color = COLOR_DEFAULT;
    terminal->mouse_fg = COLOR_DEFAULT;
    terminal->mouse_bg = COLOR_DEFAULT;
    terminal->selection_fg = COLOR_DEFAULT;
    terminal->selection_bg = COLOR_DEFAULT;
    terminal->base_fg = COLOR_DEFAULT;
    terminal->base_bg = COLOR_DEFAULT;
    terminal->base_cursor_color = COLOR_DEFAULT;
    terminal->base_selection_fg = COLOR_DEFAULT;
    terminal->base_selection_bg = COLOR_DEFAULT;
    terminal->cursor_style = TERMINAL_CURSOR_DEFAULT;
    InitUIClipboardBuffer(&terminal->clipboard, "");
    InitTerminalPaneDCSBuffer(&terminal->dcs, 0);
    terminal->current_hyperlink = 0;
    terminal->hyperlink_count = 0;
    terminal_reset_charsets(terminal);
    for(i = 0; i < 256; i++)
        terminal->palette_overrides[i] = COLOR_DEFAULT;
    terminal_reset_style(terminal);
    terminal_save_cursor_state(terminal);
}

int terminal_send_alternate_scroll(TerminalState *terminal, int direction,
                                   int mods)
{
    if(terminal == NULL || direction == 0 || !terminal->alternate_screen ||
       !terminal->alternate_scroll || terminal->mouse_mode != 0)
        return 0;
    return terminal_send_key(terminal,
                             direction > 0 ? KEY_UP_CODE : KEY_DOWN_CODE,
                             mods);
}

int terminal_send_focus(TerminalState *terminal, int focused)
{
    if(terminal == NULL || !terminal->focus_reporting)
        return 0;
    return terminal_write_text(terminal, focused ? "\x1b[I" : "\x1b[O");
}

int terminal_consume_bell(TerminalState *terminal)
{
    if(terminal == NULL || !terminal->bell_pending)
        return 0;
    terminal->bell_pending = 0;
    return 1;
}
