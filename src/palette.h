#ifndef PALETTE_H
#define PALETTE_H

#include "kryon.h"

typedef struct Palette {
    Color background;
    Color terminal_background;
    Color foreground;
    Color muted;
    Color selection;
    Color chrome;
    Color chrome_light;
    Color chrome_border;
    Color menu_text;
    Color tab;
    Color tab_active;
    Color ansi[256];
} Palette;

void palette_default(Palette *palette);
void palette_apply_system_theme(Palette *palette);
Color palette_resolve(const Palette *palette, int value, Color fallback);

#endif
