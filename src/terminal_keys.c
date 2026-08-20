#include "terminal.h"

static TerminalPaneKeyMode terminal_key_mode(const TerminalState *terminal)
{
    if(terminal == NULL)
        return (TerminalPaneKeyMode){0};
    return (TerminalPaneKeyMode){
        terminal->application_cursor_keys,
        terminal->application_keypad,
        terminal->modify_other_keys,
    };
}

static int terminal_write_encoded(TerminalState *terminal, const char *seq,
                                  int len)
{
    if(terminal == NULL || seq == NULL || len <= 0)
        return 0;
    return terminal_write(terminal, seq, len);
}

int terminal_send_codepoint(TerminalState *terminal, unsigned int codepoint,
                            int mods)
{
    char seq[64];
    int len;

    if(terminal == NULL)
        return 0;
    len = EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), codepoint, mods,
                                      terminal_key_mode(terminal));
    return terminal_write_encoded(terminal, seq, len);
}

int terminal_send_key(TerminalState *terminal, int key, int mods)
{
    char seq[64];
    int len;

    if(terminal == NULL)
        return 0;
    len = EncodeTerminalPaneKey(seq, (int)sizeof(seq), key, mods,
                                terminal_key_mode(terminal));
    return terminal_write_encoded(terminal, seq, len);
}

int terminal_send_function_key(TerminalState *terminal, int function_index,
                               int mods)
{
    TerminalPaneMappedKey mapped;

    if(terminal == NULL)
        return 0;
    mapped = MapTerminalPaneFunctionKey(function_index, mods);
    if(mapped.key == 0)
        return 0;
    return terminal_send_key(terminal, mapped.key, mapped.mods);
}

int terminal_send_keypad(TerminalState *terminal, char key)
{
    char seq[16];
    int len;

    if(terminal == NULL)
        return 0;
    len = EncodeTerminalPaneKeypad(seq, (int)sizeof(seq), key,
                                   terminal_key_mode(terminal));
    return terminal_write_encoded(terminal, seq, len);
}
