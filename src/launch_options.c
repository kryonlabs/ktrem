#include "launch_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, int error_size, const char *message,
                      const char *arg)
{
    if(error == NULL || error_size <= 0)
        return;
    if(message == NULL)
        message = "invalid option";
    if(arg == NULL)
        snprintf(error, (size_t)error_size, "%s", message);
    else
        snprintf(error, (size_t)error_size, "%s: %s", message, arg);
}

static const char *option_value(const char *arg, const char *long_name)
{
    size_t len;

    if(arg == NULL || long_name == NULL)
        return NULL;
    len = strlen(long_name);
    if(strncmp(arg, long_name, len) == 0 && arg[len] == '=')
        return arg + len + 1;
    return NULL;
}

static int parse_geometry(LaunchOptions *options, const char *value)
{
    char *end;
    long cols;
    long rows;

    if(options == NULL || value == NULL || value[0] == '\0')
        return 0;
    cols = strtol(value, &end, 10);
    if(end == value || (*end != 'x' && *end != 'X'))
        return 0;
    rows = strtol(end + 1, &end, 10);
    if(cols < 8 || cols > 1000 || rows < 4 || rows > 1000)
        return 0;
    options->geometry_cols = (int)cols;
    options->geometry_rows = (int)rows;
    return 1;
}

static int tab_has_launch_options(const LaunchTabSpec *tab)
{
    if(tab == NULL)
        return 0;
    return tab->title[0] != '\0' || tab->working_directory[0] != '\0' ||
           tab->command[0] != '\0';
}

static void copy_text(char *dst, int dst_size, const char *src)
{
    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, (size_t)dst_size, "%s", src);
}

static void seed_tab_from_config(LaunchTabSpec *tab, const Config *config,
                                 const char *title)
{
    if(tab == NULL)
        return;
    memset(tab, 0, sizeof(*tab));
    if(config != NULL) {
        copy_text(tab->working_directory, (int)sizeof(tab->working_directory),
                  config->working_directory);
        copy_text(tab->command, (int)sizeof(tab->command), config->command);
    }
    copy_text(tab->title, (int)sizeof(tab->title), title);
}

static int append_launch_tab(LaunchOptions *options, const Config *config,
                             const char *title, char *error, int error_size)
{
    if(options == NULL)
        return -1;
    if(options->tab_count >= LAUNCH_MAX_TABS) {
        set_error(error, error_size, "too many tabs", NULL);
        return -1;
    }
    seed_tab_from_config(&options->tabs[options->tab_count], config, title);
    return options->tab_count++;
}

static int begin_launch_tab(LaunchOptions *options, const Config *config,
                            const Config *base_config, int *active_tab,
                            int *implicit_tab_has_options,
                            char *error, int error_size)
{
    int index;

    if(options == NULL || active_tab == NULL ||
       implicit_tab_has_options == NULL)
        return 0;
    if(options->tab_count == 0 && *active_tab < 0 &&
       *implicit_tab_has_options) {
        if(append_launch_tab(options, config, options->initial_title, error,
                             error_size) < 0)
            return 0;
        options->initial_title[0] = '\0';
    }
    index = append_launch_tab(options, base_config, NULL, error, error_size);
    if(index < 0)
        return 0;
    *active_tab = index;
    *implicit_tab_has_options = 0;
    return 1;
}

static int append_shell_quoted(char *out, int out_size, const char *arg)
{
    int used;
    const char *p;

    if(out == NULL || out_size <= 0 || arg == NULL)
        return 0;
    used = (int)strlen(out);
    if(used + 3 >= out_size)
        return 0;
    out[used++] = '\'';
    out[used] = '\0';
    for(p = arg; *p != '\0'; p++) {
        if(*p == '\'') {
            if(used + 4 >= out_size)
                return 0;
            memcpy(out + used, "'\\''", 4);
            used += 4;
        } else {
            if(used + 1 >= out_size)
                return 0;
            out[used++] = *p;
        }
        out[used] = '\0';
    }
    if(used + 2 >= out_size)
        return 0;
    out[used++] = '\'';
    out[used] = '\0';
    return 1;
}

static int join_execute_command(char *out, int out_size, int start, int argc,
                                char **argv)
{
    int i;

    if(out == NULL || out_size <= 0 || start >= argc)
        return 0;
    out[0] = '\0';
    for(i = start; i < argc; i++) {
        size_t len = strlen(out);

        if(i > start) {
            if(len + 2 >= (size_t)out_size)
                return 0;
            out[len++] = ' ';
            out[len] = '\0';
        }
        if(!append_shell_quoted(out, out_size, argv[i]))
            return 0;
    }
    return out[0] != '\0';
}

static int apply_config_value_arg(Config *config, const char *arg)
{
    static const char *names[] = {
        "--font-size",
        "--shell",
        "--scrollback",
        "--cursor-style",
        "--terminal-font",
        "--terminal-foreground",
        "--terminal-background",
        "--terminal-cursor",
        "--terminal-selection-foreground",
        "--terminal-selection-background",
        NULL
    };
    int i;

    if(config == NULL || arg == NULL)
        return 0;
    for(i = 0; names[i] != NULL; i++) {
        const char *value = option_value(arg, names[i]);

        if(value != NULL) {
            config_apply_arg(config, names[i] + 2, value);
            return 1;
        }
    }
    return 0;
}

void launch_options_defaults(LaunchOptions *options)
{
    if(options == NULL)
        return;
    memset(options, 0, sizeof(*options));
    options->show_menubar = 1;
    options->show_toolbar = 1;
    options->show_borders = 1;
}

LaunchParseResult launch_options_parse(LaunchOptions *options, Config *config,
                                       int argc, char **argv,
                                       char *error, int error_size)
{
    Config base_config;
    int i;
    int active_tab = -1;
    int implicit_tab_has_options = 0;

    if(error != NULL && error_size > 0)
        error[0] = '\0';
    if(options == NULL || config == NULL)
        return LAUNCH_PARSE_ERROR;
    base_config = *config;
    for(i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if(arg == NULL)
            continue;
        if(strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
            return LAUNCH_PARSE_HELP;
        if(strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0)
            return LAUNCH_PARSE_VERSION;
        if(strcmp(arg, "--color-table") == 0)
            return LAUNCH_PARSE_COLOR_TABLE;
        if(strcmp(arg, "--disable-server") == 0)
            continue;
        if(strcmp(arg, "--drop-down") == 0) {
            options->drop_down = 1;
            options->show_borders = 0;
            options->show_menubar = 0;
            options->show_toolbar = 0;
            continue;
        }
        if(strcmp(arg, "--tab") == 0 || strcmp(arg, "--window") == 0) {
            if(!begin_launch_tab(options, config, &base_config, &active_tab,
                                 &implicit_tab_has_options, error,
                                 error_size))
                return LAUNCH_PARSE_ERROR;
            continue;
        }
        if(strcmp(arg, "--fullscreen") == 0) {
            options->fullscreen = 1;
            continue;
        }
        if(strcmp(arg, "--maximize") == 0) {
            options->maximize = 1;
            continue;
        }
        if(strcmp(arg, "--show-menubar") == 0) {
            options->show_menubar = 1;
            continue;
        }
        if(strcmp(arg, "--hide-menubar") == 0) {
            options->show_menubar = 0;
            continue;
        }
        if(strcmp(arg, "--show-toolbar") == 0) {
            options->show_toolbar = 1;
            continue;
        }
        if(strcmp(arg, "--hide-toolbar") == 0) {
            options->show_toolbar = 0;
            continue;
        }
        if(strcmp(arg, "--show-borders") == 0) {
            options->show_borders = 1;
            continue;
        }
        if(strcmp(arg, "--hide-borders") == 0) {
            options->show_borders = 0;
            continue;
        }
        if(strcmp(arg, "--hold") == 0 || strcmp(arg, "-H") == 0) {
            options->hold = 1;
            continue;
        }
        if(strcmp(arg, "--execute") == 0 || strcmp(arg, "-x") == 0) {
            char command[1024];

            if(!join_execute_command(command, (int)sizeof(command), i + 1,
                                     argc, argv)) {
                set_error(error, error_size,
                          "execute option requires a command", arg);
                return LAUNCH_PARSE_ERROR;
            }
            if(active_tab >= 0)
                copy_text(options->tabs[active_tab].command,
                          (int)sizeof(options->tabs[active_tab].command),
                          command);
            else {
                config_apply_arg(config, "command", command);
                implicit_tab_has_options = 1;
            }
            break;
        }
        value = option_value(arg, "--command");
        if(value != NULL) {
            if(active_tab >= 0)
                copy_text(options->tabs[active_tab].command,
                          (int)sizeof(options->tabs[active_tab].command),
                          value);
            else {
                config_apply_arg(config, "command", value);
                implicit_tab_has_options = 1;
            }
            continue;
        }
        value = option_value(arg, "--working-directory");
        if(value != NULL) {
            if(active_tab >= 0)
                copy_text(options->tabs[active_tab].working_directory,
                          (int)sizeof(options->tabs[active_tab].
                                      working_directory),
                          value);
            else {
                config_apply_arg(config, "working-directory", value);
                implicit_tab_has_options = 1;
            }
            continue;
        }
        value = option_value(arg, "--default-working-directory");
        if(value != NULL) {
            if(active_tab >= 0)
                copy_text(options->tabs[active_tab].working_directory,
                          (int)sizeof(options->tabs[active_tab].
                                      working_directory),
                          value);
            else {
                config_apply_arg(config, "working-directory", value);
                implicit_tab_has_options = 1;
            }
            continue;
        }
        value = option_value(arg, "--title");
        if(value != NULL) {
            if(active_tab >= 0)
                copy_text(options->tabs[active_tab].title,
                          (int)sizeof(options->tabs[active_tab].title),
                          value);
            else {
                copy_text(options->initial_title,
                          (int)sizeof(options->initial_title), value);
                implicit_tab_has_options = 1;
            }
            continue;
        }
        value = option_value(arg, "--geometry");
        if(value != NULL) {
            if(!parse_geometry(options, value)) {
                set_error(error, error_size, "invalid geometry", value);
                return LAUNCH_PARSE_ERROR;
            }
            continue;
        }
        if(apply_config_value_arg(config, arg))
            continue;
        if((strcmp(arg, "--font-size") == 0 ||
            strcmp(arg, "--working-directory") == 0 ||
            strcmp(arg, "--default-working-directory") == 0 ||
            strcmp(arg, "--shell") == 0 ||
            strcmp(arg, "--command") == 0 ||
            strcmp(arg, "-e") == 0 ||
            strcmp(arg, "--scrollback") == 0 ||
            strcmp(arg, "--cursor-style") == 0 ||
            strcmp(arg, "--terminal-font") == 0 ||
            strcmp(arg, "--terminal-foreground") == 0 ||
            strcmp(arg, "--terminal-background") == 0 ||
            strcmp(arg, "--terminal-cursor") == 0 ||
            strcmp(arg, "--terminal-selection-foreground") == 0 ||
            strcmp(arg, "--terminal-selection-background") == 0 ||
            strcmp(arg, "--default-display") == 0 ||
            strcmp(arg, "--display") == 0 ||
            strcmp(arg, "--role") == 0 ||
            strcmp(arg, "--startup-id") == 0 ||
            strcmp(arg, "--icon") == 0 ||
            strcmp(arg, "-I") == 0 ||
            strcmp(arg, "--title") == 0 ||
            strcmp(arg, "-T") == 0 ||
            strcmp(arg, "--geometry") == 0) && i + 1 < argc) {
            value = argv[++i];
            if(strcmp(arg, "--working-directory") == 0 ||
               strcmp(arg, "--default-working-directory") == 0) {
                if(active_tab >= 0)
                    copy_text(options->tabs[active_tab].working_directory,
                              (int)sizeof(options->tabs[active_tab].
                                          working_directory),
                              value);
                else {
                    config_apply_arg(config, "working-directory", value);
                    implicit_tab_has_options = 1;
                }
            } else if(strcmp(arg, "--command") == 0 ||
                      strcmp(arg, "-e") == 0) {
                if(active_tab >= 0)
                    copy_text(options->tabs[active_tab].command,
                              (int)sizeof(options->tabs[active_tab].command),
                              value);
                else {
                    config_apply_arg(config, "command", value);
                    implicit_tab_has_options = 1;
                }
            } else if(strcmp(arg, "--title") == 0 || strcmp(arg, "-T") == 0) {
                if(active_tab >= 0)
                    copy_text(options->tabs[active_tab].title,
                              (int)sizeof(options->tabs[active_tab].title),
                              value);
                else {
                    copy_text(options->initial_title,
                              (int)sizeof(options->initial_title), value);
                    implicit_tab_has_options = 1;
                }
            }
            else if(strcmp(arg, "--geometry") == 0) {
                if(!parse_geometry(options, value)) {
                    set_error(error, error_size, "invalid geometry", value);
                    return LAUNCH_PARSE_ERROR;
                }
            } else if(strcmp(arg, "--default-display") == 0 ||
                      strcmp(arg, "--display") == 0 ||
                      strcmp(arg, "--role") == 0 ||
                      strcmp(arg, "--startup-id") == 0 ||
                      strcmp(arg, "--icon") == 0 || strcmp(arg, "-I") == 0) {
                continue;
            } else {
                config_apply_arg(config, arg + 2, value);
            }
            continue;
        }
        value = option_value(arg, "--display");
        if(value != NULL)
            continue;
        value = option_value(arg, "--default-display");
        if(value != NULL)
            continue;
        value = option_value(arg, "--role");
        if(value != NULL)
            continue;
        value = option_value(arg, "--startup-id");
        if(value != NULL)
            continue;
        value = option_value(arg, "--icon");
        if(value != NULL)
            continue;
    }
    if(options->tab_count == 1 &&
       !tab_has_launch_options(&options->tabs[0]) &&
       !implicit_tab_has_options)
        options->tab_count = 0;
    return LAUNCH_PARSE_OK;
}

void launch_options_print_usage(void)
{
    printf("usage: kapsule [options]\n"
           "  --working-directory PATH, --default-working-directory PATH\n"
           "  --shell PATH\n"
           "  --command CMD, --command=CMD, -e CMD\n"
           "  -x, --execute CMD [ARG...]\n"
           "  --tab, --window, --drop-down\n"
           "  --title TITLE, -T TITLE\n"
           "  --hold, -H\n"
           "  --geometry COLSxROWS, --fullscreen, --maximize\n"
           "  --show-menubar, --hide-menubar, --show-toolbar, --hide-toolbar\n"
           "  --show-borders, --hide-borders, --disable-server\n"
           "  --font-size N, --scrollback N, --cursor-style block|underline|bar\n"
           "  --terminal-font PATH\n"
           "  --terminal-foreground #rrggbb|default\n"
           "  --terminal-background #rrggbb|default\n"
           "  --terminal-cursor #rrggbb|default\n"
           "  --terminal-selection-foreground #rrggbb|default\n"
           "  --terminal-selection-background #rrggbb|default\n");
}

void launch_options_print_version(void)
{
    printf("kapsule 0.1\n");
}

void launch_options_print_color_table(void)
{
    int i;

    for(i = 0; i < 16; i++) {
        printf("\033[48;5;%dm  \033[0m %02d", i, i);
        if(i % 8 == 7)
            printf("\n");
        else
            printf("  ");
    }
}
