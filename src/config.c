#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    } else if(strcmp(name, "shell") == 0) {
        copy_text(config->shell, (int)sizeof(config->shell), value);
    } else if(strcmp(name, "working_directory") == 0 ||
              strcmp(name, "working-directory") == 0) {
        copy_text(config->working_directory,
                  (int)sizeof(config->working_directory), value);
    } else if(strcmp(name, "command") == 0) {
        copy_text(config->command, (int)sizeof(config->command), value);
    }
}

void config_load(Config *config)
{
    const char *xdg;
    const char *home;
    char path[1024];
    char line[2048];
    FILE *file;

    if(config == NULL)
        return;
    xdg = getenv("XDG_CONFIG_HOME");
    home = getenv("HOME");
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(path, sizeof(path), "%s/kapsule/config", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(path, sizeof(path), "%s/.config/kapsule/config", home);
    else
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

