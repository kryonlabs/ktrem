#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

#define MAX_COLS 240
#define MAX_ROWS 120
#define MAX_CSI_ARGS 16
#define SCROLLBACK_LIMIT 2000

#define COLOR_DEFAULT (-1)
#define COLOR_TRUE_RGB 0x01000000

#define STYLE_BOLD 1
#define STYLE_ITALIC 2
#define STYLE_UNDERLINE 4
#define STYLE_INVERSE 8
#define STYLE_STRIKE 16

#define MOD_SHIFT 1
#define MOD_ALT 2
#define MOD_CTRL 4

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
    KEY_INSERT_CODE
} TerminalKey;

typedef struct Cell {
    unsigned int codepoint;
    int fg;
    int bg;
    unsigned char style;
} Cell;

typedef struct Terminal {
    int pid;
    int fd;
    int running;
    int cols;
    int rows;
    int cursor_col;
    int cursor_row;
    int saved_col;
    int saved_row;
    int scroll_top;
    int scroll_bottom;
    int cursor_visible;
    int alternate_screen;
    int application_cursor_keys;
    int bracketed_paste;
    int mouse_mode;
    int mouse_sgr;
    int parser_state;
    int csi_private;
    int csi_intermediate;
    int csi_args[MAX_CSI_ARGS];
    int csi_count;
    int current_fg;
    int current_bg;
    int current_style;
    int utf8_codepoint;
    int utf8_remaining;
    Cell *main_cells;
    Cell *alt_cells;
    Cell *scrollback;
    unsigned char *tab_stops;
    int scrollback_count;
    int scrollback_head;
    int scrollback_capacity;
    int scrollback_limit;
    char title[256];
    int title_len;
} Terminal;

void terminal_init(Terminal *terminal);
int terminal_open(Terminal *terminal, const char *cwd, int cols, int rows);
int terminal_spawn(Terminal *terminal, const char *cwd, const char *shell,
                   const char *command, int cols, int rows);
int terminal_write(Terminal *terminal, const void *data, int size);
int terminal_write_text(Terminal *terminal, const char *text);
int terminal_send_key(Terminal *terminal, int key, int mods);
int terminal_send_paste(Terminal *terminal, const char *text);
void terminal_feed(Terminal *terminal, const void *data, int size);
int terminal_poll(Terminal *terminal);
void terminal_resize(Terminal *terminal, int cols, int rows);
void terminal_set_scrollback_limit(Terminal *terminal, int rows);
void terminal_close(Terminal *terminal);
const Cell *terminal_cell(const Terminal *terminal, int col, int row);
void terminal_line(const Terminal *terminal, int row, char *out, int out_size);
int terminal_scrollback_rows(const Terminal *terminal);
void terminal_scrollback_line(const Terminal *terminal, int row, char *out,
                              int out_size);
int terminal_visible_line_count(const Terminal *terminal);
void terminal_visible_line(const Terminal *terminal, int visible_row, char *out,
                           int out_size);

#endif
