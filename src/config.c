#include "config.h"

#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static void copy_text(char *dst, int dst_size, const char *src)
{
    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, (size_t)dst_size, "%s", src);
}

void config_defaults(Config *config)
{
    if(config == NULL)
        return;
    memset(config, 0, sizeof(*config));
    config->font_size = 16;
    config->padding = 10;
    config->scrollback_limit = 5000;
    config->cursor_style = TERMINAL_CURSOR_BLOCK;
    config->terminal_foreground = COLOR_DEFAULT;
    config->terminal_background = COLOR_DEFAULT;
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

static const char *cursor_style_name(int style)
{
    if(style == TERMINAL_CURSOR_UNDERLINE)
        return "underline";
    if(style == TERMINAL_CURSOR_BAR)
        return "bar";
    return "block";
}

static int hex_value(int ch)
{
    if(ch >= '0' && ch <= '9')
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    if(ch >= 'A' && ch <= 'F')
        return 10 + ch - 'A';
    return -1;
}

static int parse_color_value(const char *text, int *out)
{
    int r;
    int g;
    int b;
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;

    if(out == NULL || text == NULL)
        return 0;
    if(strcmp(text, "default") == 0 || strcmp(text, "system") == 0 ||
       strcmp(text, "theme") == 0) {
        *out = COLOR_DEFAULT;
        return 1;
    }
    if(text[0] == '#')
        text++;
    if(strlen(text) != 6)
        return 0;
    r1 = hex_value((unsigned char)text[0]);
    r2 = hex_value((unsigned char)text[1]);
    g1 = hex_value((unsigned char)text[2]);
    g2 = hex_value((unsigned char)text[3]);
    b1 = hex_value((unsigned char)text[4]);
    b2 = hex_value((unsigned char)text[5]);
    if(r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
        return 0;
    r = (r1 << 4) | r2;
    g = (g1 << 4) | g2;
    b = (b1 << 4) | b2;
    *out = COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
    return 1;
}

static void write_color(FILE *file, const char *name, int color)
{
    if(file == NULL || name == NULL || color == COLOR_DEFAULT)
        return;
    if((color & COLOR_TRUE_RGB) != 0)
        fprintf(file, "%s=#%06x\n", name, color & 0xffffff);
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
    if(config == NULL || name == NULL || value == NULL)
        return;
    if(strcmp(name, "font_size") == 0 || strcmp(name, "font-size") == 0) {
        int n = atoi(value);

        if(n >= 10 && n <= 48)
            config->font_size = n;
    } else if(strcmp(name, "padding") == 0) {
        int n = atoi(value);

        if(n >= 0 && n <= 48)
            config->padding = n;
    } else if(strcmp(name, "scrollback") == 0 ||
              strcmp(name, "scrollback_limit") == 0) {
        int n = atoi(value);

        if(n >= 100 && n <= 100000)
            config->scrollback_limit = n;
    } else if(strcmp(name, "cursor_style") == 0 ||
              strcmp(name, "cursor-style") == 0) {
        if(strcmp(value, "block") == 0 || strcmp(value, "1") == 0)
            config->cursor_style = TERMINAL_CURSOR_BLOCK;
        else if(strcmp(value, "underline") == 0 || strcmp(value, "2") == 0)
            config->cursor_style = TERMINAL_CURSOR_UNDERLINE;
        else if(strcmp(value, "bar") == 0 || strcmp(value, "beam") == 0 ||
                strcmp(value, "3") == 0)
            config->cursor_style = TERMINAL_CURSOR_BAR;
    } else if(strcmp(name, "shell") == 0) {
        copy_text(config->shell, (int)sizeof(config->shell), value);
    } else if(strcmp(name, "working_directory") == 0 ||
              strcmp(name, "working-directory") == 0) {
        copy_text(config->working_directory,
                  (int)sizeof(config->working_directory), value);
    } else if(strcmp(name, "command") == 0) {
        copy_text(config->command, (int)sizeof(config->command), value);
    } else if(strcmp(name, "terminal_font") == 0 ||
              strcmp(name, "terminal-font") == 0 ||
              strcmp(name, "font") == 0) {
        copy_text(config->terminal_font, (int)sizeof(config->terminal_font),
                  value);
    } else if(strcmp(name, "terminal_foreground") == 0 ||
              strcmp(name, "terminal-foreground") == 0 ||
              strcmp(name, "foreground") == 0) {
        int color;

        if(parse_color_value(value, &color))
            config->terminal_foreground = color;
    } else if(strcmp(name, "terminal_background") == 0 ||
              strcmp(name, "terminal-background") == 0 ||
              strcmp(name, "background") == 0) {
        int color;

        if(parse_color_value(value, &color))
            config->terminal_background = color;
    }
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
        name = line;
        value = equals + 1;
        while(*name == ' ' || *name == '\t')
            name++;
        while(*value == ' ' || *value == '\t')
            value++;
        config_apply_arg(config, name, value);
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
    fprintf(file, "padding=%d\n", config->padding);
    fprintf(file, "scrollback=%d\n", config->scrollback_limit);
    fprintf(file, "cursor_style=%s\n", cursor_style_name(config->cursor_style));
    if(config->shell[0] != '\0')
        fprintf(file, "shell=%s\n", config->shell);
    if(config->working_directory[0] != '\0')
        fprintf(file, "working_directory=%s\n", config->working_directory);
    if(config->command[0] != '\0')
        fprintf(file, "command=%s\n", config->command);
    if(config->terminal_font[0] != '\0')
        fprintf(file, "terminal_font=%s\n", config->terminal_font);
    write_color(file, "terminal_foreground", config->terminal_foreground);
    write_color(file, "terminal_background", config->terminal_background);
    fclose(file);
    return 1;
}
