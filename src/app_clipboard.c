#include "app_clipboard.h"

#include "app_sessions.h"

#include <limits.h>
#include <stddef.h>

static TerminalPaneClipboardActions
clipboard_actions(State *app, Session *session)
{
    TerminalPaneClipboardActions actions = {0};
    TerminalPaneClipboardController selection = {0};
    TerminalPaneClipboardController terminal = {0};

    if(session == NULL)
        return actions;
    if(app != NULL)
        selection =
            selection_clipboard_controller(&app->selection, &session->terminal);
    terminal = MakeTerminalPaneClipboardController(
        terminal_clipboard(&session->terminal), NULL, NULL, NULL, NULL, 0, 0,
        &session->scroll_offset);
    return MakeTerminalPaneClipboardActions(selection, terminal);
}

static TerminalPaneClipboardActions active_clipboard_actions(State *app)
{
    if(app == NULL)
        return (TerminalPaneClipboardActions){0};
    return clipboard_actions(app, active_session(app));
}

int collect_selection_text(State *app, char *buffer, size_t buffer_size)
{
    if(buffer == NULL || buffer_size == 0)
        return 0;
    buffer[0] = '\0';
    if(buffer_size > (size_t)INT_MAX)
        buffer_size = (size_t)INT_MAX;
    return TerminalPaneClipboardCollectSelectionText(
        active_clipboard_actions(app), buffer, (int)buffer_size);
}

int primary_selection_available(void)
{
    return TerminalPaneClipboardPrimarySelectionAvailable();
}

void update_primary_selection(State *app)
{
    (void)TerminalPaneClipboardUpdatePrimary(active_clipboard_actions(app));
}

void copy_selection(State *app)
{
    (void)TerminalPaneClipboardCopy(active_clipboard_actions(app));
}

void paste_text_to_session(Session *session, const char *text)
{
    if(session == NULL || text == NULL || text[0] == '\0')
        return;
    (void)TerminalPaneClipboardPasteActionsText(
        clipboard_actions(NULL, session), text);
}

void paste_clipboard_to_session(Session *session)
{
    if(session == NULL)
        return;
    (void)TerminalPaneClipboardPasteActionsClipboard(
        clipboard_actions(NULL, session));
}

void paste_primary_to_session(State *app, Session *session)
{
    if(app == NULL || session == NULL)
        return;
    (void)TerminalPaneClipboardPasteActionsPrimary(
        clipboard_actions(app, session));
}

void paste_primary_or_clipboard_to_session(State *app, Session *session)
{
    if(app == NULL || session == NULL)
        return;
    (void)TerminalPaneClipboardPasteActionsPreferred(
        clipboard_actions(app, session));
}

void sync_host_clipboard_to_terminal(Session *session)
{
    if(session == NULL)
        return;
    (void)TerminalPaneClipboardActionsSyncFromHost(
        clipboard_actions(NULL, session));
}

void flush_terminal_clipboard_to_host(Session *session)
{
    if(session == NULL)
        return;
    (void)TerminalPaneClipboardActionsFlushToHost(
        clipboard_actions(NULL, session));
}

void select_all(State *app)
{
    (void)TerminalPaneClipboardSelectAll(active_clipboard_actions(app));
}
