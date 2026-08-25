#include "input.h"

static TerminalPaneInputState keyboard_state = {0};

typedef struct InputTarget {
    TerminalState *terminal;
    InputKeyFilter key_filter;
    InputWriteFilter filter;
    void *userdata;
} InputTarget;

static TerminalPaneKeyMode input_key_mode(const TerminalState *terminal)
{
    TerminalPaneKeyMode mode = {0};

    if(terminal == NULL)
        return mode;
    mode.application_cursor_keys = terminal->application_cursor_keys;
    mode.application_keypad = terminal->application_keypad;
    mode.modify_other_keys = terminal->modify_other_keys;
    return mode;
}

static int input_write_terminal(void *userdata, const char *text, int length)
{
    InputTarget *target = userdata;

    if(target == NULL || target->terminal == NULL || text == NULL ||
       length <= 0)
        return 0;
    if(target->filter != NULL &&
       target->filter(target->userdata, text, length))
        return 1;
    return terminal_write(target->terminal, text, length);
}

static int input_filter_key(void *userdata, int platform_key, int mods)
{
    InputTarget *target = userdata;

    if(target == NULL || target->key_filter == NULL)
        return 0;
    return target->key_filter(target->userdata, platform_key, mods);
}

static TerminalPaneInput input_target(InputTarget *target)
{
    TerminalState *terminal = target != NULL ? target->terminal : NULL;

    return MakeTerminalPaneInput(input_key_mode(terminal), input_write_terminal,
                                 target, &keyboard_state);
}

int input_mods(void)
{
    return GetTerminalPaneInputModifiers();
}

int input_send_control_key(TerminalState *terminal, int key, int mods)
{
    if(terminal == NULL)
        return 0;
    {
        InputTarget target = {terminal, NULL, NULL, NULL};

        return SendTerminalPaneControlInput(input_target(&target), key, mods);
    }
}

void input_send_keyboard(TerminalState *terminal)
{
    input_send_keyboard_filtered(terminal, NULL, NULL, NULL);
}

void input_send_keyboard_filtered(TerminalState *terminal,
                                  InputKeyFilter key_filter,
                                  InputWriteFilter filter, void *userdata)
{
    if(terminal == NULL)
        return;
    {
        InputTarget target = {terminal, key_filter, filter, userdata};

        (void)PumpTerminalPaneKeyboardInputFiltered(input_target(&target),
                                                   input_filter_key, &target);
    }
}
