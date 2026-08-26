#ifndef KTREM_PROFILE_H
#define KTREM_PROFILE_H

#include "config.h"
#include "palette.h"
#include "terminal.h"

typedef TerminalPaneProfileColors TerminalProfileColors;

int profile_color_to_terminal_rgb(Color color);
TerminalProfileColors terminal_profile_colors(const Config *config,
                                              const Palette *palette);
void terminal_profile_apply_new(const Config *config, const Palette *palette,
                                TerminalState *terminal);
void terminal_profile_seed_missing(const Config *config,
                                   const Palette *palette,
                                   TerminalState *terminal);
void terminal_profile_sync_changed(TerminalState *terminal,
                                   TerminalProfileColors old_colors,
                                   TerminalProfileColors new_colors);

#endif
