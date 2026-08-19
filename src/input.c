#include "input.h"

#include "kryon.h"

#include <string.h>

#define KEY_QUEUE_SIZE 512

static int key_pressed_or_repeat(int key, const int *queued)
{
    if(key > 0 && key < KEY_QUEUE_SIZE && queued != NULL && queued[key])
        return 1;
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static int key_down_or_queued(int key, const int *queued)
{
    return IsKeyDown(key) ||
           (key > 0 && key < KEY_QUEUE_SIZE && queued != NULL && queued[key]);
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

void input_send_keyboard(TerminalState *terminal)
{
    static int last_control_keys[KEY_QUEUE_SIZE] = {0};
    static const struct {
        int key;
        unsigned char code;
    } control_map[] = {
        {KEY_SPACE, 0x00},
        {KEY_TWO, 0x00},
        {KEY_A, 0x01},
        {KEY_B, 0x02},
        {KEY_C, 0x03},
        {KEY_D, 0x04},
        {KEY_E, 0x05},
        {KEY_F, 0x06},
        {KEY_G, 0x07},
        {KEY_H, 0x08},
        {KEY_I, 0x09},
        {KEY_J, 0x0a},
        {KEY_K, 0x0b},
        {KEY_L, 0x0c},
        {KEY_M, 0x0d},
        {KEY_N, 0x0e},
        {KEY_O, 0x0f},
        {KEY_P, 0x10},
        {KEY_Q, 0x11},
        {KEY_R, 0x12},
        {KEY_S, 0x13},
        {KEY_T, 0x14},
        {KEY_U, 0x15},
        {KEY_V, 0x16},
        {KEY_W, 0x17},
        {KEY_X, 0x18},
        {KEY_Y, 0x19},
        {KEY_Z, 0x1a},
        {KEY_LEFT_BRACKET, 0x1b},
        {KEY_THREE, 0x1b},
        {KEY_BACKSLASH, 0x1c},
        {KEY_FOUR, 0x1c},
        {KEY_RIGHT_BRACKET, 0x1d},
        {KEY_FIVE, 0x1d},
        {KEY_SIX, 0x1e},
        {KEY_SEVEN, 0x1f},
        {KEY_EIGHT, 0x7f}
    };
    int queued[KEY_QUEUE_SIZE] = {0};
    int mods = input_mods();
    int control_wrote = 0;
    int keypad_wrote = 0;
    int ch;
    int key;
    int i;

    if(terminal == NULL)
        return;
    key = GetKeyPressed();
    while(key > 0) {
        if(key < KEY_QUEUE_SIZE)
            queued[key] = 1;
        key = GetKeyPressed();
    }

    if(key_pressed_or_repeat(KEY_ENTER, queued) ||
       (!terminal->application_keypad &&
        key_pressed_or_repeat(KEY_KP_ENTER, queued)))
        terminal_send_key(terminal, KEY_ENTER_CODE, mods);
    if(key_pressed_or_repeat(KEY_BACKSPACE, queued))
        terminal_send_key(terminal, KEY_BACKSPACE_CODE, mods);
    if(key_pressed_or_repeat(KEY_TAB, queued))
        terminal_send_key(terminal, KEY_TAB_CODE, mods);
    if(key_pressed_or_repeat(KEY_ESCAPE, queued))
        terminal_send_key(terminal, KEY_ESCAPE_CODE, mods);
    if(key_pressed_or_repeat(KEY_UP, queued))
        terminal_send_key(terminal, KEY_UP_CODE, mods);
    if(key_pressed_or_repeat(KEY_DOWN, queued))
        terminal_send_key(terminal, KEY_DOWN_CODE, mods);
    if(key_pressed_or_repeat(KEY_RIGHT, queued))
        terminal_send_key(terminal, KEY_RIGHT_CODE, mods);
    if(key_pressed_or_repeat(KEY_LEFT, queued))
        terminal_send_key(terminal, KEY_LEFT_CODE, mods);
    if(key_pressed_or_repeat(KEY_HOME, queued))
        terminal_send_key(terminal, KEY_HOME_CODE, mods);
    if(key_pressed_or_repeat(KEY_END, queued))
        terminal_send_key(terminal, KEY_END_CODE, mods);
    if(key_pressed_or_repeat(KEY_PAGE_UP, queued))
        terminal_send_key(terminal, KEY_PAGE_UP_CODE, mods);
    if(key_pressed_or_repeat(KEY_PAGE_DOWN, queued))
        terminal_send_key(terminal, KEY_PAGE_DOWN_CODE, mods);
    if(key_pressed_or_repeat(KEY_DELETE, queued))
        terminal_send_key(terminal, KEY_DELETE_CODE, mods);
    if(key_pressed_or_repeat(KEY_INSERT, queued))
        terminal_send_key(terminal, KEY_INSERT_CODE, mods);
    if(key_pressed_or_repeat(KEY_F1, queued))
        terminal_send_key(terminal, KEY_F1_CODE, mods);
    if(key_pressed_or_repeat(KEY_F2, queued))
        terminal_send_key(terminal, KEY_F2_CODE, mods);
    if(key_pressed_or_repeat(KEY_F3, queued))
        terminal_send_key(terminal, KEY_F3_CODE, mods);
    if(key_pressed_or_repeat(KEY_F4, queued))
        terminal_send_key(terminal, KEY_F4_CODE, mods);
    if(key_pressed_or_repeat(KEY_F5, queued))
        terminal_send_key(terminal, KEY_F5_CODE, mods);
    if(key_pressed_or_repeat(KEY_F6, queued))
        terminal_send_key(terminal, KEY_F6_CODE, mods);
    if(key_pressed_or_repeat(KEY_F7, queued))
        terminal_send_key(terminal, KEY_F7_CODE, mods);
    if(key_pressed_or_repeat(KEY_F8, queued))
        terminal_send_key(terminal, KEY_F8_CODE, mods);
    if(key_pressed_or_repeat(KEY_F9, queued))
        terminal_send_key(terminal, KEY_F9_CODE, mods);
    if(key_pressed_or_repeat(KEY_F10, queued))
        terminal_send_key(terminal, KEY_F10_CODE, mods);
    if(key_pressed_or_repeat(KEY_F11, queued))
        terminal_send_key(terminal, KEY_F11_CODE, mods);
    if(key_pressed_or_repeat(KEY_F12, queued))
        terminal_send_key(terminal, KEY_F12_CODE, mods);

    if(terminal->application_keypad) {
        static const struct {
            int key;
            char text;
        } keypad_map[] = {
            {KEY_KP_0, '0'},
            {KEY_KP_1, '1'},
            {KEY_KP_2, '2'},
            {KEY_KP_3, '3'},
            {KEY_KP_4, '4'},
            {KEY_KP_5, '5'},
            {KEY_KP_6, '6'},
            {KEY_KP_7, '7'},
            {KEY_KP_8, '8'},
            {KEY_KP_9, '9'},
            {KEY_KP_DECIMAL, '.'},
            {KEY_KP_DIVIDE, '/'},
            {KEY_KP_MULTIPLY, '*'},
            {KEY_KP_SUBTRACT, '-'},
            {KEY_KP_ADD, '+'},
            {KEY_KP_EQUAL, '='},
            {KEY_KP_ENTER, '\r'}
        };

        for(i = 0; i < (int)(sizeof(keypad_map) / sizeof(keypad_map[0]));
            i++) {
            if(key_pressed_or_repeat(keypad_map[i].key, queued)) {
                terminal_send_keypad(terminal, keypad_map[i].text);
                keypad_wrote = 1;
            }
        }
        if(keypad_wrote)
            return;
    }

    if((mods & MOD_CTRL) != 0 && (mods & MOD_SHIFT) == 0) {
        for(i = 0; i < (int)(sizeof(control_map) / sizeof(control_map[0]));
            i++) {
            int control_key = control_map[i].key;
            int down = key_down_or_queued(control_key, queued);

            if(down && !last_control_keys[control_key]) {
                char text[2];
                int len = 0;

                if((mods & MOD_ALT) != 0)
                    text[len++] = '\x1b';
                text[len++] = (char)control_map[i].code;
                terminal_write(terminal, text, len);
                control_wrote = 1;
            }
            last_control_keys[control_key] = down;
        }
        if(control_wrote)
            return;
    } else {
        memset(last_control_keys, 0, sizeof(last_control_keys));
    }

    ch = GetCharPressed();
    while(ch > 0) {
        terminal_send_codepoint(terminal, (unsigned int)ch, mods);
        ch = GetCharPressed();
    }
}
