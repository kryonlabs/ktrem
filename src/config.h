#ifndef CONFIG_H
#define CONFIG_H

#include "terminal_pane.h"

#define KAPSULE_DEFAULT_FONT_SIZE 16
#define KAPSULE_MIN_FONT_SIZE 10
#define KAPSULE_MAX_FONT_SIZE 48
#define KAPSULE_DEFAULT_SCROLLBACK_LIMIT 5000
#define KAPSULE_MIN_SCROLLBACK_LIMIT 100
#define KAPSULE_MAX_SCROLLBACK_LIMIT 100000

typedef TerminalPaneProfileSettings Config;

TerminalPaneProfileLimits config_profile_limits(void);
void config_defaults(Config *config);
void config_load(Config *config);
int config_save(const Config *config);
void config_apply_arg(Config *config, const char *name, const char *value);

#endif
