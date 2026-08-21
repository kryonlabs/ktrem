#ifndef KAPSULE_LAUNCH_OPTIONS_H
#define KAPSULE_LAUNCH_OPTIONS_H

#include "config.h"

#define LAUNCH_MAX_TABS 8

typedef enum LaunchParseResult {
    LAUNCH_PARSE_OK = 0,
    LAUNCH_PARSE_HELP = 1,
    LAUNCH_PARSE_VERSION = 2,
    LAUNCH_PARSE_COLOR_TABLE = 3,
    LAUNCH_PARSE_ERROR = 4
} LaunchParseResult;

typedef struct LaunchTabSpec {
    char title[128];
    char working_directory[1024];
    char command[1024];
} LaunchTabSpec;

typedef struct LaunchOptions {
    char initial_title[128];
    LaunchTabSpec tabs[LAUNCH_MAX_TABS];
    int tab_count;
    int hold;
    int drop_down;
    int fullscreen;
    int maximize;
    int show_menubar;
    int show_toolbar;
    int show_borders;
    int geometry_cols;
    int geometry_rows;
} LaunchOptions;

void launch_options_defaults(LaunchOptions *options);
LaunchParseResult launch_options_parse(LaunchOptions *options, Config *config,
                                       int argc, char **argv,
                                       char *error, int error_size);
void launch_options_print_usage(void);
void launch_options_print_version(void);
void launch_options_print_color_table(void);

#endif
