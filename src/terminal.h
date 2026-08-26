#ifndef KTREM_TERMINAL_H
#define KTREM_TERMINAL_H

#include "terminal_pane.h"
#include "ui_tk.h"

#include <stddef.h>

#define MAX_COLS 240
#define MAX_ROWS 120
#define MAX_CSI_ARGS 16
#define MAX_HYPERLINKS 128
#define HYPERLINK_SIZE 512
#define HYPERLINK_ID_SIZE 128
#define TITLE_STACK_DEPTH 16
#define SCROLLBACK_LIMIT 2000
#define MAX_SIXEL_IMAGES 128
#define MAX_SIXEL_IMAGE_PIXELS 4194304
#define MAX_SIXEL_TOTAL_PIXELS 8388608

#define COLOR_DEFAULT TERMINAL_PANE_COLOR_DEFAULT
#define COLOR_TRUE_RGB TERMINAL_PANE_COLOR_TRUE_RGB

#define STYLE_BOLD 1
#define STYLE_ITALIC 2
#define STYLE_UNDERLINE 4
#define STYLE_INVERSE 8
#define STYLE_STRIKE 16
#define STYLE_WIDE_CONT 32
#define STYLE_FAINT 64
#define STYLE_CONCEAL 128
#define STYLE_BLINK 256
#define STYLE_OVERLINE 512

#define MOD_SHIFT TERMINAL_PANE_MOD_SHIFT
#define MOD_ALT TERMINAL_PANE_MOD_ALT
#define MOD_CTRL TERMINAL_PANE_MOD_CTRL

#define TERMINAL_MOUSE_LEFT TERMINAL_PANE_MOUSE_LEFT
#define TERMINAL_MOUSE_MIDDLE TERMINAL_PANE_MOUSE_MIDDLE
#define TERMINAL_MOUSE_RIGHT TERMINAL_PANE_MOUSE_RIGHT
#define TERMINAL_MOUSE_RELEASE TERMINAL_PANE_MOUSE_RELEASE
#define TERMINAL_MOUSE_WHEEL_UP TERMINAL_PANE_MOUSE_WHEEL_UP
#define TERMINAL_MOUSE_WHEEL_DOWN TERMINAL_PANE_MOUSE_WHEEL_DOWN

#define TERMINAL_CURSOR_DEFAULT TERMINAL_PANE_CURSOR_DEFAULT
#define TERMINAL_CURSOR_BLOCK TERMINAL_PANE_CURSOR_BLOCK
#define TERMINAL_CURSOR_UNDERLINE TERMINAL_PANE_CURSOR_UNDERLINE
#define TERMINAL_CURSOR_BAR TERMINAL_PANE_CURSOR_BAR

typedef enum TerminalKey {
    KEY_ENTER_CODE = 1,
    KEY_BACKSPACE_CODE = 2,
    KEY_TAB_CODE = 3,
    KEY_ESCAPE_CODE = 4,
    KEY_UP_CODE = 5,
    KEY_DOWN_CODE = 6,
    KEY_RIGHT_CODE = 7,
    KEY_LEFT_CODE = 8,
    KEY_HOME_CODE = 9,
    KEY_END_CODE = 10,
    KEY_PAGE_UP_CODE = 11,
    KEY_PAGE_DOWN_CODE = 12,
    KEY_DELETE_CODE = 13,
    KEY_INSERT_CODE = 14,
    KEY_F1_CODE = 15,
    KEY_F2_CODE = 16,
    KEY_F3_CODE = 17,
    KEY_F4_CODE = 18,
    KEY_F5_CODE = 19,
    KEY_F6_CODE = 20,
    KEY_F7_CODE = 21,
    KEY_F8_CODE = 22,
    KEY_F9_CODE = 23,
    KEY_F10_CODE = 24,
    KEY_F11_CODE = 25,
    KEY_F12_CODE = 26,
    KEY_F13_CODE = 27,
    KEY_F14_CODE = 28,
    KEY_F15_CODE = 29,
    KEY_F16_CODE = 30,
    KEY_F17_CODE = 31,
    KEY_F18_CODE = 32,
    KEY_F19_CODE = 33,
    KEY_F20_CODE = 34,
    KEY_F21_CODE = 35,
    KEY_F22_CODE = 36,
    KEY_F23_CODE = 37,
    KEY_F24_CODE = 38
} TerminalKey;

typedef struct Cell {
    unsigned int codepoint;
    unsigned int combining;
    int fg;
    int bg;
    int underline;
    int hyperlink;
    unsigned short style;
} Cell;

typedef struct SixelImage {
    int col;
    int row;
    int alternate_screen;
    int width;
    int height;
    int pixel_aspect_num;
    int pixel_aspect_den;
    int *pixels;
} SixelImage;

typedef struct TerminalSearchMatch {
    int row;
    int col;
    int length;
} TerminalSearchMatch;

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
    int cursor_blink;
    int alternate_screen;
    int origin_mode;
    int autowrap;
    int application_cursor_keys;
    int application_keypad;
    int bracketed_paste;
    int modify_other_keys;
    int insert_mode;
    int newline_mode;
    int mouse_mode;
    int mouse_utf8;
    int mouse_sgr;
    int mouse_urxvt;
    int mouse_pixels;
    int alternate_scroll;
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
    int mouse_fg;
    int mouse_bg;
    int selection_fg;
    int selection_bg;
    int base_fg;
    int base_bg;
    int base_cursor_color;
    int base_selection_fg;
    int base_selection_bg;
    int palette_overrides[256];
    int cursor_style;
    char hyperlinks[MAX_HYPERLINKS][HYPERLINK_SIZE];
    char hyperlink_ids[MAX_HYPERLINKS][HYPERLINK_ID_SIZE];
    int hyperlink_count;
    char title[256];
    char icon_title[256];
    char title_stack[TITLE_STACK_DEPTH][256];
    char icon_title_stack[TITLE_STACK_DEPTH][256];
    int title_stack_count;
    int icon_title_stack_count;
    char current_directory[1024];
    UIClipboardBuffer clipboard;
    int bell_pending;
    char osc[512];
    int osc_len;
    TerminalPaneDCSBuffer dcs;
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
int terminal_send_function_key(TerminalState *terminal, int function_index,
                               int mods);
int terminal_send_keypad(TerminalState *terminal, char key);
int terminal_send_mouse(TerminalState *terminal, int button, int col, int row,
                        int pressed, int motion, int mods);
int terminal_send_mouse_pixels(TerminalState *terminal, int button, int col,
                               int row, int pixel_x, int pixel_y, int pressed,
                               int motion, int mods);
int terminal_send_alternate_scroll(TerminalState *terminal, int direction,
                                   int mods);
int terminal_send_focus(TerminalState *terminal, int focused);
TerminalPaneClipboard terminal_clipboard(TerminalState *terminal);
void terminal_feed(TerminalState *terminal, const void *data, int size);
int terminal_poll(TerminalState *terminal);
int terminal_poll_bytes(TerminalState *terminal);
void terminal_resize(TerminalState *terminal, int cols, int rows);
void terminal_set_scrollback_limit(TerminalState *terminal, int rows);
void terminal_close(TerminalState *terminal);
const Cell *terminal_cell(const TerminalState *terminal, int col, int row);
const Cell *terminal_visible_cell(const TerminalState *terminal, int col,
                                  int visible_row);
const char *terminal_hyperlink(const TerminalState *terminal, int hyperlink);
const char *terminal_hyperlink_id(const TerminalState *terminal, int hyperlink);
void terminal_line(const TerminalState *terminal, int row, char *out, int out_size);
int terminal_scrollback_rows(const TerminalState *terminal);
void terminal_scrollback_line(const TerminalState *terminal, int row, char *out,
                              int out_size);
int terminal_visible_line_count(const TerminalState *terminal);
void terminal_visible_line(const TerminalState *terminal, int visible_row, char *out,
                           int out_size);
int terminal_visible_line_wrapped(const TerminalState *terminal, int visible_row);
int terminal_find_visible(const TerminalState *terminal, const char *needle,
                          int start_row, int start_col, int direction,
                          int wrap, TerminalSearchMatch *out);
TerminalPaneSearchController terminal_search_controller(
    const TerminalState *terminal, TerminalPaneSelection *selection,
    int visible_rows, int first_visible_row, int *scroll_offset);
int terminal_consume_bell(TerminalState *terminal);
int terminal_sixel_count(const TerminalState *terminal);
const SixelImage *terminal_sixel_image(const TerminalState *terminal, int index);

#endif
