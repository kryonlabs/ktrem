#ifndef CONFIG_H
#define CONFIG_H

typedef struct Config {
    int font_size;
    int padding;
    int scrollback_limit;
    int cursor_style;
    int terminal_foreground;
    int terminal_background;
    char shell[512];
    char working_directory[1024];
    char command[1024];
    char terminal_font[1024];
} Config;

void config_defaults(Config *config);
void config_load(Config *config);
int config_save(const Config *config);
void config_apply_arg(Config *config, const char *name, const char *value);

#endif
