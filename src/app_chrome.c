#include "app_chrome.h"

#include "kryon.h"

int app_menu_bar_height(const State *app)
{
    return app != NULL && app->launch.show_menubar ? ScaleUIPx(34) : 0;
}

int app_tab_bar_visible(const State *app)
{
    return app != NULL && app->launch.show_toolbar &&
           (app->config.always_show_tabs || app->session_count > 1);
}

int app_tab_bar_height(const State *app)
{
    return app_tab_bar_visible(app) ? TabBarHeight() : 0;
}

int app_chrome_height(const State *app)
{
    return app_menu_bar_height(app) + app_tab_bar_height(app);
}
