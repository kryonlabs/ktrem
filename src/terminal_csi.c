#include "terminal_csi.h"

#include <stdio.h>

static int clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static TerminalPaneModeState terminal_mode_state(const TerminalState *terminal);

int terminal_csi_arg(const TerminalState *terminal, int index, int fallback)
{
    if(terminal == NULL || index < 0 || index >= terminal->csi_count ||
       terminal->csi_args[index] == 0)
        return fallback;
    return terminal->csi_args[index];
}

int terminal_csi_cursor_row_from_arg(const TerminalState *terminal, int index,
                                     int fallback)
{
    int row;

    if(terminal == NULL)
        return 0;
    row = terminal_csi_arg(terminal, index, fallback) - 1;
    if(terminal->origin_mode)
        row += terminal->scroll_top;
    return row;
}

void terminal_csi_set_modifier_key_mode(TerminalState *terminal)
{
    int resource;
    int value;

    if(terminal == NULL)
        return;
    resource = terminal_csi_arg(terminal, 0, 0);
    value = terminal_csi_arg(terminal, 1, 0);
    if(resource != 4)
        return;
    terminal->modify_other_keys = clamp_int(value, 0, 2);
}

void terminal_csi_disable_modifier_key_mode(TerminalState *terminal)
{
    int resource;

    if(terminal == NULL)
        return;
    resource = terminal_csi_arg(terminal, 0, 2);
    if(resource == 4)
        terminal->modify_other_keys = -1;
}

void terminal_csi_set_mode(TerminalState *terminal, int mode, int enabled)
{
    TerminalPaneModeState state;

    if(terminal == NULL)
        return;
    state = terminal_mode_state(terminal);
    (void)TerminalPaneModeStateSetMode(&state, mode, enabled);
    terminal->insert_mode = state.insert_mode;
    terminal->newline_mode = state.newline_mode;
}

void terminal_csi_apply_cursor_style(TerminalState *terminal)
{
    int style;
    int decoded_style;
    int decoded_blink;

    if(terminal == NULL)
        return;
    style = terminal_csi_arg(terminal, 0, 0);
    if(DecodeTerminalPaneCursorStyleRequest(style, &decoded_style,
                                            &decoded_blink)) {
        terminal->cursor_style = decoded_style;
        terminal->cursor_blink = decoded_blink;
    }
}

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

void terminal_csi_send_private_mode_report(TerminalState *terminal, int mode)
{
    char response[32];

    if(FormatTerminalPaneModeReport(response, (int)sizeof(response),
                                    terminal_mode_state(terminal), 1, mode) > 0)
        terminal_write_text(terminal, response);
}

void terminal_csi_send_mode_report(TerminalState *terminal, int mode)
{
    char response[32];

    if(FormatTerminalPaneModeReport(response, (int)sizeof(response),
                                    terminal_mode_state(terminal), 0, mode) > 0)
        terminal_write_text(terminal, response);
}

void terminal_csi_send_device_status_report(TerminalState *terminal)
{
    char response[32];
    int request;

    if(terminal == NULL)
        return;
    request = terminal_csi_arg(terminal, 0, 0);
    if(FormatTerminalPaneDeviceStatusReport(
           response, (int)sizeof(response), terminal->csi_private == '?',
           request, terminal->cursor_row, terminal->cursor_col) > 0)
        terminal_write_text(terminal, response);
}
