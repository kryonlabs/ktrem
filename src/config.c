#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

TerminalPaneProfileLimits config_profile_limits(void)
{
    TerminalPaneProfileLimits limits = GetDefaultTerminalPaneProfileLimits();

    limits.default_font_size = KAPSULE_DEFAULT_FONT_SIZE;
    limits.min_font_size = KAPSULE_MIN_FONT_SIZE;
    limits.max_font_size = KAPSULE_MAX_FONT_SIZE;
    limits.default_scrollback_limit = KAPSULE_DEFAULT_SCROLLBACK_LIMIT;
    limits.min_scrollback_limit = KAPSULE_MIN_SCROLLBACK_LIMIT;
    limits.max_scrollback_limit = KAPSULE_MAX_SCROLLBACK_LIMIT;
    limits.default_cursor_style = TERMINAL_PANE_CURSOR_BLOCK;
    return limits;
}

void config_defaults(Config *config)
{
    if(config == NULL)
        return;
    InitTerminalPaneProfileSettings(config, config_profile_limits());
}

static void trim_newline(char *text)
{
    size_t len;

    if(text == NULL)
        return;
    len = strlen(text);
    while(len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static char *trim_spaces(char *text)
{
    char *end;

    if(text == NULL)
        return NULL;
    while(*text == ' ' || *text == '\t')
        text++;
    end = text + strlen(text);
    while(end > text && (end[-1] == ' ' || end[-1] == '\t'))
        *--end = '\0';
    return text;
}

static void write_setting(FILE *file, const char *name, const char *value)
{
    char escaped[4096];

    if(file == NULL || name == NULL || value == NULL || value[0] == '\0')
        return;
    fprintf(file, "%s=", name);
    EscapeTerminalPaneText(escaped, (int)sizeof(escaped), value);
    fputs(escaped, file);
    fputc('\n', file);
}

static void write_color(FILE *file, const char *name, int color)
{
    char text[16];

    if(file == NULL || name == NULL ||
       color == TERMINAL_PANE_COLOR_DEFAULT)
        return;
    if(FormatTerminalPaneProfileColor(text, (int)sizeof(text), color) > 0)
        fprintf(file, "%s=%s\n", name, text);
}

static int config_path(char *path, int path_size)
{
    const char *xdg;
    const char *home;

    if(path == NULL || path_size <= 0)
        return 0;
    xdg = getenv("XDG_CONFIG_HOME");
    home = getenv("HOME");
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/kapsule/config", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, (size_t)path_size, "%s/.config/kapsule/config", home);
    else
        return 0;
    return 1;
}

static void ensure_parent_dirs(const char *path)
{
    char partial[1024];
    int i;

    if(path == NULL || path[0] == '\0')
        return;
    snprintf(partial, sizeof(partial), "%s", path);
    for(i = 1; partial[i] != '\0'; i++) {
        if(partial[i] == '/') {
            partial[i] = '\0';
            mkdir(partial, 0700);
            partial[i] = '/';
        }
    }
}

void config_apply_arg(Config *config, const char *name, const char *value)
{
    (void)ApplyTerminalPaneProfileSetting(config, config_profile_limits(), name,
                                          value);
}

void config_load(Config *config)
{
    char path[1024];
    char line[2048];
    FILE *file;

    if(config == NULL)
        return;
    if(!config_path(path, sizeof(path)))
        return;
    file = fopen(path, "r");
    if(file == NULL)
        return;
    while(fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        char *name;
        char *value;

        trim_newline(line);
        if(line[0] == '\0' || line[0] == '#')
            continue;
        equals = strchr(line, '=');
        if(equals == NULL)
            continue;
        *equals = '\0';
        name = trim_spaces(line);
        value = trim_spaces(equals + 1);
        if(name != NULL && value != NULL) {
            char decoded[2048];

            UnescapeTerminalPaneText(decoded, (int)sizeof(decoded), value);
            config_apply_arg(config, name, decoded);
        }
    }
    fclose(file);
}

int config_save(const Config *config)
{
    char path[1024];
    FILE *file;

    if(config == NULL || !config_path(path, sizeof(path)))
        return 0;
    ensure_parent_dirs(path);
    file = fopen(path, "w");
    if(file == NULL)
        return 0;
    fprintf(file, "font_size=%d\n", config->font_size);
    fprintf(file, "scrollback=%d\n", config->scrollback_limit);
    fprintf(file, "cursor_style=%s\n",
            TerminalPaneCursorStyleName(config->cursor_style));
    write_setting(file, "shell", config->shell);
    write_setting(file, "working_directory", config->working_directory);
    write_setting(file, "command", config->command);
    write_setting(file, "terminal_font", config->terminal_font);
    write_color(file, "terminal_foreground", config->terminal_foreground);
    write_color(file, "terminal_background", config->terminal_background);
    write_color(file, "terminal_cursor", config->terminal_cursor);
    write_color(file, "terminal_selection_foreground",
                config->terminal_selection_foreground);
    write_color(file, "terminal_selection_background",
                config->terminal_selection_background);
    fclose(file);
    return 1;
}
