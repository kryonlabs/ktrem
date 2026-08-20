#include "palette.h"

#include <string.h>

static int color_visible(Color color);
static Color mix_color(Color a, Color b, float t);

void palette_default(Palette *palette)
{
    TerminalPaneColors terminal_colors;
    Color bg;
    Color surface;
    Color text;
    Color button;
    Color hover;
    Color dark = {0, 0, 0, 255};
    Color light = {255, 255, 255, 255};

    if(palette == NULL)
        return;
    memset(palette, 0, sizeof(*palette));
    terminal_colors =
        ResolveTerminalPaneThemeColors(GetTerminalPaneThemeColors());
    bg = GetThemeBackground();
    surface = GetThemeSurface();
    text = GetThemeText();
    button = GetThemeButton();
    hover = GetThemeButtonHover();
    palette->background = bg;
    palette->terminal_background = terminal_colors.background;
    palette->foreground = terminal_colors.text;
    palette->muted = terminal_colors.muted_text;
    palette->selection = terminal_colors.selection;
    palette->selection_text = terminal_colors.selection_text;
    palette->cursor = terminal_colors.cursor;
    palette->scroll_indicator = terminal_colors.scroll_indicator;
    palette->scroll_indicator_text = terminal_colors.scroll_indicator_text;
    palette->bell_overlay = terminal_colors.bell_overlay;
    palette->bell_border = terminal_colors.bell_border;
    palette->link = terminal_colors.link;
    palette->chrome = surface;
    palette->chrome_light = mix_color(surface, light, 0.18f);
    palette->chrome_border = terminal_colors.border;
    palette->menu_text = text;
    palette->tab = button;
    palette->tab_active = hover;
    if(!color_visible(palette->chrome_border))
        palette->chrome_border =
            mix_color(surface, GetEffectiveThemeDarkMode() ? light : dark,
                      0.32f);
    palette->terminal_palette = GetTerminalPaneDefaultPalette();
}

static int color_visible(Color color)
{
    return color.a != 0;
}

static Color mix_color(Color a, Color b, float t)
{
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    return (Color){(unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
                   (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                   (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
                   255};
}

void palette_apply_system_theme(Palette *palette)
{
    Color bg;
    Color surface;
    Color text;
    Color button;
    Color hover;
    TerminalPaneColors terminal_colors;
    Color light = {255, 255, 255, 255};

    if(palette == NULL)
        return;
    SetThemeSource(THEME_SOURCE_SYSTEM);
    SetThemeMode(THEME_MODE_SYSTEM);
    RefreshSystemTheme();
    ReloadThemes();
    SetThemeStyle(THEME_STYLE_SYSTEM);
    SetCurrentTheme(GetDefaultThemeForThemeStyle(GetEffectiveThemeStyle()),
                    GetEffectiveThemeDarkMode() ? 1 : 0);

    bg = GetThemeBackground();
    surface = GetThemeSurface();
    text = GetThemeText();
    button = GetThemeButton();
    hover = GetThemeButtonHover();
    terminal_colors =
        ResolveTerminalPaneThemeColors(GetTerminalPaneThemeColors());

    if(color_visible(bg))
        palette->background = bg;
    palette->terminal_background = terminal_colors.background;
    if(color_visible(surface))
        palette->chrome = surface;
    if(color_visible(button))
        palette->tab = button;
    if(color_visible(hover))
        palette->tab_active = hover;
    palette->foreground = terminal_colors.text;
    if(color_visible(text))
        palette->menu_text = text;
    palette->muted = terminal_colors.muted_text;
    palette->chrome_light = mix_color(palette->chrome, light, 0.18f);
    palette->chrome_border = terminal_colors.border;
    palette->selection = terminal_colors.selection;
    palette->selection_text = terminal_colors.selection_text;
    palette->cursor = terminal_colors.cursor;
    palette->scroll_indicator = terminal_colors.scroll_indicator;
    palette->scroll_indicator_text = terminal_colors.scroll_indicator_text;
    palette->bell_overlay = terminal_colors.bell_overlay;
    palette->bell_border = terminal_colors.bell_border;
    palette->link = terminal_colors.link;
}
