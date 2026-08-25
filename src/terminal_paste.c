#include "terminal.h"

static int write_paste_chunk(void *userdata, const char *text, int size)
{
    TerminalState *terminal = userdata;

    if(terminal == NULL || text == NULL || size <= 0)
        return 0;
    return terminal_write(terminal, text, size);
}

TerminalPaneClipboard terminal_clipboard(TerminalState *terminal)
{
    TerminalPaneClipboard clipboard = {0};

    if(terminal == NULL)
        return clipboard;
    return MakeTerminalPaneClipboard(&terminal->clipboard,
                                     terminal->bracketed_paste,
                                     write_paste_chunk, terminal);
}
