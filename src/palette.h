#ifndef PALETTE_H
#define PALETTE_H

#include "kryon.h"
#include "terminal_pane.h"

typedef struct Palette {
    Color background;
    Color terminal_background;
    Color foreground;
    Color muted;
    Color selection;
    Color selection_text;
    Color cursor;
    Color scroll_indicator;
    Color scroll_indicator_text;
    Color bell_overlay;
    Color bell_border;
    Color link;
    Color chrome;
    Color chrome_light;
    Color chrome_border;
    Color menu_text;
    Color tab;
    Color tab_active;
    TerminalPanePalette terminal_palette;
} Palette;

void palette_default(Palette *palette);
void palette_apply_system_theme(Palette *palette);

#endif
