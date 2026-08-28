#ifndef CONFIG_H
#define CONFIG_H

#include "terminal_pane.h"

#define KTREM_DEFAULT_FONT_SIZE 16
#define KTREM_MIN_FONT_SIZE 10
#define KTREM_MAX_FONT_SIZE 48
#define KTREM_DEFAULT_SCROLLBACK_LIMIT 5000
#define KTREM_MIN_SCROLLBACK_LIMIT 100
#define KTREM_MAX_SCROLLBACK_LIMIT 100000
#define KTREM_UNLIMITED_SCROLLBACK_LIMIT 1000000

typedef TerminalPaneProfileSettings Config;

TerminalPaneProfileLimits config_profile_limits(void);
void config_defaults(Config *config);
void config_load(Config *config);
int config_save(const Config *config);
void config_apply_arg(Config *config, const char *name, const char *value);
int config_effective_scrollback_limit(const Config *config);

#endif
