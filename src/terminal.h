#ifndef KAPSULE_TERMINAL_H
#define KAPSULE_TERMINAL_H

#include <stddef.h>

#define MAX_COLS 240
#define MAX_ROWS 120
#define MAX_CSI_ARGS 16
#define MAX_HYPERLINKS 128
#define HYPERLINK_SIZE 512
#define SCROLLBACK_LIMIT 2000
#define MAX_SIXEL_IMAGES 128
#define MAX_SIXEL_IMAGE_PIXELS 4194304
#define MAX_SIXEL_TOTAL_PIXELS 8388608

#define COLOR_DEFAULT (-1)
#define COLOR_TRUE_RGB 0x01000000

#define STYLE_BOLD 1
#define STYLE_ITALIC 2
#define STYLE_UNDERLINE 4
#define STYLE_INVERSE 8
#define STYLE_STRIKE 16
#define STYLE_WIDE_CONT 32

#define MOD_SHIFT 1
#define MOD_ALT 2
#define MOD_CTRL 4

#define TERMINAL_MOUSE_LEFT 0
#define TERMINAL_MOUSE_MIDDLE 1
#define TERMINAL_MOUSE_RIGHT 2
#define TERMINAL_MOUSE_RELEASE 3
#define TERMINAL_MOUSE_WHEEL_UP 64
#define TERMINAL_MOUSE_WHEEL_DOWN 65

#define TERMINAL_CURSOR_DEFAULT 0
#define TERMINAL_CURSOR_BLOCK 1
#define TERMINAL_CURSOR_UNDERLINE 2
#define TERMINAL_CURSOR_BAR 3

typedef enum TerminalKey {
    KEY_ENTER_CODE = 1,
    KEY_BACKSPACE_CODE,
    KEY_TAB_CODE,
    KEY_ESCAPE_CODE,
    KEY_UP_CODE,
    KEY_DOWN_CODE,
    KEY_RIGHT_CODE,
    KEY_LEFT_CODE,
    KEY_HOME_CODE,
    KEY_END_CODE,
    KEY_PAGE_UP_CODE,
    KEY_PAGE_DOWN_CODE,
    KEY_DELETE_CODE,
    KEY_INSERT_CODE,
    KEY_F1_CODE,
    KEY_F2_CODE,
    KEY_F3_CODE,
    KEY_F4_CODE,
    KEY_F5_CODE,
    KEY_F6_CODE,
    KEY_F7_CODE,
    KEY_F8_CODE,
    KEY_F9_CODE,
    KEY_F10_CODE,
    KEY_F11_CODE,
    KEY_F12_CODE
} TerminalKey;

typedef struct Cell {
    unsigned int codepoint;
    unsigned int combining;
    int fg;
    int bg;
    int underline;
    int hyperlink;
    unsigned char style;
} Cell;

typedef struct SixelImage {
    int col;
    int row;
    int alternate_screen;
    int width;
    int height;
    int *pixels;
} SixelImage;

typedef struct TerminalState {
    int pid;
    int fd;
    int running;
    int cols;
    int rows;
    int cursor_col;
    int cursor_row;
    int saved_col;
    int saved_row;
    int saved_fg;
    int saved_bg;
    int saved_underline;
    int saved_style;
    int saved_hyperlink;
    int saved_g0_charset;
    int saved_g1_charset;
    int saved_active_charset;
    int saved_origin_mode;
    int saved_autowrap;
    int saved_insert_mode;
    int scroll_top;
    int scroll_bottom;
    int cursor_visible;
    int alternate_screen;
    int origin_mode;
    int autowrap;
    int application_cursor_keys;
    int application_keypad;
    int bracketed_paste;
    int insert_mode;
    int mouse_mode;
    int mouse_utf8;
    int mouse_sgr;
    int focus_reporting;
    int g0_charset;
    int g1_charset;
    int active_charset;
    int pending_charset;
    int parser_state;
    int csi_private;
    int csi_intermediate;
    int csi_args[MAX_CSI_ARGS];
    int csi_count;
    int current_fg;
    int current_bg;
    int current_underline;
    int current_style;
    int current_hyperlink;
    int utf8_codepoint;
    int utf8_remaining;
    Cell *main_cells;
    Cell *alt_cells;
    Cell *scrollback;
    unsigned char *main_wrapped;
    unsigned char *alt_wrapped;
    unsigned char *scrollback_wrapped;
    unsigned char *tab_stops;
    int scrollback_count;
    int scrollback_head;
    int scrollback_capacity;
    int scrollback_limit;
    int default_fg;
    int default_bg;
    int cursor_color;
    int base_fg;
    int base_bg;
    int base_cursor_color;
    int palette_overrides[256];
    int cursor_style;
    char hyperlinks[MAX_HYPERLINKS][HYPERLINK_SIZE];
    int hyperlink_count;
    char title[256];
    char current_directory[1024];
    char clipboard[4096];
    int clipboard_pending;
    char osc[512];
    int osc_len;
    char *dcs;
    int dcs_len;
    int dcs_capacity;
    int dcs_ignored;
    SixelImage *sixel_images;
    int sixel_count;
    int sixel_capacity;
    int sixel_total_pixels;
} TerminalState;

void terminal_init(TerminalState *terminal);
int terminal_open(TerminalState *terminal, const char *cwd, int cols, int rows);
int terminal_spawn(TerminalState *terminal, const char *cwd, const char *shell,
                   const char *command, int cols, int rows);
int terminal_write(TerminalState *terminal, const void *data, int size);
int terminal_write_text(TerminalState *terminal, const char *text);
int terminal_send_codepoint(TerminalState *terminal, unsigned int codepoint,
                            int mods);
int terminal_send_key(TerminalState *terminal, int key, int mods);
int terminal_send_keypad(TerminalState *terminal, char key);
int terminal_send_mouse(TerminalState *terminal, int button, int col, int row,
                        int pressed, int motion, int mods);
int terminal_send_focus(TerminalState *terminal, int focused);
int terminal_send_paste(TerminalState *terminal, const char *text);
void terminal_feed(TerminalState *terminal, const void *data, int size);
int terminal_poll(TerminalState *terminal);
void terminal_resize(TerminalState *terminal, int cols, int rows);
void terminal_set_scrollback_limit(TerminalState *terminal, int rows);
void terminal_close(TerminalState *terminal);
const Cell *terminal_cell(const TerminalState *terminal, int col, int row);
const Cell *terminal_visible_cell(const TerminalState *terminal, int col,
                                  int visible_row);
const char *terminal_hyperlink(const TerminalState *terminal, int hyperlink);
void terminal_line(const TerminalState *terminal, int row, char *out, int out_size);
int terminal_scrollback_rows(const TerminalState *terminal);
void terminal_scrollback_line(const TerminalState *terminal, int row, char *out,
                              int out_size);
int terminal_visible_line_count(const TerminalState *terminal);
void terminal_visible_line(const TerminalState *terminal, int visible_row, char *out,
                           int out_size);
int terminal_sixel_count(const TerminalState *terminal);
const SixelImage *terminal_sixel_image(const TerminalState *terminal, int index);

#endif
