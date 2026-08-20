#include "terminal_sgr.h"

static void reset_current_style(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->current_fg = COLOR_DEFAULT;
    terminal->current_bg = COLOR_DEFAULT;
    terminal->current_underline = COLOR_DEFAULT;
    terminal->current_style = 0;
}

void terminal_sgr_apply(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    if(terminal->csi_count == 0) {
        reset_current_style(terminal);
        return;
    }
    for(i = 0; i < terminal->csi_count; i++) {
        int arg = terminal->csi_args[i];

        if(arg == 0)
            reset_current_style(terminal);
        else if(arg == 1)
            terminal->current_style |= STYLE_BOLD;
        else if(arg == 2)
            terminal->current_style |= STYLE_FAINT;
        else if(arg == 3)
            terminal->current_style |= STYLE_ITALIC;
        else if(arg == 4)
            terminal->current_style |= STYLE_UNDERLINE;
        else if(arg == 5 || arg == 6)
            terminal->current_style |= STYLE_BLINK;
        else if(arg == 7)
            terminal->current_style |= STYLE_INVERSE;
        else if(arg == 8)
            terminal->current_style |= STYLE_CONCEAL;
        else if(arg == 9)
            terminal->current_style |= STYLE_STRIKE;
        else if(arg == 22)
            terminal->current_style &= ~(STYLE_BOLD | STYLE_FAINT);
        else if(arg == 23)
            terminal->current_style &= ~STYLE_ITALIC;
        else if(arg == 24)
            terminal->current_style &= ~STYLE_UNDERLINE;
        else if(arg == 25)
            terminal->current_style &= ~STYLE_BLINK;
        else if(arg == 27)
            terminal->current_style &= ~STYLE_INVERSE;
        else if(arg == 28)
            terminal->current_style &= ~STYLE_CONCEAL;
        else if(arg == 29)
            terminal->current_style &= ~STYLE_STRIKE;
        else if(arg == 53)
            terminal->current_style |= STYLE_OVERLINE;
        else if(arg == 55)
            terminal->current_style &= ~STYLE_OVERLINE;
        else if(arg == 39)
            terminal->current_fg = COLOR_DEFAULT;
        else if(arg == 49)
            terminal->current_bg = COLOR_DEFAULT;
        else if(arg == 59)
            terminal->current_underline = COLOR_DEFAULT;
        else if(arg >= 30 && arg <= 37)
            terminal->current_fg = arg - 30;
        else if(arg >= 40 && arg <= 47)
            terminal->current_bg = arg - 40;
        else if(arg >= 90 && arg <= 97)
            terminal->current_fg = 8 + arg - 90;
        else if(arg >= 100 && arg <= 107)
            terminal->current_bg = 8 + arg - 100;
        else if((arg == 38 || arg == 48 || arg == 58) &&
                i + 2 < terminal->csi_count &&
                terminal->csi_args[i + 1] == 5) {
            if(arg == 38)
                terminal->current_fg = terminal->csi_args[i + 2] & 255;
            else if(arg == 48)
                terminal->current_bg = terminal->csi_args[i + 2] & 255;
            else
                terminal->current_underline = terminal->csi_args[i + 2] & 255;
            i += 2;
        } else if((arg == 38 || arg == 48 || arg == 58) &&
                  i + 4 < terminal->csi_count &&
                  terminal->csi_args[i + 1] == 2) {
            int offset = (i + 5 < terminal->csi_count &&
                          terminal->csi_args[i + 2] == 0)
                             ? 3
                             : 2;
            int rgb = ((terminal->csi_args[i + offset] & 255) << 16) |
                      ((terminal->csi_args[i + offset + 1] & 255) << 8) |
                      (terminal->csi_args[i + offset + 2] & 255);

            if(arg == 38)
                terminal->current_fg = COLOR_TRUE_RGB | rgb;
            else if(arg == 48)
                terminal->current_bg = COLOR_TRUE_RGB | rgb;
            else
                terminal->current_underline = COLOR_TRUE_RGB | rgb;
            i += offset + 2;
        }
    }
}
