#include "palette.h"

#include "terminal.h"

#include <string.h>

void palette_default(Palette *palette)
{
    static const Color base16[16] = {
        {24, 24, 24, 255},     {205, 49, 49, 255},   {13, 188, 121, 255},
        {229, 229, 16, 255},   {36, 114, 200, 255},  {188, 63, 188, 255},
        {17, 168, 205, 255},   {229, 229, 229, 255}, {102, 102, 102, 255},
        {241, 76, 76, 255},    {35, 209, 139, 255},  {245, 245, 67, 255},
        {59, 142, 234, 255},   {214, 112, 214, 255}, {41, 184, 219, 255},
        {255, 255, 255, 255},
    };
    int i;
    int r;
    int g;
    int b;

    if(palette == NULL)
        return;
    memset(palette, 0, sizeof(*palette));
    palette->background = (Color){218, 216, 213, 255};
    palette->terminal_background = (Color){20, 20, 20, 255};
    palette->foreground = (Color){224, 228, 235, 255};
    palette->muted = (Color){112, 116, 122, 255};
    palette->selection = (Color){62, 108, 167, 190};
    palette->chrome = (Color){232, 230, 226, 255};
    palette->chrome_light = (Color){247, 246, 244, 255};
    palette->chrome_border = (Color){162, 158, 151, 255};
    palette->menu_text = (Color){32, 34, 36, 255};
    palette->tab = (Color){205, 202, 197, 255};
    palette->tab_active = (Color){250, 249, 247, 255};
    for(i = 0; i < 16; i++)
        palette->ansi[i] = base16[i];
    i = 16;
    for(r = 0; r < 6; r++) {
        for(g = 0; g < 6; g++) {
            for(b = 0; b < 6; b++) {
                palette->ansi[i++] =
                    (Color){(unsigned char)(r == 0 ? 0 : 55 + r * 40),
                            (unsigned char)(g == 0 ? 0 : 55 + g * 40),
                            (unsigned char)(b == 0 ? 0 : 55 + b * 40), 255};
            }
        }
    }
    for(i = 232; i < 256; i++) {
        unsigned char gray = (unsigned char)(8 + (i - 232) * 10);

        palette->ansi[i] = (Color){gray, gray, gray, 255};
    }
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
    Color link;
    Color dark = {0, 0, 0, 255};
    Color light = {255, 255, 255, 255};

    if(palette == NULL)
        return;
    SetThemeSource(THEME_SOURCE_SYSTEM);
    SetThemeMode(THEME_MODE_SYSTEM);
    RefreshSystemTheme();
    ReloadThemes();
    SetThemeStyle(GetDefaultPlatformThemeStyle());
    SetCurrentTheme(GetDefaultThemeForThemeStyle(GetEffectiveThemeStyle()),
                    GetEffectiveThemeDarkMode() ? 1 : 0);

    bg = GetThemeBackground();
    surface = GetThemeSurface();
    text = GetThemeText();
    button = GetThemeButton();
    hover = GetThemeButtonHover();
    link = GetThemeLink();

    if(color_visible(bg))
        palette->background = bg;
    if(color_visible(surface))
        palette->chrome = surface;
    if(color_visible(button))
        palette->tab = button;
    if(color_visible(hover))
        palette->tab_active = hover;
    if(color_visible(text))
        palette->menu_text = text;
    palette->chrome_light = mix_color(palette->chrome, light, 0.18f);
    palette->chrome_border = mix_color(palette->chrome,
                                       GetEffectiveThemeDarkMode() ? light : dark,
                                       0.32f);
    if(color_visible(link))
        palette->selection = Fade(link, 0.48f);
}

Color palette_resolve(const Palette *palette, int value, Color fallback)
{
    if((value & COLOR_TRUE_RGB) != 0) {
        int rgb = value & 0xffffff;

        return (Color){(unsigned char)((rgb >> 16) & 255),
                       (unsigned char)((rgb >> 8) & 255),
                       (unsigned char)(rgb & 255), 255};
    }
    if(palette != NULL && value >= 0 && value < 256)
        return palette->ansi[value];
    return fallback;
}
