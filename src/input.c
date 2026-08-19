#include "input.h"

#include "kryon.h"

static int key_pressed_or_repeat(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

int input_mods(void)
{
    int mods = 0;

    if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        mods |= MOD_SHIFT;
    if(IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
        mods |= MOD_ALT;
    if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        mods |= MOD_CTRL;
    return mods;
}

void input_send_keyboard(Terminal *terminal)
{
    int mods = input_mods();
    int control_wrote = 0;
    int ch;

    if(terminal == NULL)
        return;
    if(key_pressed_or_repeat(KEY_ENTER) || key_pressed_or_repeat(KEY_KP_ENTER))
        terminal_send_key(terminal, KEY_ENTER_CODE, mods);
    if(key_pressed_or_repeat(KEY_BACKSPACE))
        terminal_send_key(terminal, KEY_BACKSPACE_CODE, mods);
    if(key_pressed_or_repeat(KEY_TAB))
        terminal_send_key(terminal, KEY_TAB_CODE, mods);
    if(key_pressed_or_repeat(KEY_ESCAPE))
        terminal_send_key(terminal, KEY_ESCAPE_CODE, mods);
    if(key_pressed_or_repeat(KEY_UP))
        terminal_send_key(terminal, KEY_UP_CODE, mods);
    if(key_pressed_or_repeat(KEY_DOWN))
        terminal_send_key(terminal, KEY_DOWN_CODE, mods);
    if(key_pressed_or_repeat(KEY_RIGHT))
        terminal_send_key(terminal, KEY_RIGHT_CODE, mods);
    if(key_pressed_or_repeat(KEY_LEFT))
        terminal_send_key(terminal, KEY_LEFT_CODE, mods);
    if(key_pressed_or_repeat(KEY_HOME))
        terminal_send_key(terminal, KEY_HOME_CODE, mods);
    if(key_pressed_or_repeat(KEY_END))
        terminal_send_key(terminal, KEY_END_CODE, mods);
    if(key_pressed_or_repeat(KEY_PAGE_UP))
        terminal_send_key(terminal, KEY_PAGE_UP_CODE, mods);
    if(key_pressed_or_repeat(KEY_PAGE_DOWN))
        terminal_send_key(terminal, KEY_PAGE_DOWN_CODE, mods);
    if(key_pressed_or_repeat(KEY_DELETE))
        terminal_send_key(terminal, KEY_DELETE_CODE, mods);
    if(key_pressed_or_repeat(KEY_INSERT))
        terminal_send_key(terminal, KEY_INSERT_CODE, mods);

    if((mods & MOD_CTRL) != 0 && (mods & MOD_SHIFT) == 0) {
        if(IsKeyPressed(KEY_C)) {
            terminal_write_text(terminal, "\x03");
            control_wrote = 1;
        }
        if(IsKeyPressed(KEY_D)) {
            terminal_write_text(terminal, "\x04");
            control_wrote = 1;
        }
        if(IsKeyPressed(KEY_L)) {
            terminal_write_text(terminal, "\x0c");
            control_wrote = 1;
        }
        if(control_wrote)
            return;
    }

    ch = GetCharPressed();
    while(ch > 0) {
        char text[8];
        int len = 0;

        if(ch < 0x80) {
            text[len++] = (char)ch;
        } else if(ch < 0x800) {
            text[len++] = (char)(0xc0 | (ch >> 6));
            text[len++] = (char)(0x80 | (ch & 0x3f));
        } else if(ch < 0x10000) {
            text[len++] = (char)(0xe0 | (ch >> 12));
            text[len++] = (char)(0x80 | ((ch >> 6) & 0x3f));
            text[len++] = (char)(0x80 | (ch & 0x3f));
        } else {
            text[len++] = (char)(0xf0 | (ch >> 18));
            text[len++] = (char)(0x80 | ((ch >> 12) & 0x3f));
            text[len++] = (char)(0x80 | ((ch >> 6) & 0x3f));
            text[len++] = (char)(0x80 | (ch & 0x3f));
        }
        terminal_write(terminal, text, len);
        ch = GetCharPressed();
    }
}
