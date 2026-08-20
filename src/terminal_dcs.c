#include "terminal_dcs.h"

#include "terminal_sixel.h"

#include <stdio.h>
#include <string.h>

void terminal_dcs_begin(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    ResetTerminalPaneDCSBuffer(&terminal->dcs);
}

void terminal_dcs_append(TerminalState *terminal, unsigned int codepoint)
{
    if(terminal == NULL)
        return;
    (void)AppendTerminalPaneDCSCodepoint(&terminal->dcs, codepoint);
}

int terminal_dcs_finish(TerminalState *terminal)
{
    int rows_used = 0;
    const char *payload;

    if(terminal == NULL)
        return 0;
    payload = GetTerminalPaneDCSBufferText(&terminal->dcs);
    if(payload != NULL) {
        if(!terminal_dcs_finish_decrqss(terminal, payload) &&
           !terminal_dcs_finish_xtgettcap(terminal, payload) &&
           strchr(payload, 'q') != NULL)
            rows_used = terminal_sixel_finish(terminal, payload);
    }
    ResetTerminalPaneDCSBuffer(&terminal->dcs);
    return rows_used;
}

int terminal_dcs_finish_xtgettcap(TerminalState *terminal,
                                  const char *payload)
{
    char response[1024];
    int len;

    if(terminal == NULL || payload == NULL || strncmp(payload, "+q", 2) != 0)
        return 0;
    len = FormatTerminalPaneXTGETTCAPResponse(response,
                                              (int)sizeof(response),
                                              payload);
    if(len <= 0)
        return 1;
    terminal_write_text(terminal, response);
    return 1;
}

static void current_sgr_status(const TerminalState *terminal, char *buffer,
                               int buffer_size)
{
    TerminalPaneSGRStatus status;

    if(buffer == NULL || buffer_size <= 0)
        return;
    buffer[0] = '\0';
    if(terminal == NULL)
        return;
    status = (TerminalPaneSGRStatus){
        terminal->current_style &
            (STYLE_BOLD | STYLE_ITALIC | STYLE_UNDERLINE | STYLE_INVERSE |
             STYLE_STRIKE | STYLE_FAINT | STYLE_CONCEAL | STYLE_BLINK |
             STYLE_OVERLINE),
        terminal->current_fg,
        terminal->current_bg,
        terminal->current_underline,
    };
    (void)FormatTerminalPaneSGRStatus(buffer, buffer_size, status);
}

int terminal_dcs_finish_decrqss(TerminalState *terminal,
                                const char *payload)
{
    char status[256];
    char response[320];
    const char *selector;

    if(terminal == NULL || payload == NULL || strncmp(payload, "$q", 2) != 0)
        return 0;
    selector = payload + 2;
    status[0] = '\0';
    if(strcmp(selector, "m") == 0) {
        current_sgr_status(terminal, status, sizeof(status));
    } else if(strcmp(selector, "r") == 0) {
        snprintf(status, sizeof(status), "%d;%dr",
                 terminal->scroll_top + 1, terminal->scroll_bottom + 1);
    } else if(strcmp(selector, " q") == 0) {
        snprintf(status, sizeof(status), "%d q",
                 TerminalPaneCursorStyleReportCode(terminal->cursor_style,
                                                   terminal->cursor_blink));
    } else if(strcmp(selector, "\" q") == 0) {
        snprintf(status, sizeof(status), "0\"q");
    } else {
        terminal_write_text(terminal, "\x1bP0$r\x1b\\");
        return 1;
    }
    snprintf(response, sizeof(response), "\x1bP1$r%s\x1b\\", status);
    terminal_write_text(terminal, response);
    return 1;
}
