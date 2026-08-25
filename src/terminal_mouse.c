#include "terminal.h"

static TerminalPaneMouseMode terminal_mouse_mode(const TerminalState *terminal)
{
    TerminalPaneMouseMode mode = {0};

    if(terminal == NULL)
        return mode;
    mode.mode = terminal->mouse_mode;
    mode.utf8 = terminal->mouse_utf8;
    mode.sgr = terminal->mouse_sgr;
    mode.urxvt = terminal->mouse_urxvt;
    mode.pixels = terminal->mouse_pixels;
    return mode;
}

int terminal_send_mouse_pixels(TerminalState *terminal, int button, int col,
                               int row, int pixel_x, int pixel_y, int pressed,
                               int motion, int mods)
{
    char seq[64];
    int len;

    if(terminal == NULL)
        return 0;
    len = EncodeTerminalPaneMouse(seq, (int)sizeof(seq), button, col, row,
                                  pixel_x, pixel_y, pressed, motion, mods,
                                  terminal_mouse_mode(terminal));
    if(len <= 0)
        return 0;
    return terminal_write(terminal, seq, len);
}

int terminal_send_mouse(TerminalState *terminal, int button, int col, int row,
                        int pressed, int motion, int mods)
{
    return terminal_send_mouse_pixels(terminal, button, col, row, col, row,
                                      pressed, motion, mods);
}
