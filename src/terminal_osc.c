#include "terminal_osc.h"

#include "terminal_pane.h"

#include <stdio.h>
#include <string.h>

static int write_osc52_response(void *userdata, const char *text)
{
    TerminalState *terminal = userdata;

    if(terminal == NULL || text == NULL)
        return 0;
    return terminal_write_text(terminal, text);
}

int terminal_default_palette_color(int index)
{
    return TerminalPaneDefaultPaletteColor(index);
}

static int find_or_add_hyperlink(TerminalState *terminal, const char *id,
                                 const char *url)
{
    int i;

    if(terminal == NULL || url == NULL || url[0] == '\0')
        return 0;
    if(id == NULL)
        id = "";
    for(i = 0; i < terminal->hyperlink_count; i++) {
        if(id[0] != '\0') {
            if(strcmp(terminal->hyperlink_ids[i], id) == 0 &&
               strcmp(terminal->hyperlinks[i], url) == 0)
                return i + 1;
        } else if(terminal->hyperlink_ids[i][0] == '\0' &&
                  strcmp(terminal->hyperlinks[i], url) == 0) {
            return i + 1;
        }
    }
    if(terminal->hyperlink_count >= MAX_HYPERLINKS)
        return terminal->hyperlink_count;
    snprintf(terminal->hyperlink_ids[terminal->hyperlink_count],
             sizeof(terminal->hyperlink_ids[terminal->hyperlink_count]), "%s",
             id);
    snprintf(terminal->hyperlinks[terminal->hyperlink_count],
             sizeof(terminal->hyperlinks[terminal->hyperlink_count]), "%s",
             url);
    terminal->hyperlink_count++;
    return terminal->hyperlink_count;
}

static int send_osc_color_response(TerminalState *terminal, int code, int color)
{
    char response[64];
    int len;

    if(terminal == NULL)
        return 0;
    len = FormatTerminalPaneOSCColorResponse(response, (int)sizeof(response),
                                             code, color);
    if(len <= 0 || len >= (int)sizeof(response))
        return 0;
    return terminal_write_text(terminal, response);
}

static int send_osc_palette_response(TerminalState *terminal, int index,
                                     int color)
{
    char response[80];
    int len;

    if(terminal == NULL)
        return 0;
    len = FormatTerminalPaneOSCPaletteResponse(response, (int)sizeof(response),
                                               index, color);
    if(len <= 0 || len >= (int)sizeof(response))
        return 0;
    return terminal_write_text(terminal, response);
}

static TerminalPaneOSCColorState terminal_osc_color_state(
    const TerminalState *terminal)
{
    TerminalPaneOSCColorState state = {0};

    if(terminal == NULL)
        return state;
    state.foreground = terminal->default_fg;
    state.background = terminal->default_bg;
    state.cursor = terminal->cursor_color;
    state.mouse_foreground = terminal->mouse_fg;
    state.mouse_background = terminal->mouse_bg;
    state.selection_foreground = terminal->selection_fg;
    state.selection_background = terminal->selection_bg;
    state.base_foreground = terminal->base_fg;
    state.base_background = terminal->base_bg;
    state.base_cursor = terminal->base_cursor_color;
    state.base_selection_foreground = terminal->base_selection_fg;
    state.base_selection_background = terminal->base_selection_bg;
    return state;
}

static int *terminal_osc_color_slot(TerminalState *terminal, int target)
{
    if(terminal == NULL)
        return NULL;
    switch(target) {
    case TERMINAL_PANE_OSC_COLOR_FOREGROUND:
        return &terminal->default_fg;
    case TERMINAL_PANE_OSC_COLOR_BACKGROUND:
        return &terminal->default_bg;
    case TERMINAL_PANE_OSC_COLOR_CURSOR:
        return &terminal->cursor_color;
    case TERMINAL_PANE_OSC_COLOR_MOUSE_FOREGROUND:
        return &terminal->mouse_fg;
    case TERMINAL_PANE_OSC_COLOR_MOUSE_BACKGROUND:
        return &terminal->mouse_bg;
    case TERMINAL_PANE_OSC_COLOR_SELECTION_BACKGROUND:
        return &terminal->selection_bg;
    case TERMINAL_PANE_OSC_COLOR_SELECTION_FOREGROUND:
        return &terminal->selection_fg;
    default:
        break;
    }
    return NULL;
}

int terminal_send_title_report(TerminalState *terminal, int icon)
{
    char response[320];
    int len;

    if(terminal == NULL)
        return 0;
    len = FormatTerminalPaneOSCTitleReport(
        response, (int)sizeof(response), icon,
        icon ? terminal->icon_title : terminal->title);
    if(len <= 0 || len >= (int)sizeof(response))
        return 0;
    return terminal_write_text(terminal, response);
}

void terminal_push_title_targets(TerminalState *terminal, int target)
{
    if(terminal == NULL)
        return;
    if(target == 0 || target == 2)
        TerminalPaneOSCPushTitle(
            (char *)terminal->title_stack, TITLE_STACK_DEPTH,
            (int)sizeof(terminal->title_stack[0]),
            &terminal->title_stack_count, terminal->title);
    if(target == 0 || target == 1)
        TerminalPaneOSCPushTitle(
            (char *)terminal->icon_title_stack, TITLE_STACK_DEPTH,
            (int)sizeof(terminal->icon_title_stack[0]),
            &terminal->icon_title_stack_count, terminal->icon_title);
}

void terminal_pop_title_targets(TerminalState *terminal, int target)
{
    if(terminal == NULL)
        return;
    if(target == 0 || target == 2)
        TerminalPaneOSCPopTitle(
            (char *)terminal->title_stack, TITLE_STACK_DEPTH,
            (int)sizeof(terminal->title_stack[0]),
            &terminal->title_stack_count, terminal->title,
            (int)sizeof(terminal->title));
    if(target == 0 || target == 1)
        TerminalPaneOSCPopTitle(
            (char *)terminal->icon_title_stack, TITLE_STACK_DEPTH,
            (int)sizeof(terminal->icon_title_stack[0]),
            &terminal->icon_title_stack_count, terminal->icon_title,
            (int)sizeof(terminal->icon_title));
}

static void push_titles(TerminalState *terminal, const char *payload)
{
    int window;
    int icon;

    if(terminal == NULL)
        return;
    TerminalPaneOSCTitleTargets(payload, &window, &icon);
    if(window && icon)
        terminal_push_title_targets(terminal, 0);
    else if(icon)
        terminal_push_title_targets(terminal, 1);
    else if(window)
        terminal_push_title_targets(terminal, 2);
}

static void pop_titles(TerminalState *terminal, const char *payload)
{
    int window;
    int icon;

    if(terminal == NULL)
        return;
    TerminalPaneOSCTitleTargets(payload, &window, &icon);
    if(window && icon)
        terminal_pop_title_targets(terminal, 0);
    else if(icon)
        terminal_pop_title_targets(terminal, 1);
    else if(window)
        terminal_pop_title_targets(terminal, 2);
}

void terminal_finish_osc(TerminalState *terminal)
{
    const char *payload;
    int code;
    int color;

    if(terminal == NULL)
        return;
    terminal->osc[sizeof(terminal->osc) - 1] = '\0';
    if(!ParseTerminalPaneOSCCommand(terminal->osc, &code, &payload))
        return;
    if(code == 0 || code == 2) {
        (void)CopyTerminalPaneTitleText(
            terminal->title, (int)sizeof(terminal->title), payload);
        if(code == 2)
            return;
    }
    if(code == 0 || code == 1) {
        (void)CopyTerminalPaneTitleText(
            terminal->icon_title, (int)sizeof(terminal->icon_title),
            payload);
        return;
    }
    if(code == 7) {
        DecodeTerminalPaneOSCFileURIPath(
            terminal->current_directory,
            (int)sizeof(terminal->current_directory), payload);
        return;
    }
    if(code == 22) {
        push_titles(terminal, payload);
        return;
    }
    if(code == 23) {
        pop_titles(terminal, payload);
        return;
    }
    {
        int target = TerminalPaneOSCColorTargetForCode(code);
        int *slot = terminal_osc_color_slot(terminal, target);

        if(slot != NULL) {
            if(payload[0] == '?') {
                color = TerminalPaneOSCColorQueryValue(
                    target, terminal_osc_color_state(terminal));
                send_osc_color_response(terminal, code, color);
                return;
            }
            color = ParseTerminalPaneOSCColor(payload);
            if(color == COLOR_DEFAULT)
                return;
            *slot = color;
            return;
        }
    }
    {
        int target = TerminalPaneOSCColorTargetForResetCode(code);
        int *slot = terminal_osc_color_slot(terminal, target);

        if(slot != NULL) {
            *slot = COLOR_DEFAULT;
            return;
        }
    }
    if(code == 4) {
        const char *cursor = payload;
        TerminalPaneOSCPaletteEntry entry;

        while(NextTerminalPaneOSCPaletteEntry(&cursor, &entry)) {
            if(!entry.valid)
                continue;
            if(entry.query) {
                int value = terminal->palette_overrides[entry.index];

                if(value == COLOR_DEFAULT)
                    value = terminal_default_palette_color(entry.index);
                send_osc_palette_response(terminal, entry.index, value);
            } else {
                terminal->palette_overrides[entry.index] = entry.color;
            }
        }
        return;
    }
    if(code == 104) {
        const char *cursor = payload;
        int index;

        if(cursor[0] == '\0') {
            int i;

            for(i = 0; i < 256; i++)
                terminal->palette_overrides[i] = COLOR_DEFAULT;
            return;
        }
        while(NextTerminalPaneOSCPaletteResetIndex(&cursor, &index)) {
            if(index >= 0)
                terminal->palette_overrides[index] = COLOR_DEFAULT;
        }
        return;
    }
    if(code == 8) {
        char safe_id[HYPERLINK_ID_SIZE];
        char safe_url[HYPERLINK_SIZE];
        const char *url = strchr(payload, ';');

        if(url == NULL)
            return;
        url++;
        if(url[0] == '\0') {
            terminal->current_hyperlink = 0;
            return;
        }
        if(!CopyTerminalPaneOSCHyperlinkURL(safe_url,
                                            (int)sizeof(safe_url), url))
            return;
        (void)CopyTerminalPaneOSCHyperlinkID(safe_id, (int)sizeof(safe_id),
                                             payload);
        terminal->current_hyperlink =
            find_or_add_hyperlink(terminal, safe_id, safe_url);
        return;
    }
    if(code == 52)
        (void)HandleUIClipboardOSC52(&terminal->clipboard, payload,
                                     write_osc52_response, terminal);
}
