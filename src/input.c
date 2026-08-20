#include "input.h"

static TerminalPaneInputState keyboard_state = {0};

static TerminalPaneKeyMode input_key_mode(const TerminalState *terminal)
{
    if(terminal == NULL)
        return (TerminalPaneKeyMode){0};
    return (TerminalPaneKeyMode){
        terminal->application_cursor_keys,
        terminal->application_keypad,
        terminal->modify_other_keys,
    };
}

static int input_write_terminal(void *userdata, const char *text, int length)
{
    TerminalState *terminal = userdata;

    if(terminal == NULL || text == NULL || length <= 0)
        return 0;
    return terminal_write(terminal, text, length);
}

static TerminalPaneInput input_target(TerminalState *terminal)
{
    return MakeTerminalPaneInput(input_key_mode(terminal), input_write_terminal,
                                 terminal, &keyboard_state);
}

int input_mods(void)
{
    return GetTerminalPaneInputModifiers();
}

int input_send_control_key(TerminalState *terminal, int key, int mods)
{
    if(terminal == NULL)
        return 0;
    return SendTerminalPaneControlInput(input_target(terminal), key, mods);
}

void input_send_keyboard(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    (void)PumpTerminalPaneKeyboardInput(input_target(terminal));
}
