#include "kryon.h"
#include "config.h"
#include "input.h"
#include "launch_options.h"
#include "palette.h"
#include "profile.h"
#include "selection.h"
#include "session.h"
#include "session_store.h"
#include "terminal.h"
#include "terminal_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static char test_clipboard[UI_CLIPBOARD_BUFFER_SIZE];
static Color test_theme_background;
static Color test_theme_surface;
static Color test_theme_text;
static Color test_theme_button;
static Color test_theme_button_hover;
static Color test_theme_icon;
static Color test_theme_link;
static int test_theme_dark;

static int color_equals(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Color test_color_fade(Color color, float alpha)
{
    if(alpha < 0.0f)
        alpha = 0.0f;
    if(alpha > 1.0f)
        alpha = 1.0f;
    color.a = (unsigned char)((float)color.a * alpha);
    return color;
}

Color Fade(Color color, float alpha)
{
    return test_color_fade(color, alpha);
}

void SetSystemThemePalette(const char *name, Color background, Color surface,
                           Color text, Color circle, Color button,
                           Color button_hover, Color icon, Color link,
                           bool prefers_dark, bool supports_mode)
{
    (void)name;
    (void)circle;
    (void)supports_mode;
    test_theme_background = background;
    test_theme_surface = surface;
    test_theme_text = text;
    test_theme_button = button;
    test_theme_button_hover = button_hover;
    test_theme_icon = icon;
    test_theme_link = link;
    test_theme_dark = prefers_dark ? 1 : 0;
}

void SetThemeSource(ThemeSource source)
{
    (void)source;
}

void SetThemeMode(ThemeMode mode)
{
    test_theme_dark = mode == THEME_MODE_DARK ? 1 : 0;
}

void SetThemeStyle(ThemeStyle style)
{
    (void)style;
}

ThemeStyle GetEffectiveThemeStyle(void)
{
    return THEME_STYLE_SYSTEM;
}

int GetDefaultThemeForThemeStyle(ThemeStyle style)
{
    (void)style;
    return 0;
}

void SetCurrentTheme(int theme_id, int dark_mode)
{
    (void)theme_id;
    test_theme_dark = dark_mode ? 1 : 0;
}

bool RefreshSystemTheme(void)
{
    return true;
}

void ReloadThemes(void)
{
}

bool GetEffectiveThemeDarkMode(void)
{
    return test_theme_dark ? true : false;
}

Color GetThemeBackground(void)
{
    return test_theme_background;
}

Color GetThemeSurface(void)
{
    return test_theme_surface;
}

Color GetThemeText(void)
{
    return test_theme_text;
}

Color GetThemeButton(void)
{
    return test_theme_button;
}

Color GetThemeButtonHover(void)
{
    return test_theme_button_hover;
}

Color GetThemeIcon(void)
{
    return test_theme_icon;
}

Color GetThemeLink(void)
{
    return test_theme_link;
}

TerminalPaneColors GetTerminalPaneThemeColors(void)
{
    return (TerminalPaneColors){
        test_theme_background,
        test_theme_text,
        test_color_fade(test_theme_text, 0.70f),
        test_color_fade(test_theme_link, 0.28f),
        test_theme_background,
        test_theme_text,
        test_theme_link,
        test_color_fade(test_theme_surface, 0.72f),
        test_color_fade(test_theme_background, 0.82f),
        test_color_fade(test_theme_text, 0.78f),
        test_color_fade(test_theme_link, 0.16f),
        test_color_fade(test_theme_link, 0.85f)
    };
}

TerminalPaneColors ResolveTerminalPaneThemeColors(TerminalPaneColors colors)
{
    TerminalPaneColors fallback = GetTerminalPaneThemeColors();

    if(colors.background.a == 0)
        colors.background = fallback.background;
    if(colors.text.a == 0)
        colors.text = fallback.text;
    if(colors.muted_text.a == 0)
        colors.muted_text = fallback.muted_text;
    if(colors.selection.a == 0)
        colors.selection = fallback.selection;
    if(colors.selection_text.a == 0)
        colors.selection_text = fallback.selection_text;
    if(colors.cursor.a == 0)
        colors.cursor = fallback.cursor;
    if(colors.link.a == 0)
        colors.link = fallback.link;
    if(colors.border.a == 0)
        colors.border = fallback.border;
    if(colors.scroll_indicator.a == 0)
        colors.scroll_indicator = fallback.scroll_indicator;
    if(colors.scroll_indicator_text.a == 0)
        colors.scroll_indicator_text = fallback.scroll_indicator_text;
    if(colors.bell_overlay.a == 0)
        colors.bell_overlay = fallback.bell_overlay;
    if(colors.bell_border.a == 0)
        colors.bell_border = fallback.bell_border;
    return colors;
}

void SetClipboardText(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(test_clipboard, sizeof(test_clipboard), "%s", text);
}

const char *GetClipboardText(void)
{
    return test_clipboard;
}

bool IsKeyPressed(int key)
{
    (void)key;
    return 0;
}

bool IsKeyPressedRepeat(int key)
{
    (void)key;
    return 0;
}

bool IsKeyDown(int key)
{
    (void)key;
    return 0;
}

int GetKeyPressed(void)
{
    return 0;
}

int GetCharPressed(void)
{
    return 0;
}

static int config_terminal_cursor_color_parses(void)
{
    Config config;

    config_defaults(&config);
    if(config.font_size != KAPSULE_DEFAULT_FONT_SIZE ||
       config.scrollback_limit != KAPSULE_DEFAULT_SCROLLBACK_LIMIT) {
        fprintf(stderr, "profile defaults did not use shared constants\n");
        return 0;
    }
    config_apply_arg(&config, "font-size", "48");
    if(config.font_size != KAPSULE_MAX_FONT_SIZE) {
        fprintf(stderr, "font-size max did not parse\n");
        return 0;
    }
    config_apply_arg(&config, "font-size", "49");
    if(config.font_size != KAPSULE_MAX_FONT_SIZE) {
        fprintf(stderr, "font-size above max changed value\n");
        return 0;
    }
    config_apply_arg(&config, "font-size", "10");
    if(config.font_size != KAPSULE_MIN_FONT_SIZE) {
        fprintf(stderr, "font-size min did not parse\n");
        return 0;
    }
    config_apply_arg(&config, "font-size", "9");
    if(config.font_size != KAPSULE_MIN_FONT_SIZE) {
        fprintf(stderr, "font-size below min changed value\n");
        return 0;
    }
    config_apply_arg(&config, "scrollback", "100000");
    if(config.scrollback_limit != KAPSULE_MAX_SCROLLBACK_LIMIT) {
        fprintf(stderr, "scrollback max did not parse\n");
        return 0;
    }
    config_apply_arg(&config, "scrollback", "100001");
    if(config.scrollback_limit != KAPSULE_MAX_SCROLLBACK_LIMIT) {
        fprintf(stderr, "scrollback above max changed value\n");
        return 0;
    }
    config_apply_arg(&config, "scrollback", "100");
    if(config.scrollback_limit != KAPSULE_MIN_SCROLLBACK_LIMIT) {
        fprintf(stderr, "scrollback min did not parse\n");
        return 0;
    }
    config_apply_arg(&config, "scrollback", "99");
    if(config.scrollback_limit != KAPSULE_MIN_SCROLLBACK_LIMIT) {
        fprintf(stderr, "scrollback below min changed value\n");
        return 0;
    }
    if(config.terminal_cursor != COLOR_DEFAULT) {
        fprintf(stderr, "terminal cursor default should follow theme\n");
        return 0;
    }
    if(config.terminal_selection_foreground != COLOR_DEFAULT ||
       config.terminal_selection_background != COLOR_DEFAULT) {
        fprintf(stderr, "terminal selection defaults should follow theme\n");
        return 0;
    }
    config_apply_arg(&config, "terminal_cursor", "#123456");
    if(config.terminal_cursor != (COLOR_TRUE_RGB | 0x123456)) {
        fprintf(stderr, "terminal_cursor did not parse hex color\n");
        return 0;
    }
    config_apply_arg(&config, "terminal-cursor", "default");
    if(config.terminal_cursor != COLOR_DEFAULT) {
        fprintf(stderr, "terminal-cursor default did not reset to theme\n");
        return 0;
    }
    config_apply_arg(&config, "cursor-color", "abcdef");
    if(config.terminal_cursor != (COLOR_TRUE_RGB | 0xabcdef)) {
        fprintf(stderr, "cursor-color alias did not parse hex color\n");
        return 0;
    }
    config_apply_arg(&config, "terminal-selection-foreground", "#010203");
    config_apply_arg(&config, "selection-background", "040506");
    if(config.terminal_selection_foreground != (COLOR_TRUE_RGB | 0x010203) ||
       config.terminal_selection_background != (COLOR_TRUE_RGB | 0x040506)) {
        fprintf(stderr, "selection color profile values did not parse\n");
        return 0;
    }
    config_apply_arg(&config, "selection-foreground", "default");
    config_apply_arg(&config, "terminal_selection_background", "default");
    if(config.terminal_selection_foreground != COLOR_DEFAULT ||
       config.terminal_selection_background != COLOR_DEFAULT) {
        fprintf(stderr, "selection color profile values did not reset\n");
        return 0;
    }
    return 1;
}

static int config_save_load_escapes_profile_text(void)
{
    char dir[] = "/tmp/kapsule-config-test-XXXXXX";
    const char *old_xdg;
    char old_xdg_copy[1024];
    Config saved;
    Config loaded;
    int ok = 1;

    old_xdg = getenv("XDG_CONFIG_HOME");
    if(old_xdg != NULL)
        snprintf(old_xdg_copy, sizeof(old_xdg_copy), "%s", old_xdg);
    else
        old_xdg_copy[0] = '\0';
    if(mkdtemp(dir) == NULL) {
        fprintf(stderr, "config save/load: mkdtemp failed\n");
        return 0;
    }
    setenv("XDG_CONFIG_HOME", dir, 1);
    config_defaults(&saved);
    config_apply_arg(&saved, "font-size", "18");
    config_apply_arg(&saved, "scrollback", "12000");
    config_apply_arg(&saved, "cursor-style", "bar");
    config_apply_arg(&saved, "shell", "/bin/sh");
    config_apply_arg(&saved, "working_directory", "/tmp/kapsule profile");
    config_apply_arg(&saved, "command", "printf 'a\\\\b'\nnext");
    config_apply_arg(&saved, "terminal_font", "/tmp/fonts/mono\\tab\t.ttf");
    config_apply_arg(&saved, "terminal-cursor", "#123456");
    config_apply_arg(&saved, "terminal-selection-foreground", "#abcdef");
    config_apply_arg(&saved, "terminal-selection-background", "#654321");
    if(!config_save(&saved)) {
        fprintf(stderr, "config save/load: save failed\n");
        ok = 0;
    }
    config_defaults(&loaded);
    config_load(&loaded);
    if(loaded.font_size != 18 ||
       loaded.scrollback_limit != 12000 ||
       loaded.cursor_style != TERMINAL_CURSOR_BAR ||
       strcmp(loaded.shell, saved.shell) != 0 ||
       strcmp(loaded.working_directory, saved.working_directory) != 0 ||
       strcmp(loaded.command, saved.command) != 0 ||
       strcmp(loaded.terminal_font, saved.terminal_font) != 0 ||
       loaded.terminal_cursor != saved.terminal_cursor ||
       loaded.terminal_selection_foreground !=
           saved.terminal_selection_foreground ||
       loaded.terminal_selection_background !=
           saved.terminal_selection_background) {
        fprintf(stderr, "config save/load did not preserve escaped profile text\n");
        ok = 0;
    }
    if(old_xdg != NULL)
        setenv("XDG_CONFIG_HOME", old_xdg_copy, 1);
    else
        unsetenv("XDG_CONFIG_HOME");
    return ok;
}

static int launch_options_accept_xfce_aliases(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "--default-working-directory=/tmp/project",
        "--command=printf ok",
        "--font-size=20",
        "--title",
        "Build",
        "--geometry=120x40",
        "--hold",
        "--maximize",
        "--hide-menubar",
        "--show-toolbar",
        "--hide-borders",
        "--disable-server"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) != LAUNCH_PARSE_OK) {
        fprintf(stderr, "xfce alias parse failed: %s\n", error);
        return 0;
    }
    if(strcmp(config.working_directory, "/tmp/project") != 0 ||
       strcmp(config.command, "printf ok") != 0 ||
       config.font_size != 20 ||
       strcmp(options.initial_title, "Build") != 0 ||
       options.geometry_cols != 120 || options.geometry_rows != 40 ||
       !options.hold || !options.maximize || options.show_menubar ||
       !options.show_toolbar || options.show_borders) {
        fprintf(stderr, "xfce alias parse produced wrong state\n");
        return 0;
    }
    return 1;
}

static int launch_options_execute_quotes_remainder(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "-x",
        "printf",
        "a b",
        "c'd"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) != LAUNCH_PARSE_OK) {
        fprintf(stderr, "execute parse failed: %s\n", error);
        return 0;
    }
    if(strcmp(config.command, "'printf' 'a b' 'c'\\''d'") != 0) {
        fprintf(stderr, "execute command was not shell quoted: %s\n",
                config.command);
        return 0;
    }
    return 1;
}

static int launch_options_reject_bad_geometry(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {"kapsule", "--geometry", "wide"};

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) !=
       LAUNCH_PARSE_ERROR) {
        fprintf(stderr, "bad geometry was accepted\n");
        return 0;
    }
    return 1;
}

static int launch_options_accept_drop_down(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "--default-display=:7",
        "--drop-down",
        "--tab",
        "--title",
        "System",
        "--command=top"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) != LAUNCH_PARSE_OK) {
        fprintf(stderr, "drop-down parse failed: %s\n", error);
        return 0;
    }
    if(!options.drop_down || options.show_borders ||
       options.tab_count != 1 ||
       strcmp(options.tabs[0].title, "System") != 0 ||
       strcmp(options.tabs[0].command, "top") != 0) {
        fprintf(stderr, "drop-down parse produced wrong state\n");
        return 0;
    }
    return 1;
}

static int launch_options_builds_tab_specs(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "--command=make test",
        "--title",
        "Build",
        "--tab",
        "--default-working-directory",
        "/tmp/logs",
        "--title=Logs",
        "--command=tail -f app.log",
        "--tab",
        "-T",
        "Shell"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) != LAUNCH_PARSE_OK) {
        fprintf(stderr, "tab spec parse failed: %s\n", error);
        return 0;
    }
    if(options.tab_count != 3 ||
       strcmp(options.tabs[0].title, "Build") != 0 ||
       strcmp(options.tabs[0].command, "make test") != 0 ||
       strcmp(options.tabs[1].title, "Logs") != 0 ||
       strcmp(options.tabs[1].working_directory, "/tmp/logs") != 0 ||
       strcmp(options.tabs[1].command, "tail -f app.log") != 0 ||
       strcmp(options.tabs[2].title, "Shell") != 0) {
        fprintf(stderr, "tab spec parse produced wrong state\n");
        return 0;
    }
    return 1;
}

static int launch_options_tab_separator_at_start(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "--tab",
        "--title",
        "One",
        "--window",
        "--command",
        "pwd"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) != LAUNCH_PARSE_OK) {
        fprintf(stderr, "leading tab separator parse failed: %s\n", error);
        return 0;
    }
    if(options.tab_count != 2 ||
       strcmp(options.tabs[0].title, "One") != 0 ||
       strcmp(options.tabs[1].command, "pwd") != 0) {
        fprintf(stderr, "leading tab separator state was wrong\n");
        return 0;
    }
    return 1;
}

static int launch_options_reject_too_many_tabs(void)
{
    Config config;
    LaunchOptions options;
    char error[256];
    char *argv[] = {
        "kapsule",
        "--tab",
        "--tab",
        "--tab",
        "--tab",
        "--tab",
        "--tab",
        "--tab",
        "--tab",
        "--tab"
    };

    config_defaults(&config);
    launch_options_defaults(&options);
    if(launch_options_parse(&options, &config,
                            (int)(sizeof(argv) / sizeof(argv[0])), argv,
                            error, (int)sizeof(error)) !=
       LAUNCH_PARSE_ERROR) {
        fprintf(stderr, "too many launch tabs were accepted\n");
        return 0;
    }
    return 1;
}

static int session_store_roundtrips_escaped_tabs(void)
{
    char dir[] = "/tmp/kapsule-session-test-XXXXXX";
    const char *old_xdg;
    char old_xdg_copy[1024];
    Session sessions[2];
    SessionRecord records[2];
    int active = -1;
    int count;
    int ok = 1;

    old_xdg = getenv("XDG_STATE_HOME");
    if(old_xdg != NULL)
        snprintf(old_xdg_copy, sizeof(old_xdg_copy), "%s", old_xdg);
    else
        old_xdg_copy[0] = '\0';
    if(mkdtemp(dir) == NULL) {
        fprintf(stderr, "session store: mkdtemp failed\n");
        return 0;
    }
    setenv("XDG_STATE_HOME", dir, 1);
    session_init(&sessions[0]);
    session_init(&sessions[1]);
    snprintf(sessions[0].cwd, sizeof(sessions[0].cwd), "/tmp/one\ttab");
    snprintf(sessions[0].shell, sizeof(sessions[0].shell), "/bin/sh");
    snprintf(sessions[0].command, sizeof(sessions[0].command),
             "printf 'a\\\\b'\nnext");
    sessions[0].scroll_offset = 42;
    session_set_title(&sessions[0], "Manual\tTitle");
    snprintf(sessions[1].cwd, sizeof(sessions[1].cwd), "/tmp/two");
    snprintf(sessions[1].shell, sizeof(sessions[1].shell), "/bin/bash");
    snprintf(sessions[1].command, sizeof(sessions[1].command), "top");
    sessions[1].scroll_offset = 7;
    if(!session_store_save(sessions, 2, 1)) {
        fprintf(stderr, "session store: save failed\n");
        ok = 0;
    }
    memset(records, 0, sizeof(records));
    count = session_store_load(records, 2, &active);
    if(count != 2 || active != 1 ||
       strcmp(records[0].cwd, sessions[0].cwd) != 0 ||
       strcmp(records[0].shell, sessions[0].shell) != 0 ||
       strcmp(records[0].command, sessions[0].command) != 0 ||
       strcmp(records[0].title, "Manual\tTitle") != 0 ||
       !records[0].title_override ||
       records[0].scroll_offset != 42 ||
       strcmp(records[1].cwd, sessions[1].cwd) != 0 ||
       strcmp(records[1].shell, sessions[1].shell) != 0 ||
       strcmp(records[1].command, sessions[1].command) != 0 ||
       records[1].title[0] != '\0' ||
       records[1].title_override ||
       records[1].scroll_offset != 7) {
        fprintf(stderr, "session store did not roundtrip escaped records\n");
        ok = 0;
    }
    {
        char path[1024];
        FILE *file;

        if(!session_store_state_path(path, sizeof(path))) {
            fprintf(stderr, "session store: path failed\n");
            ok = 0;
        } else {
            file = fopen(path, "w");
            if(file == NULL) {
                fprintf(stderr, "session store: old format write failed\n");
                ok = 0;
            } else {
                fputs("active=0\n"
                      "tab\t/tmp/old\t/bin/sh\tOld Title\t1\told-command\n",
                      file);
                fclose(file);
                memset(records, 0xff, sizeof(records));
                count = session_store_load(records, 2, &active);
                if(count != 1 || active != 0 ||
                   strcmp(records[0].cwd, "/tmp/old") != 0 ||
                   strcmp(records[0].shell, "/bin/sh") != 0 ||
                   strcmp(records[0].title, "Old Title") != 0 ||
                   !records[0].title_override ||
                   strcmp(records[0].command, "old-command") != 0 ||
                   records[0].scroll_offset != 0) {
                    fprintf(stderr,
                            "session store old format compatibility failed\n");
                    ok = 0;
                }
            }
        }
    }
    session_close(&sessions[0]);
    session_close(&sessions[1]);
    if(old_xdg != NULL)
        setenv("XDG_STATE_HOME", old_xdg_copy, 1);
    else
        unsetenv("XDG_STATE_HOME");
    return ok;
}

static void test_palette_defaults(Palette *palette)
{
    if(palette == NULL)
        return;
    memset(palette, 0, sizeof(*palette));
    palette->foreground = (Color){224, 228, 235, 255};
    palette->terminal_background = (Color){20, 20, 20, 255};
    palette->cursor = palette->foreground;
    palette->selection = (Color){62, 108, 167, 190};
    palette->selection_text = (Color){1, 2, 3, 255};
    palette->scroll_indicator = (Color){10, 11, 12, 220};
    palette->scroll_indicator_text = (Color){220, 221, 222, 255};
    palette->bell_overlay = (Color){30, 40, 50, 50};
    palette->bell_border = (Color){30, 40, 50, 220};
}

static int palette_defaults_follow_kryon_theme_tokens(void)
{
    Palette palette;
    TerminalPaneColors terminal_colors;
    TerminalPanePalette terminal_palette;

    SetSystemThemePalette("Kapsule Test",
                          (Color){10, 20, 30, 255},
                          (Color){40, 50, 60, 255},
                          (Color){210, 220, 230, 255},
                          (Color){70, 80, 90, 255},
                          (Color){90, 100, 110, 255},
                          (Color){120, 130, 140, 255},
                          (Color){150, 160, 170, 255},
                          (Color){25, 120, 220, 255},
                          0, 0);
    SetThemeSource(THEME_SOURCE_SYSTEM);
    SetThemeMode(THEME_MODE_LIGHT);
    SetThemeStyle(THEME_STYLE_SYSTEM);
    SetCurrentTheme(GetDefaultThemeForThemeStyle(GetEffectiveThemeStyle()), 0);
    terminal_colors = GetTerminalPaneThemeColors();
    terminal_palette = GetTerminalPaneDefaultPalette();
    palette_default(&palette);
    if(!color_equals(palette.background, GetThemeBackground()) ||
       !color_equals(palette.chrome, GetThemeSurface()) ||
       !color_equals(palette.menu_text, GetThemeText()) ||
       !color_equals(palette.tab, GetThemeButton()) ||
       !color_equals(palette.tab_active, GetThemeButtonHover()) ||
       !color_equals(palette.link, terminal_colors.link) ||
       !color_equals(palette.terminal_background, terminal_colors.background) ||
       !color_equals(palette.foreground, terminal_colors.text) ||
       !color_equals(palette.selection, terminal_colors.selection) ||
       !color_equals(palette.selection_text, terminal_colors.selection_text) ||
       !color_equals(palette.cursor, terminal_colors.cursor) ||
       !color_equals(palette.scroll_indicator,
                     terminal_colors.scroll_indicator) ||
       !color_equals(palette.scroll_indicator_text,
                     terminal_colors.scroll_indicator_text) ||
       !color_equals(palette.bell_overlay, terminal_colors.bell_overlay) ||
       !color_equals(palette.bell_border, terminal_colors.bell_border) ||
       !color_equals(palette.terminal_palette.ansi[1],
                     terminal_palette.ansi[1]) ||
       !color_equals(palette.terminal_palette.ansi[232],
                     terminal_palette.ansi[232])) {
        fprintf(stderr, "palette defaults did not follow Kryon theme tokens\n");
        return 0;
    }
    return 1;
}

static int profile_applies_configured_terminal_colors(void)
{
    Config config;
    Palette palette;
    TerminalState terminal;

    config_defaults(&config);
    test_palette_defaults(&palette);
    config_apply_arg(&config, "terminal-foreground", "#112233");
    config_apply_arg(&config, "terminal-background", "#445566");
    config_apply_arg(&config, "terminal-cursor", "#778899");
    config_apply_arg(&config, "terminal-selection-foreground", "#aabbcc");
    config_apply_arg(&config, "terminal-selection-background", "#ddeeff");
    terminal_init(&terminal);
    terminal_profile_apply_new(&config, &palette, &terminal);
    if(terminal.base_fg != (COLOR_TRUE_RGB | 0x112233) ||
       terminal.base_bg != (COLOR_TRUE_RGB | 0x445566) ||
       terminal.default_fg != (COLOR_TRUE_RGB | 0x112233) ||
       terminal.default_bg != (COLOR_TRUE_RGB | 0x445566) ||
       terminal.base_cursor_color != (COLOR_TRUE_RGB | 0x778899) ||
       terminal.cursor_color != (COLOR_TRUE_RGB | 0x778899) ||
       terminal.base_selection_fg != (COLOR_TRUE_RGB | 0xaabbcc) ||
       terminal.selection_fg != (COLOR_TRUE_RGB | 0xaabbcc) ||
       terminal.base_selection_bg != (COLOR_TRUE_RGB | 0xddeeff) ||
       terminal.selection_bg != (COLOR_TRUE_RGB | 0xddeeff)) {
        fprintf(stderr, "profile configured colors not applied\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int profile_sync_preserves_terminal_overrides(void)
{
    Config old_config;
    Config new_config;
    Palette palette;
    TerminalState terminal;
    TerminalProfileColors old_colors;
    TerminalProfileColors new_colors;

    config_defaults(&old_config);
    config_defaults(&new_config);
    test_palette_defaults(&palette);
    config_apply_arg(&old_config, "terminal-foreground", "#101010");
    config_apply_arg(&old_config, "terminal-background", "#202020");
    config_apply_arg(&old_config, "terminal-cursor", "#303030");
    config_apply_arg(&new_config, "terminal-foreground", "#aaaaaa");
    config_apply_arg(&new_config, "terminal-background", "#bbbbbb");
    config_apply_arg(&new_config, "terminal-cursor", "#cccccc");
    old_colors = terminal_profile_colors(&old_config, &palette);
    new_colors = terminal_profile_colors(&new_config, &palette);
    terminal_init(&terminal);
    terminal_profile_apply_new(&old_config, &palette, &terminal);
    terminal.default_fg = COLOR_TRUE_RGB | 0x010203;
    terminal.cursor_color = COLOR_TRUE_RGB | 0x040506;
    terminal.selection_bg = COLOR_TRUE_RGB | 0x070809;
    terminal_profile_sync_changed(&terminal, old_colors, new_colors);
    if(terminal.base_fg != (COLOR_TRUE_RGB | 0xaaaaaa) ||
       terminal.base_bg != (COLOR_TRUE_RGB | 0xbbbbbb) ||
       terminal.base_cursor_color != (COLOR_TRUE_RGB | 0xcccccc) ||
       terminal.base_selection_fg != (COLOR_TRUE_RGB | 0x010203) ||
       terminal.selection_fg != (COLOR_TRUE_RGB | 0x010203) ||
       terminal.default_bg != (COLOR_TRUE_RGB | 0xbbbbbb) ||
       terminal.default_fg != (COLOR_TRUE_RGB | 0x010203) ||
       terminal.cursor_color != (COLOR_TRUE_RGB | 0x040506) ||
       terminal.selection_bg != (COLOR_TRUE_RGB | 0x070809)) {
        fprintf(stderr, "profile sync did not preserve terminal overrides\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int line_has(TerminalState *terminal, const char *needle)
{
    char line[256];
    int row;

    for(row = 0; row < terminal->rows; row++) {
        terminal_line(terminal, row, line, sizeof(line));
        if(strstr(line, needle) != NULL)
            return 1;
    }
    return 0;
}

static int line_equals(TerminalState *terminal, int row, const char *expected)
{
    char line[256];

    terminal_line(terminal, row, line, sizeof(line));
    if(strcmp(line, expected) != 0) {
        fprintf(stderr, "row %d: expected '%s', got '%s'\n", row, expected,
                line);
        return 0;
    }
    return 1;
}

static int utf8_terminal_glyphs_roundtrip(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    if(!terminal_allocate_screen(&terminal, 100, 4)) {
        fprintf(stderr, "utf8 glyph screen allocation failed\n");
        return 0;
    }
    terminal_feed(&terminal,
                  "box ──┼── block ▁▂▃▄▅▆▇█ check ✓ greek Λ cjk 測試",
                  80);
    if(!line_equals(&terminal, 0,
                    "box ──┼── block ▁▂▃▄▅▆▇█ check ✓ greek Λ cjk 測試")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int ctrl_c_interrupts_child(void)
{
    TerminalState terminal;
    int i;

    terminal_init(&terminal);
    if(!terminal_spawn(&terminal, NULL, "/bin/sh", "sleep 20", 24, 4)) {
        fprintf(stderr, "spawn failed\n");
        return 0;
    }
    usleep(250000);
    terminal_write_text(&terminal, "\x03");
    for(i = 0; i < 40; i++) {
        terminal_poll(&terminal);
        if(!terminal.running) {
            terminal_close(&terminal);
            return 1;
        }
        usleep(50000);
    }
    terminal_close(&terminal);
    fprintf(stderr, "ctrl-c did not interrupt child\n");
    return 0;
}

static int close_does_not_hang_on_stubborn_child(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    if(!terminal_spawn(&terminal, NULL, "/bin/sh",
                       "trap '' HUP TERM; while :; do sleep 1; done",
                       24, 4)) {
        fprintf(stderr, "stubborn child spawn failed\n");
        return 0;
    }
    usleep(100000);
    alarm(3);
    terminal_close(&terminal);
    alarm(0);
    return 1;
}

static int bash_with_temp_bashrc_outputs(const char *name,
                                         const char *bashrc_text,
                                         const char *command,
                                         const char *marker)
{
    TerminalState terminal;
    char dir[] = "/tmp/kapsule-bashrc-test-XXXXXX";
    char bashrc[sizeof(dir) + 16];
    char old_home_copy[512];
    const char *old_home;
    FILE *fp;
    int ok = 0;
    int i;

    if(access("/bin/bash", X_OK) != 0) {
        fprintf(stderr, "%s needs /bin/bash\n", name);
        return 0;
    }
    if(mkdtemp(dir) == NULL) {
        fprintf(stderr, "%s: mkdtemp failed\n", name);
        return 0;
    }
    snprintf(bashrc, sizeof(bashrc), "%s/.bashrc", dir);
    fp = fopen(bashrc, "w");
    if(fp == NULL) {
        fprintf(stderr, "%s: could not write .bashrc\n", name);
        rmdir(dir);
        return 0;
    }
    fputs(bashrc_text, fp);
    fclose(fp);

    old_home = getenv("HOME");
    snprintf(old_home_copy, sizeof(old_home_copy), "%s",
             old_home != NULL ? old_home : "");
    setenv("HOME", dir, 1);

    terminal_init(&terminal);
    if(!terminal_spawn(&terminal, NULL, "/bin/bash", command, 80, 8)) {
        fprintf(stderr, "%s: spawn failed\n", name);
        if(old_home != NULL)
            setenv("HOME", old_home_copy, 1);
        else
            unsetenv("HOME");
        unlink(bashrc);
        rmdir(dir);
        return 0;
    }
    for(i = 0; i < 80; i++) {
        terminal_poll(&terminal);
        if(line_has(&terminal, marker)) {
            ok = 1;
            break;
        }
        usleep(25000);
    }
    terminal_close(&terminal);

    if(old_home != NULL)
        setenv("HOME", old_home_copy, 1);
    else
        unsetenv("HOME");
    unlink(bashrc);
    rmdir(dir);

    if(!ok)
        fprintf(stderr, "%s: marker did not appear\n", name);
    return ok;
}

static int interactive_bash_sources_bashrc(void)
{
    return bash_with_temp_bashrc_outputs(
        "interactive bashrc test", "echo KAPSULE_BASHRC_MARKER\nexit\n", NULL,
        "KAPSULE_BASHRC_MARKER");
}

static int command_bash_sources_bashrc(void)
{
    return bash_with_temp_bashrc_outputs(
        "command bashrc test",
        "kapsule_marker() { echo KAPSULE_COMMAND_BASHRC_MARKER; }\n",
        "kapsule_marker", "KAPSULE_COMMAND_BASHRC_MARKER");
}

static int finish_capture(const char *name, TerminalState *terminal, int read_fd,
                          const char *expected)
{
    char got[2048];
    int n;

    terminal_close(terminal);
    n = (int)read(read_fd, got, sizeof(got) - 1);
    close(read_fd);
    if(n < 0)
        n = 0;
    got[n] = '\0';
    if(strcmp(got, expected) != 0) {
        fprintf(stderr, "%s: expected", name);
        for(int i = 0; expected[i] != '\0'; i++)
            fprintf(stderr, " %02x", (unsigned char)expected[i]);
        fprintf(stderr, ", got");
        for(int i = 0; i < n; i++)
            fprintf(stderr, " %02x", (unsigned char)got[i]);
        fprintf(stderr, "\n");
        return 0;
    }
    return 1;
}

static int capture_key_sequence(int key, int mods, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!terminal_send_key(&terminal, key, mods))
        return 0;
    return finish_capture("key sequence", &terminal, fds[0], expected);
}

static int capture_function_key_sequence(int function_index, int mods,
                                         const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "function key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!terminal_send_function_key(&terminal, function_index, mods))
        return 0;
    return finish_capture("function key sequence", &terminal, fds[0],
                          expected);
}

static int capture_configured_key_sequence(const char *setup, int key,
                                           int mods, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "configured key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, setup, (int)strlen(setup));
    if(!terminal_send_key(&terminal, key, mods))
        return 0;
    return finish_capture("configured key sequence", &terminal, fds[0],
                          expected);
}

static int capture_codepoint_sequence(unsigned int codepoint, int mods,
                                      const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "codepoint sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!terminal_send_codepoint(&terminal, codepoint, mods))
        return 0;
    return finish_capture("codepoint sequence", &terminal, fds[0], expected);
}

static int capture_configured_codepoint_sequence(const char *setup,
                                                unsigned int codepoint,
                                                int mods,
                                                const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "configured codepoint sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, setup, (int)strlen(setup));
    if(!terminal_send_codepoint(&terminal, codepoint, mods))
        return 0;
    return finish_capture("configured codepoint sequence", &terminal, fds[0],
                          expected);
}

static int capture_control_key_sequence(int key, int mods,
                                        const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "control key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    if(!input_send_control_key(&terminal, key, mods))
        return 0;
    return finish_capture("control key sequence", &terminal, fds[0],
                          expected);
}

static int capture_configured_control_key_sequence(const char *setup, int key,
                                                  int mods,
                                                  const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "configured control key sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, setup, (int)strlen(setup));
    if(!input_send_control_key(&terminal, key, mods))
        return 0;
    return finish_capture("configured control key sequence", &terminal,
                          fds[0], expected);
}

static int capture_application_cursor_sequence(void)
{
    TerminalState terminal;
    int fds[2];
    char got[32];
    int n;

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "application cursor: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.application_cursor_keys = 1;
    if(!terminal_send_key(&terminal, KEY_UP_CODE, 0))
        return 0;
    if(!terminal_send_key(&terminal, KEY_HOME_CODE, 0))
        return 0;
    if(!terminal_send_key(&terminal, KEY_END_CODE, 0))
        return 0;
    terminal_close(&terminal);
    n = (int)read(fds[0], got, sizeof(got) - 1);
    close(fds[0]);
    if(n < 0)
        n = 0;
    got[n] = '\0';
    if(strcmp(got, "\x1bOA\x1bOH\x1bOF") != 0) {
        fprintf(stderr, "application cursor: expected");
        for(int i = 0; "\x1bOA\x1bOH\x1bOF"[i] != '\0'; i++)
            fprintf(stderr, " %02x",
                    (unsigned char)"\x1bOA\x1bOH\x1bOF"[i]);
        fprintf(stderr, ", got");
        for(int i = 0; i < n; i++)
            fprintf(stderr, " %02x", (unsigned char)got[i]);
        fprintf(stderr, "\n");
        return 0;
    }
    return 1;
}

static int capture_keypad_sequence(int app_mode, char key, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "keypad sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.application_keypad = app_mode;
    if(!terminal_send_keypad(&terminal, key))
        return 0;
    return finish_capture("keypad sequence", &terminal, fds[0], expected);
}

static int capture_bracketed_paste(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "bracketed paste: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.bracketed_paste = 1;
    if(!TerminalPaneClipboardPerform(terminal_clipboard(&terminal),
                                     TERMINAL_PANE_CLIPBOARD_PASTE_TEXT,
                                     "paste\ntext"))
        return 0;
    return finish_capture("bracketed paste", &terminal, fds[0],
                          "\x1b[200~paste\ntext\x1b[201~");
}

static int capture_sanitized_bracketed_paste(void)
{
    TerminalState terminal;
    int fds[2];
    const char payload[] = "safe\x1b[201~after\x1b[31mred\xd0\x80\a"
                           "\x9b" "32mgreen"
                           "osc\x1b]2;title\aafterosc"
                           "dcs\x1bPq~~\x1b\\afterdcs"
                           "c1osc\x9d" "2;bad\aafterc1osc"
                           "c1dcs\x90q~\x9c" "afterc1dcs";

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "sanitized bracketed paste: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.bracketed_paste = 1;
    if(!TerminalPaneClipboardPerform(terminal_clipboard(&terminal),
                                     TERMINAL_PANE_CLIPBOARD_PASTE_TEXT,
                                     payload))
        return 0;
    return finish_capture("sanitized bracketed paste", &terminal, fds[0],
                          "\x1b[200~safeafterred\xd0\x80green"
                          "oscafteroscdcsafterdcs"
                          "c1oscafterc1oscc1dcsafterc1dcs\x1b[201~");
}

static int capture_clipboard_setter_query(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "clipboard setter query: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    RequestUIClipboardBufferWrite(&terminal.clipboard, "pending");
    SetUIClipboardBufferText(&terminal.clipboard, "shared");
    if(UIClipboardBufferHasPendingWrite(&terminal.clipboard)) {
        fprintf(stderr, "clipboard setter left pending flag set\n");
        terminal_close(&terminal);
        close(fds[0]);
        return 0;
    }
    terminal_feed(&terminal, "\x1b]52;c;?\a",
                  (int)strlen("\x1b]52;c;?\a"));
    return finish_capture("clipboard setter query", &terminal, fds[0],
                          "\x1b]52;c;c2hhcmVk\a");
}

static int capture_mouse_sequence(int sgr, int button, int pressed, int motion,
                                  int mods, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "mouse sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.mouse_mode = motion ? 1002 : 1000;
    terminal.mouse_sgr = sgr;
    if(!terminal_send_mouse(&terminal, button, 4, 2, pressed, motion, mods))
        return 0;
    return finish_capture("mouse sequence", &terminal, fds[0], expected);
}

static int capture_mouse_mode_sequence(const char *mode_sequence, int button,
                                       int pressed, int motion, int mods,
                                       const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "mouse mode sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, mode_sequence, (int)strlen(mode_sequence));
    if(!terminal_send_mouse(&terminal, button, 4, 2, pressed, motion, mods))
        return 0;
    return finish_capture("mouse mode sequence", &terminal, fds[0], expected);
}

static int capture_no_mouse_mode_sequence(const char *name,
                                          const char *mode_sequence,
                                          int button, int pressed, int motion,
                                          int mods)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "%s: pipe failed\n", name);
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, mode_sequence, (int)strlen(mode_sequence));
    if(terminal_send_mouse(&terminal, button, 4, 2, pressed, motion, mods)) {
        fprintf(stderr, "%s: unexpected mouse report\n", name);
        terminal_close(&terminal);
        close(fds[0]);
        return 0;
    }
    return finish_capture(name, &terminal, fds[0], "");
}

static int capture_utf8_mouse_sequence(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "utf8 mouse sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, "\x1b[?1000h\x1b[?1005h",
                  (int)strlen("\x1b[?1000h\x1b[?1005h"));
    if(!terminal_send_mouse(&terminal, TERMINAL_MOUSE_LEFT, 300, 2, 1, 0, 0))
        return 0;
    return finish_capture("utf8 mouse sequence", &terminal, fds[0],
                          "\x1b[M \xc5\x8d#");
}

static int capture_pixel_mouse_sequence(void)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "pixel mouse sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, "\x1b[?1000h\x1b[?1016h",
                  (int)strlen("\x1b[?1000h\x1b[?1016h"));
    if(!terminal_send_mouse_pixels(&terminal, TERMINAL_MOUSE_LEFT, 4, 2, 80,
                                   40, 1, 0, 0))
        return 0;
    return finish_capture("pixel mouse sequence", &terminal, fds[0],
                          "\x1b[<0;81;41M");
}

static int capture_alternate_scroll_sequence(const char *setup, int direction,
                                             const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "alternate scroll sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_feed(&terminal, setup, (int)strlen(setup));
    if(!terminal_send_alternate_scroll(&terminal, direction, 0))
        return 0;
    return finish_capture("alternate scroll sequence", &terminal, fds[0],
                          expected);
}

static int capture_focus_sequence(int focused, const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "focus sequence: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.focus_reporting = 1;
    if(!terminal_send_focus(&terminal, focused))
        return 0;
    return finish_capture("focus sequence", &terminal, fds[0], expected);
}

static int capture_osc_color_query(void)
{
    TerminalState terminal;
    int fds[2];
    const char *input = "\x1b]10;#112233\a\x1b]10;?\a"
                        "\x1b]4;2;#445566\a\x1b]4;2;?\a"
                        "\x1b]104;2abc\a\x1b]4;2;?\a"
                        "\x1b]4;x;#000000;1abc;#000000\a"
                        "\x1b]4;1;?\a"
                        "\x1b]104;2\a\x1b]4;2;?\a"
                        "\x1b]110\a\x1b]10;?\a"
                        "\x1b]111\a\x1b]11;?\a"
                        "\x1b]112\a\x1b]12;?\a"
                        "\x1b]13;#778899\a\x1b]13;?\a"
                        "\x1b]14;#a0b0c0\a\x1b]14;?\a"
                        "\x1b]113\a\x1b]13;?\a"
                        "\x1b]114\a\x1b]14;?\a"
                        "\x1b]17;#224466\a\x1b]17;?\a"
                        "\x1b]19;#ddeeff\a\x1b]19;?\a"
                        "\x1b]117\a\x1b]17;?\a"
                        "\x1b]119\a\x1b]19;?\a";

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "osc color query: pipe failed\n");
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal.base_fg = COLOR_TRUE_RGB | 0xaabbcc;
    terminal.base_bg = COLOR_TRUE_RGB | 0x010203;
    terminal.base_cursor_color = COLOR_TRUE_RGB | 0x334455;
    terminal.base_selection_bg = COLOR_TRUE_RGB | 0x556677;
    terminal.base_selection_fg = COLOR_TRUE_RGB | 0xeeccaa;
    terminal_feed(&terminal, input, (int)strlen(input));
    return finish_capture("osc color query", &terminal, fds[0],
                          "\x1b]10;rgb:1111/2222/3333\a"
                          "\x1b]4;2;rgb:4444/5555/6666\a"
                          "\x1b]4;2;rgb:4444/5555/6666\a"
                          "\x1b]4;1;rgb:cdcd/3131/3131\a"
                          "\x1b]4;2;rgb:0d0d/bcbc/7979\a"
                          "\x1b]10;rgb:aaaa/bbbb/cccc\a"
                          "\x1b]11;rgb:0101/0202/0303\a"
                          "\x1b]12;rgb:3333/4444/5555\a"
                          "\x1b]13;rgb:7777/8888/9999\a"
                          "\x1b]14;rgb:a0a0/b0b0/c0c0\a"
                          "\x1b]13;rgb:aaaa/bbbb/cccc\a"
                          "\x1b]14;rgb:0101/0202/0303\a"
                          "\x1b]17;rgb:2222/4444/6666\a"
                          "\x1b]19;rgb:dddd/eeee/ffff\a"
                          "\x1b]17;rgb:5555/6666/7777\a"
                          "\x1b]19;rgb:eeee/cccc/aaaa\a");
}

static int c1_controls_parse_csi_and_osc(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x9b" "31mR\x9b" "0mN",
                  (int)strlen("\x9b" "31mR\x9b" "0mN"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 'R' || cell->fg != 1) {
        fprintf(stderr, "8-bit csi sgr failed\n");
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 1, 0);
    if(cell == NULL || cell->codepoint != 'N' ||
       cell->fg != COLOR_DEFAULT) {
        fprintf(stderr, "8-bit csi reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x9d" "2;C1 Title\a",
                  (int)strlen("\x9d" "2;C1 Title\a"));
    if(strcmp(terminal.title, "C1 Title") != 0) {
        fprintf(stderr, "8-bit osc bel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x9d" "10;#112233\x9c",
                  (int)strlen("\x9d" "10;#112233\x9c"));
    if(terminal.default_fg != (COLOR_TRUE_RGB | 0x112233)) {
        fprintf(stderr, "8-bit osc st failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int control_strings_are_ignored(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, "A\x1b_hidden\x1b\\B",
                  (int)strlen("A\x1b_hidden\x1b\\B"));
    if(!line_equals(&terminal, 0, "AB")) {
        fprintf(stderr, "apc ignore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\x1b[2K\x1b^hidden\x1b\\C",
                  (int)strlen("\r\x1b[2K\x1b^hidden\x1b\\C"));
    if(!line_equals(&terminal, 0, "C")) {
        fprintf(stderr, "pm ignore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\x1b[2K\x1bXhidden\x1b\\D",
                  (int)strlen("\r\x1b[2K\x1bXhidden\x1b\\D"));
    if(!line_equals(&terminal, 0, "D")) {
        fprintf(stderr, "sos ignore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\x1b[2K\x9fhidden\x9c" "E",
                  (int)strlen("\r\x1b[2K\x9fhidden\x9c" "E"));
    if(!line_equals(&terminal, 0, "E")) {
        fprintf(stderr, "8-bit apc ignore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int bell_is_reported_without_text_output(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 16, 4);
    terminal_feed(&terminal, "A\aB", (int)strlen("A\aB"));
    if(!line_equals(&terminal, 0, "AB")) {
        fprintf(stderr, "bell text output failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!terminal_consume_bell(&terminal)) {
        fprintf(stderr, "bell pending failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(terminal_consume_bell(&terminal)) {
        fprintf(stderr, "bell consume failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int parser_cancel_controls_abort_active_sequence(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, "\x1b[31\x18X",
                  (int)strlen("\x1b[31\x18X"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 'X' ||
       cell->fg != COLOR_DEFAULT) {
        fprintf(stderr, "csi cancel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 4);
    snprintf(terminal.title, sizeof(terminal.title), "old");
    terminal_feed(&terminal, "\x1b]2;bad\x1aX",
                  (int)strlen("\x1b]2;bad\x1aX"));
    if(strcmp(terminal.title, "old") != 0 || !line_equals(&terminal, 0, "X")) {
        fprintf(stderr, "osc cancel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, "\x1bPq~\x18X",
                  (int)strlen("\x1bPq~\x18X"));
    if(terminal_sixel_count(&terminal) != 0 ||
       !line_equals(&terminal, 0, "X")) {
        fprintf(stderr, "dcs cancel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, "\x1b_hidden\x18X",
                  (int)strlen("\x1b_hidden\x18X"));
    if(!line_equals(&terminal, 0, "X")) {
        fprintf(stderr, "ignore string cancel failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int capture_response_sequence(const char *name, const char *input,
                                     const char *expected)
{
    TerminalState terminal;
    int fds[2];

    terminal_init(&terminal);
    if(pipe(fds) != 0) {
        fprintf(stderr, "%s: pipe failed\n", name);
        return 0;
    }
    terminal.fd = fds[1];
    terminal.running = 1;
    terminal_resize(&terminal, 24, 4);
    terminal_feed(&terminal, input, (int)strlen(input));
    return finish_capture(name, &terminal, fds[0], expected);
}

static int resize_reflows_wrapped_text(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, "abcdefghijklmnopq", 17);
    terminal_resize(&terminal, 12, 5);
    if(!line_equals(&terminal, 0, "abcdefghijkl")) {
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 1, "mnopq")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int visible_line_wrapped_reports_soft_wraps(void)
{
    TerminalState terminal;
    const char *scroll_text = "\r\n123456789\r\nzzzz";

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 3);
    terminal_feed(&terminal, "abcdefghi", 9);
    if(!terminal_visible_line_wrapped(&terminal, 0) ||
       terminal_visible_line_wrapped(&terminal, 1)) {
        fprintf(stderr, "screen wrapped-line report failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, scroll_text, (int)strlen(scroll_text));
    if(!terminal_visible_line_wrapped(&terminal, 0) ||
       terminal_visible_line_wrapped(&terminal, 1)) {
        fprintf(stderr, "scrollback wrapped-line report failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int resize_preserves_cursor_on_sparse_screen(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 100, 30);
    terminal_feed(&terminal, "$ ", 2);
    terminal_resize(&terminal, 80, 24);
    if(terminal.cursor_row != 0 || terminal.cursor_col != 2 ||
       !line_equals(&terminal, 0, "$")) {
        fprintf(stderr, "sparse resize moved cursor to row %d col %d\n",
                terminal.cursor_row, terminal.cursor_col);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int resize_reflows_hidden_main_screen_in_alternate_mode(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "abcdefghijklmnopq",
                  (int)strlen("abcdefghijklmnopq"));
    terminal_feed(&terminal, "\x1b[?1049hALT",
                  (int)strlen("\x1b[?1049hALT"));
    if(!terminal.alternate_screen || !line_equals(&terminal, 0, "ALT")) {
        fprintf(stderr, "alternate resize setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[?1049l", (int)strlen("\x1b[?1049l"));
    if(terminal.alternate_screen ||
       !line_equals(&terminal, 0, "abcdefghijkl") ||
       !line_equals(&terminal, 1, "mnopq")) {
        fprintf(stderr, "hidden main screen did not reflow after alt resize\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int dec_autowrap_mode_controls_right_margin(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "abcdefghI", 9);
    if(!line_equals(&terminal, 0, "abcdefgh") ||
       !line_equals(&terminal, 1, "I")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "\x1b[?7labcdefghI", 14);
    if(terminal.autowrap || !line_equals(&terminal, 0, "abcdefgI") ||
       !line_equals(&terminal, 1, "")) {
        fprintf(stderr, "autowrap disable failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?7h", 5);
    if(!terminal.autowrap) {
        fprintf(stderr, "autowrap re-enable failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\nabcdefghF", 11);
    if(!line_equals(&terminal, 1, "abcdefgh") ||
       !line_equals(&terminal, 2, "F")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b" "c", 2);
    if(!terminal.autowrap) {
        fprintf(stderr, "autowrap reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int resize_reflows_scrollback(void)
{
    TerminalState terminal;
    const char *text = "abcdefghijkl\r\none\r\ntwo\r\nthree\r\nfour\r\nfive\r\n";
    char line[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 6, 3);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal_scrollback_rows(&terminal) < 3) {
        fprintf(stderr, "scrollback setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_resize(&terminal, 12, 3);
    terminal_scrollback_line(&terminal, 0, line, sizeof(line));
    if(strcmp(line, "abcdefghijkl") != 0) {
        fprintf(stderr, "reflowed scrollback: expected 'abcdefghijkl', got '%s'\n",
                line);
        terminal_close(&terminal);
        return 0;
    }
    terminal_scrollback_line(&terminal, 1, line, sizeof(line));
    if(strcmp(line, "one") != 0) {
        fprintf(stderr, "reflowed scrollback: expected 'one', got '%s'\n",
                line);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int resize_reflows_wrapped_scrollback_boundary(void)
{
    TerminalState terminal;
    char line[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, "abcdefghijklmnopqrstuvwxyz\r\nnext",
                  (int)strlen("abcdefghijklmnopqrstuvwxyz\r\nnext"));
    if(terminal_scrollback_rows(&terminal) != 1 ||
       !terminal_visible_line_wrapped(&terminal, 0)) {
        fprintf(stderr,
                "wrapped scrollback boundary setup failed: rows=%d wrap0=%d\n",
                terminal_scrollback_rows(&terminal),
                terminal_visible_line_wrapped(&terminal, 0));
        terminal_close(&terminal);
        return 0;
    }
    terminal_resize(&terminal, 12, 4);
    if(terminal_scrollback_rows(&terminal) != 0) {
        terminal_scrollback_line(&terminal, 0, line, sizeof(line));
        fprintf(stderr, "boundary reflow kept unexpected scrollback '%s'\n",
                line);
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 0, "abcdefghijkl") ||
       !line_equals(&terminal, 1, "mnopqrstuvwx") ||
       !line_equals(&terminal, 2, "yz") || !line_equals(&terminal, 3, "next")) {
        fprintf(stderr, "boundary reflow main screen failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int origin_mode_uses_scroll_region(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 5);
    terminal_feed(&terminal, "\x1b[2;4r\x1b[?6h\x1b[Htop",
                  (int)strlen("\x1b[2;4r\x1b[?6h\x1b[Htop"));
    if(!line_equals(&terminal, 1, "top")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[99;1Hbottom",
                  (int)strlen("\x1b[99;1Hbottom"));
    if(!line_equals(&terminal, 3, "bottom")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?6l\x1b[Hhome",
                  (int)strlen("\x1b[?6l\x1b[Hhome"));
    if(!line_equals(&terminal, 0, "home")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int erase_saved_lines_clears_scrollback_only(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 3);
    terminal_feed(&terminal, "one\r\ntwo\r\nthree\r\nfour\r\nfive",
                  (int)strlen("one\r\ntwo\r\nthree\r\nfour\r\nfive"));
    if(terminal_scrollback_rows(&terminal) <= 0 || !line_has(&terminal, "five")) {
        fprintf(stderr, "erase saved lines setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[3J", 4);
    if(terminal_scrollback_rows(&terminal) != 0 || !line_has(&terminal, "five")) {
        fprintf(stderr, "erase saved lines failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int insert_mode_inserts_printable_text(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "abcd\r\x1b[3G\x1b[4hXY\x1b[4lZ",
                  (int)strlen("abcd\r\x1b[3G\x1b[4hXY\x1b[4lZ"));
    if(!line_equals(&terminal, 0, "abXYZd")) {
        terminal_close(&terminal);
        return 0;
    }
    if(terminal.insert_mode) {
        fprintf(stderr, "insert mode reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int csi_repeat_and_cursor_aliases_work(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[31mA\x1b[3b",
                  (int)strlen("\x1b[31mA\x1b[3b"));
    if(!line_equals(&terminal, 0, "AAAA")) {
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 3, 0);
    if(cell == NULL || cell->fg != 1) {
        fprintf(stderr, "csi repeat style failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[0m\r\x1b[2K\x1b[4`X",
                  (int)strlen("\x1b[0m\r\x1b[2K\x1b[4`X"));
    if(!line_equals(&terminal, 0, "   X")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[1;1H\x1b[3aY\x1b[2eZ",
                  (int)strlen("\x1b[1;1H\x1b[3aY\x1b[2eZ"));
    if(!line_equals(&terminal, 0, "   Y") ||
       !line_equals(&terminal, 2, "    Z")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int dec_screen_alignment_fills_visible_grid(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 3);
    terminal_feed(&terminal, "\x1b[31mabc\r\nxyz\x1b#8",
                  (int)strlen("\x1b[31mabc\r\nxyz\x1b#8"));
    if(!line_equals(&terminal, 0, "EEEEEEEE") ||
       !line_equals(&terminal, 1, "EEEEEEEE") ||
       !line_equals(&terminal, 2, "EEEEEEEE")) {
        terminal_close(&terminal);
        return 0;
    }
    if(terminal.cursor_row != 0 || terminal.cursor_col != 0) {
        fprintf(stderr, "screen alignment cursor reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int escape_next_line_moves_to_column_zero(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 3);
    terminal_feed(&terminal, "abc\x1b" "EX",
                  (int)strlen("abc\x1b" "EX"));
    if(!line_equals(&terminal, 0, "abc") ||
       !line_equals(&terminal, 1, "X")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int c1_single_byte_controls_work(void)
{
    TerminalState terminal;
    const unsigned char ind[] = {'a', 0x84, 'b'};
    const unsigned char nel[] = {'a', 0x85, 'b'};
    const unsigned char hts[] = {'a', 'b', 0x88, '\r', '\t', 'c'};
    const unsigned char ri[] = {'\x1b', '[', '2', ';', '1', 'H', 0x8d, 'x'};

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, ind, (int)sizeof(ind));
    if(!line_equals(&terminal, 0, "a") ||
       !line_equals(&terminal, 1, " b")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, nel, (int)sizeof(nel));
    if(!line_equals(&terminal, 0, "a") ||
       !line_equals(&terminal, 1, "b")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, hts, (int)sizeof(hts));
    if(!line_equals(&terminal, 0, "abc")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 4);
    terminal_feed(&terminal, ri, (int)sizeof(ri));
    if(!line_equals(&terminal, 0, "x")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int tab_clear_controls_work(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[3g\tA", (int)strlen("\x1b[3g\tA"));
    if(!line_equals(&terminal, 0, "           A")) {
        fprintf(stderr, "clear all tab stops failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\x1b[2K  \x1b" "H\r\tB",
                  (int)strlen("\r\x1b[2K  \x1b" "H\r\tB"));
    if(!line_equals(&terminal, 0, "  B")) {
        fprintf(stderr, "set tab stop failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\r\x1b[2K  \x1b[g\r\tC",
                  (int)strlen("\r\x1b[2K  \x1b[g\r\tC"));
    if(!line_equals(&terminal, 0, "           C")) {
        fprintf(stderr, "clear current tab stop failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int wide_characters_use_two_columns(void)
{
    TerminalState terminal;
    const Cell *cell;
    const char *text = "ab\xe4\xbd\xa0""cd";

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal.cursor_col != 6) {
        fprintf(stderr, "wide cursor width failed: %d\n", terminal.cursor_col);
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 3, 0);
    if(cell == NULL || (cell->style & STYLE_WIDE_CONT) == 0) {
        fprintf(stderr, "wide continuation failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 0, "ab\xe4\xbd\xa0""cd")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, "1234567\xe4\xbd\xa0", 10);
    if(!line_equals(&terminal, 0, "1234567") ||
       !line_equals(&terminal, 1, "\xe4\xbd\xa0")) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int combining_marks_stay_on_base_cell(void)
{
    TerminalState terminal;
    const Cell *cell;
    const char *text = "e\xcc\x81x";

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 5);
    terminal_feed(&terminal, text, (int)strlen(text));
    if(terminal.cursor_col != 2) {
        fprintf(stderr, "combining cursor width failed: %d\n",
                terminal.cursor_col);
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 'e' || cell->combining != 0x0301) {
        fprintf(stderr, "combining mark attach failed\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!line_equals(&terminal, 0, text)) {
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int dec_special_graphics_charset_maps_line_drawing(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b(0lqkx\x1b(Bx",
                  (int)strlen("\x1b(0lqkx\x1b(Bx"));
    if(!line_equals(&terminal, 0, "┌─┐│x")) {
        fprintf(stderr, "dec special graphics g0 failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b)0\x0e""mqj\x0f""mqj",
                  (int)strlen("\x1b)0\x0e""mqj\x0f""mqj"));
    if(!line_equals(&terminal, 0, "└─┘mqj")) {
        fprintf(stderr, "dec special graphics g1 failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b(0l\x1b" "cl",
                  (int)strlen("\x1b(0l\x1b" "cl"));
    if(!line_equals(&terminal, 0, "l")) {
        fprintf(stderr, "dec special graphics reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int save_restore_preserves_rendition_and_charset(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal,
                  "\x1b[31m\x1b(0\x1b" "7\x1b[32m\x1b(BX\x1b" "8l",
                  (int)strlen(
                      "\x1b[31m\x1b(0\x1b" "7\x1b[32m\x1b(BX\x1b" "8l"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x250c || cell->fg != 1) {
        fprintf(stderr, "escape save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[34m\x1b(0\x1b[s\x1b[35m\x1b(BX\x1b[ul",
                  (int)strlen(
                      "\x1b[34m\x1b(0\x1b[s\x1b[35m\x1b(BX\x1b[ul"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x250c || cell->fg != 4) {
        fprintf(stderr, "csi save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[33m\x1b(0\x1b[?1048h\x1b[35m\x1b(B"
                             "\x1b[?1048lq",
                  (int)strlen("\x1b[33m\x1b(0\x1b[?1048h\x1b[35m\x1b(B"
                              "\x1b[?1048lq"));
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->codepoint != 0x2500 || cell->fg != 3) {
        fprintf(stderr, "1048 save/restore rendition failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int osc_current_directory_updates_session_title(void)
{
    Session session;
    char cwd[1024];

    session_init(&session);
    snprintf(session.cwd, sizeof(session.cwd), "/home/wao");
    session_sync_terminal_metadata(&session);
    terminal_feed(&session.terminal,
                  "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a",
                  (int)strlen(
                      "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a"));
    session_sync_terminal_metadata(&session);
    session_current_cwd(&session, cwd, sizeof(cwd));
    if(strcmp(cwd, "/home/wao/Projects/Kapsule Test") != 0 ||
       strcmp(session_title(&session), "Kapsule Test") != 0) {
        fprintf(stderr, "osc current directory sync failed\n");
        session_close(&session);
        return 0;
    }
    terminal_feed(&session.terminal, "\x1b]2;Editor\a",
                  (int)strlen("\x1b]2;Editor\a"));
    session_sync_terminal_metadata(&session);
    if(strcmp(session_title(&session), "Editor") != 0) {
        fprintf(stderr, "osc title sync failed\n");
        session_close(&session);
        return 0;
    }
    terminal_feed(&session.terminal,
                  "\x1b]2;wao@omega:/mnt/storage/Projects/kapsule\a",
                  (int)strlen(
                      "\x1b]2;wao@omega:/mnt/storage/Projects/kapsule\a"));
    session_sync_terminal_metadata(&session);
    if(strcmp(session_title(&session), "kapsule") != 0) {
        fprintf(stderr, "path-like osc title was not shortened\n");
        session_close(&session);
        return 0;
    }
    session_set_title(&session, "manual");
    terminal_feed(&session.terminal,
                  "\x1b]2;Ignored\a\x1b]7;file://omega/tmp/project\a",
                  (int)strlen(
                      "\x1b]2;Ignored\a\x1b]7;file://omega/tmp/project\a"));
    session_sync_terminal_metadata(&session);
    if(strcmp(session_title(&session), "manual") != 0) {
        fprintf(stderr, "manual tab title override failed\n");
        session_close(&session);
        return 0;
    }
    session_restore_title(&session, "/tmp/full/path", 0);
    if(strcmp(session_title(&session), "path") != 0) {
        fprintf(stderr, "auto restored title was not shortened\n");
        session_close(&session);
        return 0;
    }
    session_restore_title(&session, "/tmp/full/path", 1);
    if(strcmp(session_title(&session), "/tmp/full/path") != 0) {
        fprintf(stderr, "manual restored title was not preserved\n");
        session_close(&session);
        return 0;
    }
    session_close(&session);
    return 1;
}

static int alternate_screen_modes_preserve_expected_state(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[2;5H\x1b[?1048h\x1b[4;7H\x1b[?1048lX",
                  (int)strlen("\x1b[2;5H\x1b[?1048h\x1b[4;7H\x1b[?1048lX"));
    if(terminal.alternate_screen || terminal.cursor_row != 1 ||
       terminal.cursor_col != 5 || !line_equals(&terminal, 1, "    X")) {
        fprintf(stderr, "1048 cursor save/restore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[?47hALT\x1b[?47lMAIN\x1b[?47h",
                  (int)strlen("\x1b[?47hALT\x1b[?47lMAIN\x1b[?47h"));
    if(!terminal.alternate_screen || !line_has(&terminal, "ALT")) {
        fprintf(stderr, "47 alternate buffer preservation failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?47l", 6);
    if(terminal.alternate_screen || !line_has(&terminal, "MAIN")) {
        fprintf(stderr, "47 return to main failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[?1047hOLD\x1b[?1047lMAIN\x1b[?1047h",
                  (int)strlen("\x1b[?1047hOLD\x1b[?1047lMAIN"
                              "\x1b[?1047h"));
    if(!terminal.alternate_screen || line_has(&terminal, "OLD")) {
        fprintf(stderr, "1047 alternate screen was not cleared on entry\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "NEW\x1b[?1047l", 11);
    if(terminal.alternate_screen || !line_has(&terminal, "MAIN")) {
        fprintf(stderr, "1047 return to main failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?1047h", 8);
    if(!terminal.alternate_screen || line_has(&terminal, "NEW")) {
        fprintf(stderr, "1047 alternate screen was not cleared on exit\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "MAIN\x1b[2;6H\x1b[?1049hALT\x1b[?1049lZ",
                  (int)strlen("MAIN\x1b[2;6H\x1b[?1049hALT\x1b[?1049lZ"));
    if(terminal.alternate_screen || terminal.cursor_row != 1 ||
       terminal.cursor_col != 6 || !line_has(&terminal, "MAIN") ||
       !line_equals(&terminal, 1, "     Z")) {
        fprintf(stderr, "1049 alternate screen restore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?1049h", 8);
    if(!terminal.alternate_screen || line_has(&terminal, "ALT")) {
        fprintf(stderr, "1049 alternate clear failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 3);
    terminal_feed(&terminal, "one\ntwo\nthree\nfour",
                  (int)strlen("one\ntwo\nthree\nfour"));
    if(terminal_scrollback_rows(&terminal) <= 0) {
        fprintf(stderr, "alternate scrollback setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?1049h\x1b[Halt",
                  (int)strlen("\x1b[?1049h\x1b[Halt"));
    if(terminal_visible_line_count(&terminal) != terminal.rows ||
       !line_equals(&terminal, 0, "alt")) {
        fprintf(stderr, "alternate visible rows leaked scrollback\n");
        terminal_close(&terminal);
        return 0;
    }
    {
        char line[64];

        terminal_visible_line(&terminal, 0, line, sizeof(line));
        if(strcmp(line, "alt") != 0) {
            fprintf(stderr, "alternate visible line failed\n");
            terminal_close(&terminal);
            return 0;
        }
    }
    terminal_feed(&terminal, "\x1b[?1049l", 8);
    if(terminal_visible_line_count(&terminal) <= terminal.rows) {
        fprintf(stderr, "alternate scrollback restore failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int newline_mode_controls_linefeed(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "ab\nc", (int)strlen("ab\nc"));
    if(!line_equals(&terminal, 1, "  c")) {
        fprintf(stderr, "normal linefeed should preserve column\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[20hab\nc", (int)strlen("\x1b[20hab\nc"));
    if(!terminal.newline_mode || !line_equals(&terminal, 1, "c")) {
        fprintf(stderr, "newline mode did not return linefeed to column zero\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\rxy\vz", (int)strlen("\rxy\vz"));
    if(!line_equals(&terminal, 2, "z")) {
        fprintf(stderr, "newline mode did not apply to vertical tab\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\rwq\fp", (int)strlen("\rwq\fp"));
    if(!line_equals(&terminal, 3, "p")) {
        fprintf(stderr, "newline mode did not apply to form feed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[20hab\x1b[20l\nc",
                  (int)strlen("\x1b[20hab\x1b[20l\nc"));
    if(terminal.newline_mode || !line_equals(&terminal, 1, "  c")) {
        fprintf(stderr, "normal linefeed mode was not restored\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int decstr_soft_reset_preserves_screen_and_resets_modes(void)
{
    TerminalState terminal;
    const Cell *cell;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal,
                  "abc\x1b[2;3r\x1b[2;2H\x1b[31m\x1b(0"
                  "\x1b[?1h\x1b[?6h\x1b[?7l\x1b[?12l\x1b[4h"
                  "\x1b[20h"
                  "\x1b[?1000h\x1b[?1005h\x1b[?1006h\x1b[?1015h"
                  "\x1b[?1016h\x1b[?1007h"
                  "\x1b[?2004h\x1b[!pX",
                  (int)strlen("abc\x1b[2;3r\x1b[2;2H\x1b[31m\x1b(0"
                              "\x1b[?1h\x1b[?6h\x1b[?7l\x1b[?12l\x1b[4h"
                              "\x1b[20h"
                              "\x1b[?1000h\x1b[?1005h\x1b[?1006h"
                              "\x1b[?1015h\x1b[?1016h\x1b[?1007h"
                              "\x1b[?2004h\x1b[!pX"));
    if(!line_equals(&terminal, 0, "Xbc")) {
        fprintf(stderr, "decstr did not preserve screen at home\n");
        terminal_close(&terminal);
        return 0;
    }
    cell = terminal_cell(&terminal, 0, 0);
    if(cell == NULL || cell->fg != COLOR_DEFAULT || cell->style != 0 ||
       terminal.cursor_row != 0 || terminal.cursor_col != 1 ||
       terminal.scroll_top != 0 || terminal.scroll_bottom != 3 ||
       !terminal.cursor_visible || !terminal.cursor_blink ||
       terminal.origin_mode ||
       !terminal.autowrap || terminal.application_cursor_keys ||
       terminal.application_keypad || terminal.bracketed_paste ||
       terminal.modify_other_keys || terminal.insert_mode ||
       terminal.newline_mode ||
       terminal.mouse_mode || terminal.mouse_utf8 || terminal.mouse_sgr ||
       terminal.mouse_urxvt || terminal.mouse_pixels ||
       terminal.alternate_scroll) {
        fprintf(stderr, "decstr soft reset state failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "q", 1);
    if(!line_equals(&terminal, 0, "Xqc")) {
        fprintf(stderr, "decstr charset reset failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int ris_hard_reset_returns_to_clean_main_screen(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "MAIN\x1b[?1049hALT\x1b" "cZ",
                  (int)strlen("MAIN\x1b[?1049hALT\x1b" "cZ"));
    if(terminal.alternate_screen || !line_equals(&terminal, 0, "Z")) {
        fprintf(stderr, "RIS did not return to a clean main screen\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?47h", (int)strlen("\x1b[?47h"));
    if(!terminal.alternate_screen || line_has(&terminal, "ALT")) {
        fprintf(stderr, "RIS did not clear alternate screen content\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int modifier_key_disable_sequence_disables_modify_other_keys(void)
{
    TerminalState terminal;

    terminal_init(&terminal);
    terminal_feed(&terminal, "\x1b[>4;2m\x1b[>4n",
                  (int)strlen("\x1b[>4;2m\x1b[>4n"));
    if(terminal.modify_other_keys != -1) {
        fprintf(stderr, "modifyOtherKeys disable sequence failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[>4;1m", (int)strlen("\x1b[>4;1m"));
    if(terminal.modify_other_keys != 1) {
        fprintf(stderr, "modifyOtherKeys re-enable after disable failed\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int visible_search_finds_scrollback_with_smartcase(void)
{
    TerminalState terminal;
    TerminalSearchMatch match;
    char line[256];
    int total;

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 3);
    terminal_feed(&terminal,
                  "alpha\r\nBeta Needle\r\ngamma\r\ndelta needle\r\nomega",
                  (int)strlen(
                      "alpha\r\nBeta Needle\r\ngamma\r\ndelta needle\r\nomega"));
    total = terminal_visible_line_count(&terminal);
    if(total <= terminal.rows) {
        fprintf(stderr, "visible search setup did not create scrollback\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!terminal_find_visible(&terminal, "needle", total - 1, 4096, -1, 0,
                              &match)) {
        fprintf(stderr, "visible search did not find lowercase query\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_visible_line(&terminal, match.row, line, sizeof(line));
    if(strcmp(line, "delta needle") != 0 || match.col != 6 ||
       match.length != 6) {
        fprintf(stderr, "visible search lowercase: row '%s' col %d len %d\n",
                line, match.col, match.length);
        terminal_close(&terminal);
        return 0;
    }
    if(!terminal_find_visible(&terminal, "Needle", total - 1, 4096, -1, 0,
                              &match)) {
        fprintf(stderr, "visible search did not find uppercase query\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_visible_line(&terminal, match.row, line, sizeof(line));
    if(strcmp(line, "Beta Needle") != 0 || match.col != 5) {
        fprintf(stderr, "visible search smartcase: row '%s' col %d\n",
                line, match.col);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int visible_search_wraps_and_respects_direction(void)
{
    TerminalState terminal;
    TerminalSearchMatch match;
    char line[256];
    int total;

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 3);
    terminal_feed(&terminal, "alpha one\r\nbeta two\r\ngamma three\r\nomega",
                  (int)strlen("alpha one\r\nbeta two\r\ngamma three\r\nomega"));
    total = terminal_visible_line_count(&terminal);
    if(!terminal_find_visible(&terminal, "alpha", total - 1, 4096, 1, 1,
                              &match)) {
        fprintf(stderr, "visible search did not wrap forward\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_visible_line(&terminal, match.row, line, sizeof(line));
    if(strcmp(line, "alpha one") != 0 || match.col != 0) {
        fprintf(stderr, "visible search wrap forward: row '%s' col %d\n",
                line, match.col);
        terminal_close(&terminal);
        return 0;
    }
    if(!terminal_find_visible(&terminal, "omega", 0, 0, -1, 1, &match)) {
        fprintf(stderr, "visible search did not wrap backward\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_visible_line(&terminal, match.row, line, sizeof(line));
    if(strcmp(line, "omega") != 0 || match.col != 0) {
        fprintf(stderr, "visible search wrap backward: row '%s' col %d\n",
                line, match.col);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int visible_search_uses_alternate_screen_only(void)
{
    TerminalState terminal;
    TerminalSearchMatch match;
    char line[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 24, 3);
    terminal_feed(&terminal,
                  "main needle\r\nscroll needle\r\nthird\r\nfourth\r\nlast\r\n",
                  (int)strlen(
                      "main needle\r\nscroll needle\r\nthird\r\nfourth\r\nlast\r\n"));
    if(terminal_scrollback_rows(&terminal) <= 0) {
        fprintf(stderr, "alternate search setup did not create scrollback\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_feed(&terminal, "\x1b[?1049hALT needle",
                  (int)strlen("\x1b[?1049hALT needle"));
    if(terminal_visible_line_count(&terminal) != terminal.rows) {
        fprintf(stderr, "alternate search visible count includes scrollback\n");
        terminal_close(&terminal);
        return 0;
    }
    if(terminal_find_visible(&terminal, "main", 0, 0, 1, 1, &match)) {
        fprintf(stderr, "alternate search leaked main screen scrollback\n");
        terminal_close(&terminal);
        return 0;
    }
    if(!terminal_find_visible(&terminal, "ALT", 0, 0, 1, 1, &match)) {
        fprintf(stderr, "alternate search did not find alternate screen\n");
        terminal_close(&terminal);
        return 0;
    }
    terminal_visible_line(&terminal, match.row, line, sizeof(line));
    if(strcmp(line, "ALT needle") != 0) {
        fprintf(stderr, "alternate search found wrong line '%s'\n", line);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int selection_selects_words_and_lines(void)
{
    TerminalState terminal;
    Selection selection;
    char text[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 48, 3);
    terminal_feed(&terminal, "open /tmp/kapsule-test.txt now\r\nsecond line",
                  (int)strlen(
                      "open /tmp/kapsule-test.txt now\r\nsecond line"));
    selection_clear(&selection);
    selection_select_word(&selection, &terminal, 0, 8);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "/tmp/kapsule-test.txt") != 0) {
        fprintf(stderr, "word selection expected path, got '%s'\n", text);
        terminal_close(&terminal);
        return 0;
    }
    if(!selection_contains(&selection, 0, 5) ||
       !selection_contains(&selection, 0, 25) ||
       selection_contains(&selection, 0, 26)) {
        fprintf(stderr, "word selection contains range failed\n");
        terminal_close(&terminal);
        return 0;
    }
    selection_select_line(&selection, &terminal, 1);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "second line") != 0) {
        fprintf(stderr, "line selection expected line, got '%s'\n", text);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 48, 2);
    terminal_feed(&terminal, "alpha beta gamma", 16);
    selection_clear(&selection);
    selection_select_word(&selection, &terminal, 0, 7);
    selection_update_end(&selection, &terminal, 0, 1);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "alpha beta") != 0) {
        fprintf(stderr, "backward word drag expected words, got '%s'\n",
                text);
        terminal_close(&terminal);
        return 0;
    }
    selection_select_word(&selection, &terminal, 0, 7);
    selection_update_end(&selection, &terminal, 0, 12);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "beta gamma") != 0) {
        fprintf(stderr, "forward word drag expected words, got '%s'\n", text);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int selection_collection_respects_wrapped_lines(void)
{
    TerminalState terminal;
    Selection selection;
    char text[256];

    terminal_init(&terminal);
    terminal_resize(&terminal, 8, 3);
    terminal_feed(&terminal, "abcdefghijklmnopq", 17);
    if(!terminal_visible_line_wrapped(&terminal, 0) ||
       !terminal_visible_line_wrapped(&terminal, 1)) {
        fprintf(stderr, "wrapped selection setup failed\n");
        terminal_close(&terminal);
        return 0;
    }
    selection_set_range(&selection, SELECTION_MODE_CHAR, 0, 0, 0, 2, 1);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "abcdefghijklmnopq") != 0) {
        fprintf(stderr, "wrapped selection expected joined text, got '%s'\n",
                text);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 16, 3);
    terminal_feed(&terminal, "first\r\nnext", (int)strlen("first\r\nnext"));
    selection_set_range(&selection, SELECTION_MODE_CHAR, 0, 0, 0, 1, 4);
    if(!selection_collect_text(&selection, &terminal, text, sizeof(text)) ||
       strcmp(text, "first\nnext") != 0) {
        fprintf(stderr, "wrapped selection newline expected, got '%s'\n",
                text);
        terminal_close(&terminal);
        return 0;
    }
    terminal_close(&terminal);
    return 1;
}

static int selection_edge_scroll_uses_viewport_edges(void)
{
    if(selection_edge_scroll_delta(90.0f, 100.0f, 200.0f, 24.0f) != 1 ||
       selection_edge_scroll_delta(110.0f, 100.0f, 200.0f, 24.0f) != 1 ||
       selection_edge_scroll_delta(180.0f, 100.0f, 200.0f, 24.0f) != 0 ||
       selection_edge_scroll_delta(286.0f, 100.0f, 200.0f, 24.0f) != -1 ||
       selection_edge_scroll_delta(310.0f, 100.0f, 200.0f, 24.0f) != -1) {
        fprintf(stderr, "selection edge scroll delta failed\n");
        return 0;
    }
    if(selection_edge_scroll_row(12, 8, 1) != 12 ||
       selection_edge_scroll_row(12, 8, -1) != 19 ||
       selection_edge_scroll_row(12, 8, 0) != -1 ||
       selection_edge_scroll_row(12, 0, 1) != -1) {
        fprintf(stderr, "selection edge scroll row failed\n");
        return 0;
    }
    if(selection_first_visible_row(40, 8, 0) != 32 ||
       selection_first_visible_row(40, 8, 5) != 27 ||
       selection_first_visible_row(40, 8, 100) != 0 ||
       selection_first_visible_row(4, 8, 0) != 0) {
        fprintf(stderr, "selection first visible row failed\n");
        return 0;
    }
    if(selection_edge_scroll_row(selection_first_visible_row(40, 8, 1), 8,
                                 1) != 31 ||
       selection_edge_scroll_row(selection_first_visible_row(40, 8, 0), 8,
                                 -1) != 39) {
        fprintf(stderr, "selection edge row after scroll failed\n");
        return 0;
    }
    return 1;
}

int main(void)
{
    TerminalState terminal;
    const Cell *cell;
    const Cell *first_cell;
    const Cell *second_cell;
    const char *styled = "hello\r\n\x1b[31mred\x1b[0m\r\n";
    const char *cleared = "\x1b[2Jclear";
    const char *delete_case = "\r\x1b[2Kabcd\x1b[2D\x1b[PZ";
    const char *bracketed = "\x1b[?2004h";
    const char *osc = "\x1b]2;Kapsule\a\x1b]10;#112233\a"
                      "\x1b]11;rgb:4455/6677/8899\x1b\\"
                      "\x1b]12;rgb:aa/bb/cc\a"
                      "\x1b]13;#778899\a"
                      "\x1b]14;#a0b0c0\a"
                      "\x1b]17;#224466\a"
                      "\x1b]19;#ddeeff\a"
                      "\x1b]4;1;#010203;2;rgb:1020/3040/5060\a"
                      "\x1b]7;file://omega/home/wao/Projects/Kapsule%20Test\a"
                      "\x1b]52;c;aGVsbG8=\a";
    const char *alternate = "\x1b[?1049hALT\x1b[?1049l";
    const char *scroll = "one\r\ntwo\r\nthree\r\nfour\r\nfive\r\n";

    if(!config_terminal_cursor_color_parses())
        return 1;
    if(!config_save_load_escapes_profile_text())
        return 1;
    if(!launch_options_accept_xfce_aliases())
        return 1;
    if(!launch_options_execute_quotes_remainder())
        return 1;
    if(!launch_options_reject_bad_geometry())
        return 1;
    if(!launch_options_accept_drop_down())
        return 1;
    if(!launch_options_builds_tab_specs())
        return 1;
    if(!launch_options_tab_separator_at_start())
        return 1;
    if(!launch_options_reject_too_many_tabs())
        return 1;
    if(!session_store_roundtrips_escaped_tabs())
        return 1;
    if(!palette_defaults_follow_kryon_theme_tokens())
        return 1;
    if(!profile_applies_configured_terminal_colors())
        return 1;
    if(!profile_sync_preserves_terminal_overrides())
        return 1;
    terminal_init(&terminal);
    terminal_resize(&terminal, 40, 8);
    terminal_feed(&terminal, styled, (int)strlen(styled));
    if(!line_has(&terminal, "hello") || !line_has(&terminal, "red")) {
        fprintf(stderr, "basic feed failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 0, 1);
    if(cell == NULL || cell->fg != 1) {
        fprintf(stderr, "sgr color failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[38:2::17:34:51mF\x1b[48:2::68:85:102mB\x1b[0m",
                  (int)strlen(
                      "\x1b[38:2::17:34:51mF\x1b[48:2::68:85:102mB\x1b[0m"));
    cell = terminal_cell(&terminal, 0, 2);
    if(cell == NULL || cell->fg != (COLOR_TRUE_RGB | 0x112233)) {
        fprintf(stderr, "sgr colon foreground failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 1, 2);
    if(cell == NULL || cell->bg != (COLOR_TRUE_RGB | 0x445566)) {
        fprintf(stderr, "sgr colon background failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[4;58;5;12mI\x1b[58:2::1:2:3mT\x1b[59mR",
                  (int)strlen(
                      "\x1b[4;58;5;12mI\x1b[58:2::1:2:3mT\x1b[59mR"));
    cell = terminal_cell(&terminal, 2, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != 12) {
        fprintf(stderr, "sgr indexed underline color failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 3, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != (COLOR_TRUE_RGB | 0x010203)) {
        fprintf(stderr, "sgr truecolor underline color failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 4, 2);
    if(cell == NULL || (cell->style & STYLE_UNDERLINE) == 0 ||
       cell->underline != COLOR_DEFAULT) {
        fprintf(stderr, "sgr underline color reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\r\n\x1b[1;2;5;8;53mD\x1b[22;25mF\x1b[28;55mV",
                  (int)strlen(
                      "\r\n\x1b[1;2;5;8;53mD\x1b[22;25mF\x1b[28;55mV"));
    cell = terminal_cell(&terminal, 0, 3);
    if(cell == NULL || (cell->style & STYLE_BOLD) == 0 ||
       (cell->style & STYLE_FAINT) == 0 ||
       (cell->style & STYLE_BLINK) == 0 ||
       (cell->style & STYLE_CONCEAL) == 0 ||
       (cell->style & STYLE_OVERLINE) == 0) {
        fprintf(stderr, "sgr extended attributes set failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 1, 3);
    if(cell == NULL || (cell->style & STYLE_BOLD) != 0 ||
       (cell->style & STYLE_FAINT) != 0 ||
       (cell->style & STYLE_BLINK) != 0 ||
       (cell->style & STYLE_CONCEAL) == 0) {
        fprintf(stderr, "sgr intensity/blink reset failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 2, 3);
    if(cell == NULL || (cell->style & STYLE_CONCEAL) != 0 ||
       (cell->style & STYLE_OVERLINE) != 0) {
        fprintf(stderr, "sgr conceal/overline reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, cleared, (int)strlen(cleared));
    if(!line_has(&terminal, "clear")) {
        fprintf(stderr, "clear screen failed\n");
        return 1;
    }
    terminal_feed(&terminal, delete_case, (int)strlen(delete_case));
    if(!line_equals(&terminal, 0, "abZ")) {
        fprintf(stderr, "delete char failed\n");
        return 1;
    }
    terminal_feed(&terminal, bracketed, (int)strlen(bracketed));
    if(!terminal.bracketed_paste) {
        fprintf(stderr, "bracketed paste mode failed\n");
        return 1;
    }
    terminal_feed(&terminal, osc, (int)strlen(osc));
    if(strcmp(terminal.title, "Kapsule") != 0 ||
       terminal.icon_title[0] != '\0' ||
       terminal.default_fg != (COLOR_TRUE_RGB | 0x112233) ||
       terminal.default_bg != (COLOR_TRUE_RGB | 0x446688) ||
       terminal.cursor_color != (COLOR_TRUE_RGB | 0xaabbcc) ||
       terminal.mouse_fg != (COLOR_TRUE_RGB | 0x778899) ||
       terminal.mouse_bg != (COLOR_TRUE_RGB | 0xa0b0c0) ||
       terminal.selection_bg != (COLOR_TRUE_RGB | 0x224466) ||
       terminal.selection_fg != (COLOR_TRUE_RGB | 0xddeeff) ||
       terminal.palette_overrides[1] != (COLOR_TRUE_RGB | 0x010203) ||
       terminal.palette_overrides[2] != (COLOR_TRUE_RGB | 0x103050) ||
       strcmp(terminal.current_directory,
              "/home/wao/Projects/Kapsule Test") != 0 ||
       !UIClipboardBufferHasPendingWrite(&terminal.clipboard) ||
       strcmp(GetUIClipboardBufferText(&terminal.clipboard), "hello") != 0) {
        fprintf(stderr, "osc title/color failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b]2x;Wrong\a\x1b]x;Wrong\a\x1b]10x;#000000\a",
                  (int)strlen("\x1b]2x;Wrong\a\x1b]x;Wrong\a"
                              "\x1b]10x;#000000\a"));
    if(strcmp(terminal.title, "Kapsule") != 0 ||
       terminal.icon_title[0] != '\0' ||
       terminal.default_fg != (COLOR_TRUE_RGB | 0x112233)) {
        fprintf(stderr, "malformed osc command code changed state\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]1;Icon\a",
                  (int)strlen("\x1b]1;Icon\a"));
    if(strcmp(terminal.title, "Kapsule") != 0 ||
       strcmp(terminal.icon_title, "Icon") != 0) {
        fprintf(stderr, "osc icon title separation failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]2;Bad\nTitle\tOK\x01no\a",
                  (int)strlen("\x1b]2;Bad\nTitle\tOK\x01no\a"));
    if(strcmp(terminal.title, "BadTitle\tOKno") != 0 ||
       strcmp(terminal.icon_title, "Icon") != 0) {
        fprintf(stderr, "osc title sanitization failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]0;Both\a",
                  (int)strlen("\x1b]0;Both\a"));
    if(strcmp(terminal.title, "Both") != 0 ||
       strcmp(terminal.icon_title, "Both") != 0) {
        fprintf(stderr, "osc combined title failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b]22;0\a\x1b]2;Editor\a\x1b]1;EditorIcon\a"
                  "\x1b]22;2\a\x1b]2;Build\a\x1b]23;2\a\x1b]23;0\a",
                  (int)strlen("\x1b]22;0\a\x1b]2;Editor\a"
                              "\x1b]1;EditorIcon\a\x1b]22;2\a"
                              "\x1b]2;Build\a\x1b]23;2\a\x1b]23;0\a"));
    if(strcmp(terminal.title, "Both") != 0 ||
       strcmp(terminal.icon_title, "Both") != 0 ||
       terminal.title_stack_count != 0 ||
       terminal.icon_title_stack_count != 0) {
        fprintf(stderr, "osc title stack restore failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]22;1\a\x1b]1;TempIcon\a\x1b]23;1\a",
                  (int)strlen("\x1b]22;1\a\x1b]1;TempIcon\a\x1b]23;1\a"));
    if(strcmp(terminal.title, "Both") != 0 ||
       strcmp(terminal.icon_title, "Both") != 0 ||
       terminal.title_stack_count != 0 ||
       terminal.icon_title_stack_count != 0) {
        fprintf(stderr, "osc icon-title stack restore failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]22;x\a",
                  (int)strlen("\x1b]22;x\a"));
    if(terminal.title_stack_count != 0 ||
       terminal.icon_title_stack_count != 0) {
        fprintf(stderr, "malformed osc title target changed stack\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]104;1\a", 8);
    if(terminal.palette_overrides[1] != COLOR_DEFAULT ||
       terminal.palette_overrides[2] != (COLOR_TRUE_RGB | 0x103050)) {
        fprintf(stderr, "osc palette index reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]104\a", 6);
    if(terminal.palette_overrides[2] != COLOR_DEFAULT) {
        fprintf(stderr, "osc palette reset failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b]110\a\x1b]111\a\x1b]112\a\x1b]113\a\x1b]114\a"
                  "\x1b]117\a\x1b]119\a",
                  (int)strlen("\x1b]110\a\x1b]111\a\x1b]112\a"
                              "\x1b]113\a\x1b]114\a\x1b]117\a"
                              "\x1b]119\a"));
    if(terminal.default_fg != COLOR_DEFAULT ||
       terminal.default_bg != COLOR_DEFAULT ||
       terminal.cursor_color != COLOR_DEFAULT ||
       terminal.mouse_fg != COLOR_DEFAULT ||
       terminal.mouse_bg != COLOR_DEFAULT ||
       terminal.selection_bg != COLOR_DEFAULT ||
       terminal.selection_fg != COLOR_DEFAULT) {
        fprintf(stderr, "osc default color reset failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]10;#abc\a\x1b]11;#123456789\a",
                  (int)strlen("\x1b]10;#abc\a\x1b]11;#123456789\a"));
    if(terminal.default_fg != (COLOR_TRUE_RGB | 0xaabbcc) ||
       terminal.default_bg != (COLOR_TRUE_RGB | 0x124578)) {
        fprintf(stderr, "osc hash color widths failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b]10;rgbi:1/0.5/0\a"
                  "\x1b]4;3;rgbi:0/0.25/1\a",
                  (int)strlen("\x1b]10;rgbi:1/0.5/0\a"
                              "\x1b]4;3;rgbi:0/0.25/1\a"));
    if(terminal.default_fg != (COLOR_TRUE_RGB | 0xff8000) ||
       terminal.palette_overrides[3] != (COLOR_TRUE_RGB | 0x0040ff)) {
        fprintf(stderr, "osc rgbi color failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[3;1H\x1b]8;id=docs;https://example.test\aDocs"
                  "\x1b]8;;\a plain",
                  (int)strlen("\x1b[3;1H\x1b]8;id=docs;https://example.test\aDocs"
                              "\x1b]8;;\a plain"));
    cell = terminal_cell(&terminal, 0, 2);
    if(cell == NULL || cell->hyperlink <= 0 ||
       strcmp(terminal_hyperlink(&terminal, cell->hyperlink),
              "https://example.test") != 0 ||
       strcmp(terminal_hyperlink_id(&terminal, cell->hyperlink), "docs") != 0) {
        fprintf(stderr, "osc hyperlink attach failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 5, 2);
    if(cell == NULL || cell->hyperlink != 0) {
        fprintf(stderr, "osc hyperlink close failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[4;1H\x1b]8;id=docs;https://example.test/with\n"
                  "control\aSafe\x1b]8;id=docs;\aEnd",
                  (int)strlen("\x1b[4;1H\x1b]8;id=docs;"
                              "https://example.test/with\ncontrol\aSafe"
                              "\x1b]8;id=docs;\aEnd"));
    cell = terminal_cell(&terminal, 0, 3);
    if(cell == NULL || cell->hyperlink <= 0 ||
       strcmp(terminal_hyperlink(&terminal, cell->hyperlink),
              "https://example.test/withcontrol") != 0 ||
       strcmp(terminal_hyperlink_id(&terminal, cell->hyperlink), "docs") != 0) {
        fprintf(stderr, "osc hyperlink sanitized url failed\n");
        return 1;
    }
    cell = terminal_cell(&terminal, 4, 3);
    if(cell == NULL || cell->hyperlink != 0) {
        fprintf(stderr, "osc hyperlink close with params failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[4;8H\x1b]8;id=keep;https://keep.test\aK"
                  "\x1b]8;id=keep\aM",
                  (int)strlen("\x1b[4;8H\x1b]8;id=keep;https://keep.test\aK"
                              "\x1b]8;id=keep\aM"));
    cell = terminal_cell(&terminal, 8, 3);
    if(cell == NULL || cell->hyperlink <= 0 ||
       strcmp(terminal_hyperlink(&terminal, cell->hyperlink),
              "https://keep.test") != 0 ||
       strcmp(terminal_hyperlink_id(&terminal, cell->hyperlink), "keep") != 0) {
        fprintf(stderr, "osc hyperlink malformed sequence changed state\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[5;1H\x1b]8;id=one;https://same.test\aA"
                  "\x1b]8;;\a\x1b[5;3H"
                  "\x1b]8;id=two;https://same.test\aB\x1b]8;;\a",
                  (int)strlen("\x1b[5;1H\x1b]8;id=one;"
                              "https://same.test\aA\x1b]8;;\a"
                              "\x1b[5;3H\x1b]8;id=two;"
                              "https://same.test\aB\x1b]8;;\a"));
    first_cell = terminal_cell(&terminal, 0, 4);
    second_cell = terminal_cell(&terminal, 2, 4);
    if(first_cell == NULL || second_cell == NULL ||
       first_cell->hyperlink <= 0 || second_cell->hyperlink <= 0 ||
       first_cell->hyperlink == second_cell->hyperlink ||
       strcmp(terminal_hyperlink(&terminal, first_cell->hyperlink),
              "https://same.test") != 0 ||
       strcmp(terminal_hyperlink(&terminal, second_cell->hyperlink),
              "https://same.test") != 0 ||
       strcmp(terminal_hyperlink_id(&terminal, first_cell->hyperlink), "one") != 0 ||
       strcmp(terminal_hyperlink_id(&terminal, second_cell->hyperlink), "two") != 0) {
        fprintf(stderr, "osc hyperlink id identity failed\n");
        return 1;
    }
    terminal_feed(&terminal,
                  "\x1b[5;5H\x1b]8;;https://noid.test\aC\x1b]8;;\a"
                  "\x1b[5;7H\x1b]8;;https://noid.test\aD\x1b]8;;\a",
                  (int)strlen("\x1b[5;5H\x1b]8;;https://noid.test\aC"
                              "\x1b]8;;\a\x1b[5;7H\x1b]8;;https://noid.test\aD"
                              "\x1b]8;;\a"));
    first_cell = terminal_cell(&terminal, 4, 4);
    second_cell = terminal_cell(&terminal, 6, 4);
    if(first_cell == NULL || second_cell == NULL ||
       first_cell->hyperlink <= 0 || second_cell->hyperlink <= 0 ||
       first_cell->hyperlink != second_cell->hyperlink ||
       strcmp(terminal_hyperlink_id(&terminal, first_cell->hyperlink), "") != 0) {
        fprintf(stderr, "osc hyperlink no-id dedupe failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[?1004h", 8);
    if(!terminal.focus_reporting) {
        fprintf(stderr, "focus reporting enable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[?1004l", 8);
    if(terminal.focus_reporting) {
        fprintf(stderr, "focus reporting disable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[3 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_UNDERLINE ||
       !terminal.cursor_blink) {
        fprintf(stderr, "blinking underline cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[4 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_UNDERLINE ||
       terminal.cursor_blink) {
        fprintf(stderr, "steady underline cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[5 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BAR ||
       !terminal.cursor_blink) {
        fprintf(stderr, "blinking bar cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[6 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BAR ||
       terminal.cursor_blink) {
        fprintf(stderr, "steady bar cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[1 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BLOCK ||
       !terminal.cursor_blink) {
        fprintf(stderr, "blinking block cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[2 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_BLOCK ||
       terminal.cursor_blink) {
        fprintf(stderr, "steady block cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[0 q", 5);
    if(terminal.cursor_style != TERMINAL_CURSOR_DEFAULT ||
       !terminal.cursor_blink) {
        fprintf(stderr, "default cursor style failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b=", 2);
    if(!terminal.application_keypad) {
        fprintf(stderr, "application keypad enable failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b>", 2);
    if(terminal.application_keypad) {
        fprintf(stderr, "application keypad disable failed\n");
        return 1;
    }
    terminal_feed(&terminal, alternate, (int)strlen(alternate));
    if(terminal.alternate_screen || !line_has(&terminal, "abZ")) {
        fprintf(stderr, "alternate screen failed\n");
        return 1;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 20, 8);
    terminal_feed(&terminal, "\x1bPq#1;2;100;0;0!3~-$#2;2;0;100;0!2~\x1b\\",
                  (int)strlen("\x1bPq#1;2;100;0;0!3~-$#2;2;0;100;0!2~\x1b\\"));
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel image decode failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 0);

        if(image == NULL || image->width != 3 || image->height != 12 ||
           image->pixel_aspect_num != 1 || image->pixel_aspect_den != 1 ||
           image->pixels[0] != (COLOR_TRUE_RGB | 0xff0000) ||
           image->pixels[5 * image->width + 2] !=
               (COLOR_TRUE_RGB | 0xff0000) ||
           image->pixels[6 * image->width] !=
               (COLOR_TRUE_RGB | 0x00ff00) ||
           image->pixels[6 * image->width + 2] != COLOR_DEFAULT ||
           terminal.cursor_row != 2) {
            fprintf(stderr, "sixel image pixels failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\x1bP0;2q\"1;1;4;6#3;1;120;50;100?\x1b\\",
                  (int)strlen("\x1bP0;2q\"1;1;4;6#3;1;120;50;100?\x1b\\"));
    if(terminal_sixel_count(&terminal) != 2) {
        fprintf(stderr, "sixel second image failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 1);

        if(image == NULL || image->width != 4 || image->height != 6 ||
           image->pixel_aspect_num != 1 || image->pixel_aspect_den != 1 ||
           image->pixels[0] != (COLOR_TRUE_RGB | 0x000000)) {
            fprintf(stderr, "sixel raster/background failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\x1bPq\"2;1;2;6#4;2;0;0;100~~\x1b\\",
                  (int)strlen("\x1bPq\"2;1;2;6#4;2;0;0;100~~\x1b\\"));
    if(terminal_sixel_count(&terminal) != 3) {
        fprintf(stderr, "sixel aspect image failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 2);

        if(image == NULL || image->width != 2 || image->height != 6 ||
           image->pixel_aspect_num != 2 || image->pixel_aspect_den != 1 ||
           image->pixels[0] != (COLOR_TRUE_RGB | 0x0000ff)) {
            fprintf(stderr, "sixel pixel aspect failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\x1b[2J", 4);
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel clear failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1bPq!12\x1b\\",
                  (int)strlen("\x1bPq!12\x1b\\"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel malformed repeat failed\n");
        return 1;
    }
    {
        const unsigned char sixel_8bit[] = {0x90, 'q', '~', 0x9c};

        terminal_feed(&terminal, sixel_8bit, (int)sizeof(sixel_8bit));
    }
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel 8-bit dcs failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[1;1H\x1b[2K",
                  (int)strlen("\x1b[1;1H\x1b[2K"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel erase line failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1bPq~\x1b\\",
                  (int)strlen("\x1bPq~\x1b\\"));
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel erase display setup failed\n");
        return 1;
    }
    terminal_feed(&terminal, "\x1b[1;1H\x1b[J",
                  (int)strlen("\x1b[1;1H\x1b[J"));
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel erase display failed\n");
        return 1;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_set_scrollback_limit(&terminal, 2);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, "\x1b[H\x1bPq~\x1b\\\n\n\n",
                  (int)strlen("\x1b[H\x1bPq~\x1b\\\n\n\n"));
    if(terminal_sixel_count(&terminal) != 1) {
        fprintf(stderr, "sixel scrollback preserve failed\n");
        return 1;
    } else {
        const SixelImage *image = terminal_sixel_image(&terminal, 0);

        if(image == NULL || image->row != -1) {
            fprintf(stderr, "sixel scrollback anchor failed\n");
            return 1;
        }
    }
    terminal_feed(&terminal, "\n\n", 2);
    if(terminal_sixel_count(&terminal) != 0) {
        fprintf(stderr, "sixel scrollback eviction failed\n");
        return 1;
    }
    terminal_close(&terminal);

    terminal_init(&terminal);
    terminal_resize(&terminal, 12, 4);
    terminal_feed(&terminal, scroll, (int)strlen(scroll));
    if(terminal_scrollback_rows(&terminal) <= 0) {
        fprintf(stderr, "scrollback failed\n");
        return 1;
    }
    terminal_close(&terminal);
    if(!capture_key_sequence(KEY_TAB_CODE, MOD_SHIFT, "\x1b[Z"))
        return 1;
    if(!capture_key_sequence(KEY_ENTER_CODE, MOD_ALT, "\x1b\r"))
        return 1;
    if(!capture_codepoint_sequence('a', 0, "a"))
        return 1;
    if(!capture_codepoint_sequence('a', MOD_ALT, "\x1b""a"))
        return 1;
    if(!capture_codepoint_sequence('c', MOD_CTRL, "\x03"))
        return 1;
    if(!capture_codepoint_sequence('C', MOD_CTRL, "\x03"))
        return 1;
    if(!capture_codepoint_sequence('[', MOD_CTRL, "\x1b"))
        return 1;
    if(!capture_codepoint_sequence('/', MOD_CTRL, "\x1f"))
        return 1;
    if(!capture_codepoint_sequence('-', MOD_CTRL, "\x1f"))
        return 1;
    if(!capture_codepoint_sequence('8', MOD_CTRL, "\x7f"))
        return 1;
    if(!capture_codepoint_sequence('/', MOD_ALT | MOD_CTRL, "\x1b\x1f"))
        return 1;
    if(!capture_codepoint_sequence('c', MOD_ALT | MOD_CTRL, "\x1b\x03"))
        return 1;
    if(!capture_configured_codepoint_sequence("\x1b[>4;1m", 'c', MOD_CTRL,
                                             "\x1b[27;5;99~"))
        return 1;
    if(!capture_configured_codepoint_sequence("\x1b[>4;1m", 'a', MOD_SHIFT,
                                             "a"))
        return 1;
    if(!capture_configured_codepoint_sequence("\x1b[>4;2m", 'a', MOD_SHIFT,
                                             "\x1b[27;2;97~"))
        return 1;
    if(!capture_configured_codepoint_sequence("\x1b[>4;2m\x1b[>4;0m", 'c',
                                             MOD_CTRL, "\x03"))
        return 1;
    if(!capture_configured_codepoint_sequence("\x1b[>4;2m\x1b[>4n", 'c',
                                             MOD_CTRL, "\x03"))
        return 1;
    if(!capture_configured_key_sequence("\x1b[>4;1m", KEY_ENTER_CODE,
                                        MOD_CTRL, "\x1b[27;5;13~"))
        return 1;
    if(!capture_configured_key_sequence("\x1b[>4;1m", KEY_BACKSPACE_CODE,
                                        MOD_ALT | MOD_CTRL,
                                        "\x1b[27;7;127~"))
        return 1;
    if(!capture_configured_key_sequence("\x1b[>4;2m", KEY_ESCAPE_CODE,
                                        MOD_SHIFT, "\x1b[27;2;27~"))
        return 1;
    if(!capture_control_key_sequence(KEY_C, MOD_CTRL, "\x03"))
        return 1;
    if(!capture_control_key_sequence(KEY_SLASH, MOD_CTRL, "\x1f"))
        return 1;
    if(!capture_configured_control_key_sequence("\x1b[>4;1m", KEY_C,
                                                MOD_CTRL,
                                                "\x1b[27;5;99~"))
        return 1;
    if(!capture_configured_control_key_sequence("\x1b[>4;1m", KEY_SLASH,
                                                MOD_CTRL,
                                                "\x1b[27;5;47~"))
        return 1;
    if(!capture_configured_control_key_sequence("\x1b[>4;1m", KEY_C,
                                                MOD_ALT | MOD_CTRL,
                                                "\x1b[27;7;99~"))
        return 1;
    if(!capture_codepoint_sequence(0x00e9, MOD_ALT, "\x1b\xc3\xa9"))
        return 1;
    if(!capture_key_sequence(KEY_F1_CODE, 0, "\x1bOP"))
        return 1;
    if(!capture_key_sequence(KEY_F5_CODE, 0, "\x1b[15~"))
        return 1;
    if(!capture_key_sequence(KEY_F12_CODE, MOD_CTRL, "\x1b[24;5~"))
        return 1;
    if(!capture_key_sequence(KEY_F13_CODE, 0, "\x1b[1;2P"))
        return 1;
    if(!capture_key_sequence(KEY_F16_CODE, 0, "\x1b[1;2S"))
        return 1;
    if(!capture_key_sequence(KEY_F17_CODE, 0, "\x1b[15;2~"))
        return 1;
    if(!capture_key_sequence(KEY_F24_CODE, 0, "\x1b[24;2~"))
        return 1;
    if(!capture_key_sequence(KEY_F24_CODE, MOD_CTRL, "\x1b[24;6~"))
        return 1;
    if(!capture_function_key_sequence(1, MOD_SHIFT, "\x1b[1;2P"))
        return 1;
    if(!capture_function_key_sequence(12, MOD_SHIFT | MOD_CTRL,
                                      "\x1b[24;6~"))
        return 1;
    if(!capture_key_sequence(KEY_HOME_CODE, 0, "\x1b[H"))
        return 1;
    if(!capture_key_sequence(KEY_END_CODE, 0, "\x1b[F"))
        return 1;
    if(!capture_key_sequence(KEY_HOME_CODE, MOD_CTRL, "\x1b[1;5H"))
        return 1;
    if(!capture_key_sequence(KEY_END_CODE, MOD_CTRL, "\x1b[1;5F"))
        return 1;
    if(!capture_application_cursor_sequence())
        return 1;
    if(!capture_keypad_sequence(0, '7', "7"))
        return 1;
    if(!capture_keypad_sequence(1, '7', "\x1bOw"))
        return 1;
    if(!capture_keypad_sequence(1, '5', "\x1bOu"))
        return 1;
    if(!capture_keypad_sequence(1, '3', "\x1bOs"))
        return 1;
    if(!capture_keypad_sequence(1, '+', "\x1bOk"))
        return 1;
    if(!capture_keypad_sequence(1, '=', "\x1bOX"))
        return 1;
    if(!capture_keypad_sequence(1, '\r', "\x1bOM"))
        return 1;
    if(!capture_bracketed_paste())
        return 1;
    if(!capture_sanitized_bracketed_paste())
        return 1;
    if(!capture_clipboard_setter_query())
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_LEFT, 1, 0, 0,
                               "\x1b[<0;5;3M"))
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_LEFT, 0, 0, 0,
                               "\x1b[<0;5;3m"))
        return 1;
    if(!capture_mouse_sequence(1, TERMINAL_MOUSE_WHEEL_DOWN, 1, 0, MOD_CTRL,
                               "\x1b[<81;5;3M"))
        return 1;
    if(!capture_mouse_sequence(0, TERMINAL_MOUSE_LEFT, 1, 0, 0, "\x1b[M %#"))
        return 1;
    if(!capture_mouse_mode_sequence("\x1b[?9h", TERMINAL_MOUSE_LEFT, 1, 0, 0,
                                    "\x1b[M %#"))
        return 1;
    if(!capture_no_mouse_mode_sequence("x10 suppresses release",
                                       "\x1b[?9h", TERMINAL_MOUSE_LEFT,
                                       0, 0, 0))
        return 1;
    if(!capture_no_mouse_mode_sequence("x10 suppresses wheel",
                                       "\x1b[?9h",
                                       TERMINAL_MOUSE_WHEEL_DOWN, 1, 0, 0))
        return 1;
    if(!capture_mouse_mode_sequence("\x1b[?1000h\x1b[?1015h",
                                    TERMINAL_MOUSE_LEFT, 1, 0, MOD_CTRL,
                                    "\x1b[48;5;3M"))
        return 1;
    if(!capture_mouse_mode_sequence("\x1b[?1000h\x1b[?1005h\x1b[?1006h",
                                    TERMINAL_MOUSE_LEFT, 1, 0, 0,
                                    "\x1b[<0;5;3M"))
        return 1;
    if(!capture_mouse_mode_sequence("\x1b[?1000h\x1b[?1006h\x1b[?1015h",
                                    TERMINAL_MOUSE_LEFT, 1, 0, 0,
                                    "\x1b[32;5;3M"))
        return 1;
    if(!capture_no_mouse_mode_sequence("button-event suppresses hover motion",
                                       "\x1b[?1002h",
                                       TERMINAL_MOUSE_RELEASE, 1, 1, 0))
        return 1;
    if(!capture_utf8_mouse_sequence())
        return 1;
    if(!capture_pixel_mouse_sequence())
        return 1;
    if(!capture_alternate_scroll_sequence("\x1b[?1049h\x1b[?1007h", 1,
                                          "\x1b[A"))
        return 1;
    if(!capture_alternate_scroll_sequence("\x1b[?1049h\x1b[?1007h", -1,
                                          "\x1b[B"))
        return 1;
    if(!capture_alternate_scroll_sequence("\x1b[?1049h\x1b[?1007h\x1b[?1h",
                                          1, "\x1bOA"))
        return 1;
    if(!capture_focus_sequence(1, "\x1b[I"))
        return 1;
    if(!capture_focus_sequence(0, "\x1b[O"))
        return 1;
    if(!capture_osc_color_query())
        return 1;
    SetUIPrimarySelectionTextValue("");
    if(!capture_response_sequence("osc 52 clipboard query",
                                  "\x1b]52;c;aGVsbG8=\a\x1b]52;c;?\a",
                                  "\x1b]52;c;aGVsbG8=\a"))
        return 1;
    if(!capture_response_sequence("osc 52 empty clipboard query",
                                  "\x1b]52;p;?\a",
                                  "\x1b]52;p;\a"))
        return 1;
    if(!capture_response_sequence("osc 52 empty clipboard clears",
                                  "\x1b]52;c;aGVsbG8=\a\x1b]52;c;\a"
                                  "\x1b]52;c;?\a",
                                  "\x1b]52;c;\a"))
        return 1;
    if(!capture_response_sequence("osc 52 primary query",
                                  "\x1b]52;p;cHJpbWFyeQ==\a\x1b]52;p;?\a",
                                  "\x1b]52;p;cHJpbWFyeQ==\a"))
        return 1;
    if(!capture_response_sequence("osc 52 primary and clipboard separate",
                                  "\x1b]52;c;Y2xpcA==\a"
                                  "\x1b]52;p;cHJpbWFyeQ==\a"
                                  "\x1b]52;c;?\a\x1b]52;p;?\a",
                                  "\x1b]52;c;Y2xpcA==\a"
                                  "\x1b]52;p;cHJpbWFyeQ==\a"))
        return 1;
    if(!capture_response_sequence("osc 52 primary clears",
                                  "\x1b]52;p;cHJpbWFyeQ==\a"
                                  "\x1b]52;p;\a\x1b]52;p;?\a",
                                  "\x1b]52;p;\a"))
        return 1;
    if(!capture_response_sequence("osc 52 combined targets",
                                  "\x1b]52;cp;Ym90aA==\a"
                                  "\x1b]52;c;?\a\x1b]52;p;?\a",
                                  "\x1b]52;c;Ym90aA==\a"
                                  "\x1b]52;p;Ym90aA==\a"))
        return 1;
    if(!capture_response_sequence("osc 52 selection target",
                                  "\x1b]52;s;c2VsZWN0\a\x1b]52;s;?\a"
                                  "\x1b]52;c;?\a",
                                  "\x1b]52;s;c2VsZWN0\a"
                                  "\x1b]52;c;c2VsZWN0\a"))
        return 1;
    if(!capture_response_sequence("osc 52 unknown target fallback",
                                  "\x1b]52;x;ZmFsbGJhY2s=\a"
                                  "\x1b]52;c;?\a",
                                  "\x1b]52;c;ZmFsbGJhY2s=\a"))
        return 1;
    if(!capture_response_sequence("osc 52 invalid payload keeps clipboard",
                                  "\x1b]52;c;aGVsbG8=\a"
                                  "\x1b]52;c;%%%%\a\x1b]52;c;?\a",
                                  "\x1b]52;c;aGVsbG8=\a"))
        return 1;
    if(!c1_controls_parse_csi_and_osc())
        return 1;
    if(!control_strings_are_ignored())
        return 1;
    if(!bell_is_reported_without_text_output())
        return 1;
    if(!parser_cancel_controls_abort_active_sequence())
        return 1;
    if(!capture_response_sequence("primary device attributes", "\x1b[c",
                                  "\x1b[?1;2c"))
        return 1;
    if(!capture_response_sequence("decid device attributes", "\x1b" "Z",
                                  "\x1b[?1;2c"))
        return 1;
    if(!capture_response_sequence("secondary device attributes", "\x1b[>c",
                                  "\x1b[>0;0;0c"))
        return 1;
    if(!capture_response_sequence("device status report", "\x1b[5n",
                                  "\x1b[0n"))
        return 1;
    if(!capture_response_sequence("cursor position report", "abc\x1b[6n",
                                  "\x1b[1;4R"))
        return 1;
    if(!capture_response_sequence("private cursor position report",
                                  "abc\x1b[?6n",
                                  "\x1b[?1;4R"))
        return 1;
    if(!capture_response_sequence("private printer status report",
                                  "\x1b[?15n",
                                  "\x1b[?10n"))
        return 1;
    if(!capture_response_sequence("private udk status report",
                                  "\x1b[?25n",
                                  "\x1b[?20n"))
        return 1;
    if(!capture_response_sequence("private keyboard status report",
                                  "\x1b[?26n",
                                  "\x1b[?27;1;0;0n"))
        return 1;
    if(!capture_response_sequence("private locator status report",
                                  "\x1b[?55n\x1b[?56n",
                                  "\x1b[?53n\x1b[?57;0n"))
        return 1;
    if(!capture_response_sequence("private integrity/session reports",
                                  "\x1b[?75n\x1b[?85n",
                                  "\x1b[?70n\x1b[?83n"))
        return 1;
    if(!capture_response_sequence("terminal character size report",
                                  "\x1b[18t",
                                  "\x1b[8;4;24t"))
        return 1;
    if(!capture_response_sequence("screen character size report",
                                  "\x1b[19t",
                                  "\x1b[9;4;24t"))
        return 1;
    if(!capture_response_sequence("icon and window title report",
                                  "\x1b]1;Icon Label\a\x1b]2;Window Title\a"
                                  "\x1b[20t\x1b[21t",
                                  "\x1b]LIcon Label\x1b\\"
                                  "\x1b]lWindow Title\x1b\\"))
        return 1;
    if(!capture_response_sequence("title report strips control bytes",
                                  "\x1b]2;Bad\nTitle\tOK\a\x1b[21t",
                                  "\x1b]lBadTitle\tOK\x1b\\"))
        return 1;
    if(!capture_response_sequence("csi title stack saves both titles",
                                  "\x1b]1;Icon A\a\x1b]2;Window A\a"
                                  "\x1b[22;0t"
                                  "\x1b]1;Icon B\a\x1b]2;Window B\a"
                                  "\x1b[23;0t\x1b[20t\x1b[21t",
                                  "\x1b]LIcon A\x1b\\"
                                  "\x1b]lWindow A\x1b\\"))
        return 1;
    if(!capture_response_sequence("csi title stack target selection",
                                  "\x1b]1;Icon A\a\x1b]2;Window A\a"
                                  "\x1b[22;1t\x1b[22;2t"
                                  "\x1b]1;Icon B\a\x1b]2;Window B\a"
                                  "\x1b[23;1t\x1b[20t\x1b[21t"
                                  "\x1b[23;2t\x1b[20t\x1b[21t",
                                  "\x1b]LIcon A\x1b\\"
                                  "\x1b]lWindow B\x1b\\"
                                  "\x1b]LIcon A\x1b\\"
                                  "\x1b]lWindow A\x1b\\"))
        return 1;
    if(!capture_response_sequence("decrqss sgr report",
                                  "\x1b[1;31;48;2;1;2;3m\x1bP$qm\x1b\\",
                                  "\x1bP1$r1;38;5;1;48;2;1;2;3m\x1b\\"))
        return 1;
    if(!capture_response_sequence("decrqss margins report",
                                  "\x1b[2;3r\x1bP$qr\x1b\\",
                                  "\x1bP1$r2;3r\x1b\\"))
        return 1;
    if(!capture_response_sequence("decrqss cursor style report",
                                  "\x1b[5 q\x1bP$q q\x1b\\",
                                  "\x1bP1$r5 q\x1b\\"))
        return 1;
    if(!capture_response_sequence("decrqss character protection report",
                                  "\x1bP$q\" q\x1b\\",
                                  "\x1bP1$r0\"q\x1b\\"))
        return 1;
    if(!capture_response_sequence("decrqss invalid report",
                                  "\x1bP$qx\x1b\\",
                                  "\x1bP0$r\x1b\\"))
        return 1;
    if(!capture_response_sequence("xtgettcap terminal name",
                                  "\x1bP+q544e\x1b\\",
                                  "\x1bP1+r544e=787465726d2d323536636f6c6f72\x1b\\"))
        return 1;
    if(!capture_response_sequence("xtgettcap color and key",
                                  "\x1bP+q436f;6b63757531;6b6635\x1b\\",
                                  "\x1bP1+r436f=323536;6b63757531=1b5b41;6b6635=1b5b31357e\x1b\\"))
        return 1;
    if(!capture_response_sequence("xtgettcap extended function key",
                                  "\x1bP+q6b663234\x1b\\",
                                  "\x1bP1+r6b663234=1b5b32343b327e\x1b\\"))
        return 1;
    if(!capture_response_sequence("xtgettcap keypad keys",
                                  "\x1bP+q6b6131;6b6232;6b6333;6b656e74\x1b\\",
                                  "\x1bP1+r6b6131=1b4f77;6b6232=1b4f75;6b6333=1b4f73;6b656e74=1b4f4d\x1b\\"))
        return 1;
    if(!capture_response_sequence("xtgettcap invalid report",
                                  "\x1bP+q5858\x1b\\",
                                  "\x1bP0+r\x1b\\"))
        return 1;
    if(!capture_response_sequence("private mode report set", "\x1b[?25$p",
                                  "\x1b[?25;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report reset",
                                  "\x1b[?25l\x1b[?25$p",
                                  "\x1b[?25;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report cursor blink",
                                  "\x1b[?12$p\x1b[?12l\x1b[?12$p"
                                  "\x1b[?12h\x1b[?12$p",
                                  "\x1b[?12;1$y\x1b[?12;2$y\x1b[?12;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report bracketed paste",
                                  "\x1b[?2004h\x1b[?2004$p",
                                  "\x1b[?2004;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report utf8 mouse",
                                  "\x1b[?1005h\x1b[?1005$p"
                                  "\x1b[?1005l\x1b[?1005$p",
                                  "\x1b[?1005;1$y\x1b[?1005;2$y"))
        return 1;
    if(!capture_response_sequence("private mode mouse encoding exclusivity",
                                  "\x1b[?1005h\x1b[?1006h"
                                  "\x1b[?1005$p\x1b[?1006$p"
                                  "\x1b[?1015h\x1b[?1006$p\x1b[?1015$p"
                                  "\x1b[?1016h\x1b[?1015$p\x1b[?1016$p",
                                  "\x1b[?1005;2$y\x1b[?1006;1$y"
                                  "\x1b[?1006;2$y\x1b[?1015;1$y"
                                  "\x1b[?1015;2$y\x1b[?1016;1$y"))
        return 1;
    if(!capture_response_sequence("private mode unrelated mouse reset",
                                  "\x1b[?1000h\x1b[?1002l\x1b[?1000$p"
                                  "\x1b[?1000l\x1b[?1000$p",
                                  "\x1b[?1000;1$y\x1b[?1000;2$y"))
        return 1;
    if(!capture_response_sequence("private mode button-event reset isolation",
                                  "\x1b[?1002h\x1b[?1000l\x1b[?1002$p",
                                  "\x1b[?1002;1$y"))
        return 1;
    if(!capture_response_sequence("private mode report alternate scroll",
                                  "\x1b[?1007h\x1b[?1007$p"
                                  "\x1b[?1007l\x1b[?1007$p",
                                  "\x1b[?1007;1$y\x1b[?1007;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report alternate screen",
                                  "\x1b[?1047$p\x1b[?1047h\x1b[?1047$p"
                                  "\x1b[?1047l\x1b[?1047$p",
                                  "\x1b[?1047;2$y\x1b[?1047;1$y"
                                  "\x1b[?1047;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report cursor save",
                                  "\x1b[?1048$p\x1b[?1048h"
                                  "\x1b[?1048$p\x1b[?1048l"
                                  "\x1b[?1048$p",
                                  "\x1b[?1048;2$y\x1b[?1048;2$y"
                                  "\x1b[?1048;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report urxvt mouse",
                                  "\x1b[?1015h\x1b[?1015$p"
                                  "\x1b[?1015l\x1b[?1015$p",
                                  "\x1b[?1015;1$y\x1b[?1015;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report pixel mouse",
                                  "\x1b[?1016h\x1b[?1016$p"
                                  "\x1b[?1016l\x1b[?1016$p",
                                  "\x1b[?1016;1$y\x1b[?1016;2$y"))
        return 1;
    if(!capture_response_sequence("private mode report unknown",
                                  "\x1b[?9999$p",
                                  "\x1b[?9999;0$y"))
        return 1;
    if(!capture_response_sequence("insert mode report set",
                                  "\x1b[4h\x1b[4$p",
                                  "\x1b[4;1$y"))
        return 1;
    if(!capture_response_sequence("insert mode report reset",
                                  "\x1b[4l\x1b[4$p",
                                  "\x1b[4;2$y"))
        return 1;
    if(!capture_response_sequence("newline mode report",
                                  "\x1b[20$p\x1b[20h\x1b[20$p"
                                  "\x1b[20l\x1b[20$p",
                                  "\x1b[20;2$y\x1b[20;1$y"
                                  "\x1b[20;2$y"))
        return 1;
    if(!resize_reflows_wrapped_text())
        return 1;
    if(!visible_line_wrapped_reports_soft_wraps())
        return 1;
    if(!resize_preserves_cursor_on_sparse_screen())
        return 1;
    if(!resize_reflows_hidden_main_screen_in_alternate_mode())
        return 1;
    if(!dec_autowrap_mode_controls_right_margin())
        return 1;
    if(!resize_reflows_scrollback())
        return 1;
    if(!resize_reflows_wrapped_scrollback_boundary())
        return 1;
    if(!origin_mode_uses_scroll_region())
        return 1;
    if(!erase_saved_lines_clears_scrollback_only())
        return 1;
    if(!insert_mode_inserts_printable_text())
        return 1;
    if(!csi_repeat_and_cursor_aliases_work())
        return 1;
    if(!dec_screen_alignment_fills_visible_grid())
        return 1;
    if(!escape_next_line_moves_to_column_zero())
        return 1;
    if(!c1_single_byte_controls_work())
        return 1;
    if(!tab_clear_controls_work())
        return 1;
    if(!wide_characters_use_two_columns())
        return 1;
    if(!combining_marks_stay_on_base_cell())
        return 1;
    if(!dec_special_graphics_charset_maps_line_drawing())
        return 1;
    if(!save_restore_preserves_rendition_and_charset())
        return 1;
    if(!osc_current_directory_updates_session_title())
        return 1;
    if(!decstr_soft_reset_preserves_screen_and_resets_modes())
        return 1;
    if(!ris_hard_reset_returns_to_clean_main_screen())
        return 1;
    if(!modifier_key_disable_sequence_disables_modify_other_keys())
        return 1;
    if(!visible_search_finds_scrollback_with_smartcase())
        return 1;
    if(!visible_search_wraps_and_respects_direction())
        return 1;
    if(!visible_search_uses_alternate_screen_only())
        return 1;
    if(!selection_selects_words_and_lines())
        return 1;
    if(!selection_collection_respects_wrapped_lines())
        return 1;
    if(!selection_edge_scroll_uses_viewport_edges())
        return 1;
    if(!alternate_screen_modes_preserve_expected_state())
        return 1;
    if(!newline_mode_controls_linefeed())
        return 1;
    if(!utf8_terminal_glyphs_roundtrip())
        return 1;
    if(!ctrl_c_interrupts_child())
        return 1;
    if(!interactive_bash_sources_bashrc())
        return 1;
    if(!command_bash_sources_bashrc())
        return 1;
    if(!close_does_not_hang_on_stubborn_child())
        return 1;
    printf("ok terminal\n");
    return 0;
}
