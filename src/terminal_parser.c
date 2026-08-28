#include "terminal.h"

#include "terminal_csi.h"
#include "terminal_dcs.h"
#include "terminal_modes.h"
#include "terminal_osc.h"
#include "terminal_sgr.h"
#include "terminal_screen.h"
#include "terminal_text.h"

#include <stdio.h>

enum {
    STATE_TEXT,
    STATE_ESCAPE,
    STATE_CSI,
    STATE_OSC,
    STATE_OSC_ESCAPE,
    STATE_DCS,
    STATE_DCS_ESCAPE,
    STATE_CHARSET,
    STATE_HASH,
    STATE_IGNORE_STRING,
    STATE_IGNORE_ESCAPE
};

static void line_feed(TerminalState *terminal)
{
    if(terminal->cursor_row == terminal->scroll_bottom)
        terminal_scroll_up(terminal, terminal->scroll_top,
                           terminal->scroll_bottom, 1);
    else
        terminal->cursor_row++;
    terminal_clamp_cursor(terminal);
}

static void newline_control(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    line_feed(terminal);
    if(terminal->newline_mode)
        terminal->cursor_col = 0;
}

static void put_codepoint(TerminalState *terminal, unsigned int codepoint)
{
    Cell *cells = terminal_screen_cells(terminal);
    Cell cell;
    int width;
    int target_col;
    int target_row;

    if(terminal == NULL || cells == NULL)
        return;
    codepoint = terminal_translate_charset(terminal, codepoint);
    width = terminal_codepoint_width_for(terminal, codepoint);
    if(width <= 0) {
        if(codepoint < 0x300)
            return;
        target_col = terminal->cursor_col - 1;
        target_row = terminal->cursor_row;
        if(target_col < 0) {
            target_row--;
            target_col = terminal->cols - 1;
        }
        if(target_row < 0 || target_col < 0)
            return;
        cell = cells[target_row * terminal->cols + target_col];
        if((cell.style & STYLE_WIDE_CONT) != 0 && target_col > 0)
            target_col--;
        if(cells[target_row * terminal->cols + target_col].codepoint != 0 &&
           (cells[target_row * terminal->cols + target_col].style &
            STYLE_WIDE_CONT) == 0)
            cells[target_row * terminal->cols + target_col].combining =
                codepoint;
        return;
    }
    if(terminal->cursor_col >= terminal->cols ||
       terminal->cursor_col + width > terminal->cols) {
        unsigned char *wrapped = terminal_screen_wrapped(terminal);

        if(terminal->autowrap) {
            if(wrapped != NULL && terminal->cursor_row >= 0 &&
               terminal->cursor_row < terminal->rows)
                wrapped[terminal->cursor_row] = 1;
            terminal->cursor_col = 0;
            line_feed(terminal);
        } else {
            terminal->cursor_col = terminal->cols - width;
            if(terminal->cursor_col < 0)
                terminal->cursor_col = 0;
        }
    }
    terminal_clamp_cursor(terminal);
    if(terminal->insert_mode)
        terminal_insert_blank_chars(terminal, width);
    cell.codepoint = codepoint >= 32 ? codepoint : '?';
    cell.combining = 0;
    cell.fg = terminal->current_fg;
    cell.bg = terminal->current_bg;
    cell.underline = terminal->current_underline;
    cell.hyperlink = terminal->current_hyperlink;
    cell.style = (unsigned short)terminal->current_style;
    cells[terminal->cursor_row * terminal->cols + terminal->cursor_col] = cell;
    if(width == 2 && terminal->cursor_col + 1 < terminal->cols) {
        Cell cont = terminal_blank_cell(terminal);

        cont.codepoint = 0;
        cont.style = (unsigned short)(cont.style | STYLE_WIDE_CONT);
        cells[terminal->cursor_row * terminal->cols + terminal->cursor_col + 1] =
            cont;
    }
    terminal->cursor_col += width;
}

static void repeat_previous_graphic(TerminalState *terminal, int count)
{
    Cell *cells;
    Cell source;
    int source_col;
    int source_row;
    int old_fg;
    int old_bg;
    int old_underline;
    int old_style;
    int old_hyperlink;
    int i;

    if(terminal == NULL || count <= 0)
        return;
    cells = terminal_screen_cells(terminal);
    if(cells == NULL || terminal->cols <= 0 || terminal->rows <= 0)
        return;
    source_row = terminal->cursor_row;
    source_col = terminal->cursor_col - 1;
    if(source_col >= terminal->cols)
        source_col = terminal->cols - 1;
    if(source_col < 0 || source_row < 0 || source_row >= terminal->rows)
        return;
    if((cells[source_row * terminal->cols + source_col].style &
        STYLE_WIDE_CONT) != 0 &&
       source_col > 0)
        source_col--;
    source = cells[source_row * terminal->cols + source_col];
    if(source.codepoint == 0 || (source.style & STYLE_WIDE_CONT) != 0)
        return;

    old_fg = terminal->current_fg;
    old_bg = terminal->current_bg;
    old_underline = terminal->current_underline;
    old_style = terminal->current_style;
    old_hyperlink = terminal->current_hyperlink;
    terminal->current_fg = source.fg;
    terminal->current_bg = source.bg;
    terminal->current_underline = source.underline;
    terminal->current_style = source.style & ~STYLE_WIDE_CONT;
    terminal->current_hyperlink = source.hyperlink;
    for(i = 0; i < count; i++) {
        int col = terminal->cursor_col;
        int row = terminal->cursor_row;

        put_codepoint(terminal, source.codepoint);
        if(source.combining != 0 && row >= 0 && row < terminal->rows &&
           col >= 0 && col < terminal->cols)
            cells[row * terminal->cols + col].combining = source.combining;
    }
    terminal->current_fg = old_fg;
    terminal->current_bg = old_bg;
    terminal->current_underline = old_underline;
    terminal->current_style = old_style;
    terminal->current_hyperlink = old_hyperlink;
}

static void apply_csi(TerminalState *terminal, int final)
{
    int n = terminal_csi_arg(terminal, 0, 1);
    int at;
    int i;

    if(final == 'A')
        terminal->cursor_row -= n;
    else if(final == 'B')
        terminal->cursor_row += n;
    else if(final == 'C')
        terminal->cursor_col += n;
    else if(final == 'D')
        terminal->cursor_col -= n;
    else if(final == 'E') {
        terminal->cursor_row += n;
        terminal->cursor_col = 0;
    } else if(final == 'F') {
        terminal->cursor_row -= n;
        terminal->cursor_col = 0;
    }
    else if(final == 'G' || final == '`')
        terminal->cursor_col = n - 1;
    else if(final == 'd')
        terminal->cursor_row = terminal_csi_cursor_row_from_arg(terminal, 0, 1);
    else if(final == 'H' || final == 'f') {
        terminal->cursor_row = terminal_csi_cursor_row_from_arg(terminal, 0, 1);
        terminal->cursor_col = terminal_csi_arg(terminal, 1, 1) - 1;
    } else if(final == '@') {
        terminal_insert_blank_chars(terminal, n);
    } else if(final == 'J') {
        int mode = terminal_csi_arg(terminal, 0, 0);

        at = terminal->cursor_row * terminal->cols + terminal->cursor_col;
        if(mode == 2)
            terminal_clear_screen(terminal);
        else if(mode == 3)
            terminal_clear_scrollback(terminal);
        else if(mode == 0)
            terminal_erase_range(terminal, at, terminal->cols * terminal->rows);
        else if(mode == 1)
            terminal_erase_range(terminal, 0, at + 1);
    } else if(final == 'K') {
        int mode = terminal_csi_arg(terminal, 0, 0);
        int start = terminal->cursor_row * terminal->cols + terminal->cursor_col;
        int end = (terminal->cursor_row + 1) * terminal->cols;

        if(mode == 1) {
            start = terminal->cursor_row * terminal->cols;
            end = terminal->cursor_row * terminal->cols + terminal->cursor_col + 1;
        } else if(mode == 2) {
            start = terminal->cursor_row * terminal->cols;
        }
        terminal_erase_range(terminal, start, end);
    } else if(final == 'L') {
        terminal_scroll_down(terminal, terminal->cursor_row,
                             terminal->scroll_bottom, n);
    } else if(final == 'M') {
        terminal_scroll_up(terminal, terminal->cursor_row,
                           terminal->scroll_bottom, n);
    } else if(final == 'P') {
        terminal_delete_chars(terminal, n);
    } else if(final == 'X') {
        terminal_erase_chars(terminal, n);
    } else if(final == 'a') {
        terminal->cursor_col += n;
    } else if(final == 'e') {
        terminal->cursor_row += n;
    } else if(final == 'b') {
        repeat_previous_graphic(terminal, n);
    } else if(final == 'I') {
        for(i = 0; i < n; i++)
            terminal->cursor_col = terminal_next_tab_stop(terminal);
    } else if(final == 'Z') {
        int moved = 0;
        int col;

        for(col = terminal->cursor_col - 1; col >= 0 && moved < n; col--) {
            if(terminal->tab_stops != NULL && terminal->tab_stops[col]) {
                terminal->cursor_col = col;
                moved++;
            }
        }
        if(moved < n)
            terminal->cursor_col = 0;
    } else if(final == 'm' && terminal->csi_private == '>') {
        terminal_csi_set_modifier_key_mode(terminal);
    } else if(final == 'm') {
        terminal_sgr_apply(terminal);
    } else if(final == 'r') {
        int top = terminal_csi_arg(terminal, 0, 1) - 1;
        int bottom = terminal_csi_arg(terminal, 1, terminal->rows) - 1;

        if(top < bottom) {
            terminal->scroll_top = terminal_clamp_int(top, 0,
                                                      terminal->rows - 1);
            terminal->scroll_bottom = terminal_clamp_int(bottom, 0,
                                                         terminal->rows - 1);
            terminal->cursor_col = 0;
            terminal->cursor_row =
                terminal->origin_mode ? terminal->scroll_top : 0;
        }
    } else if(final == 'S') {
        terminal_scroll_up(terminal, terminal->scroll_top,
                           terminal->scroll_bottom, n);
    } else if(final == 'T') {
        terminal_scroll_down(terminal, terminal->scroll_top,
                             terminal->scroll_bottom, n);
    } else if(final == 's') {
        terminal_save_cursor_state(terminal);
    } else if(final == 'u') {
        terminal_restore_cursor_state(terminal);
    } else if(final == 'g') {
        int mode = terminal_csi_arg(terminal, 0, 0);

        if(mode == 0 && terminal->tab_stops != NULL)
            terminal->tab_stops[terminal->cursor_col] = 0;
        else if(mode == 3)
            terminal_clear_tab_stops(terminal);
    } else if(final == 'q' && terminal->csi_intermediate == ' ') {
        terminal_csi_apply_cursor_style(terminal);
    } else if((final == 'h' || final == 'l') && terminal->csi_private == '?') {
        for(i = 0; i < terminal->csi_count; i++)
            terminal_set_private_mode(terminal, terminal->csi_args[i],
                                      final == 'h');
    } else if(final == 'h' || final == 'l') {
        for(i = 0; i < terminal->csi_count; i++)
            terminal_csi_set_mode(terminal, terminal->csi_args[i],
                                  final == 'h');
    } else if(final == 'p' && terminal->csi_intermediate == '!') {
        terminal_soft_reset(terminal);
    } else if(final == 'p' && terminal->csi_private == '?' &&
              terminal->csi_intermediate == '$') {
        terminal_csi_send_private_mode_report(terminal, n);
    } else if(final == 'p' && terminal->csi_intermediate == '$') {
        terminal_csi_send_mode_report(terminal, n);
    } else if(final == 'c') {
        if(terminal->csi_private == '>')
            terminal_write_text(terminal, "\x1b[>0;0;0c");
        else
            terminal_write_text(terminal, "\x1b[?1;2c");
    } else if(final == 'n') {
        if(terminal->csi_private == '>') {
            terminal_csi_disable_modifier_key_mode(terminal);
        } else {
            terminal_csi_send_device_status_report(terminal);
        }
    } else if(final == 't') {
        if(n == 18 || n == 19) {
            char response[32];

            snprintf(response, sizeof(response), "\x1b[%d;%d;%dt",
                     n == 19 ? 9 : 8, terminal->rows, terminal->cols);
            terminal_write_text(terminal, response);
        } else if(n == 20) {
            terminal_send_title_report(terminal, 1);
        } else if(n == 21) {
            terminal_send_title_report(terminal, 0);
        } else if(n == 22) {
            terminal_push_title_targets(terminal,
                                        terminal_csi_arg(terminal, 1, 0));
        } else if(n == 23) {
            terminal_pop_title_targets(terminal,
                                       terminal_csi_arg(terminal, 1, 0));
        }
    }
    terminal_clamp_cursor(terminal);
}

static void start_csi(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_CSI;
    terminal->csi_private = 0;
    terminal->csi_intermediate = 0;
    terminal->csi_count = 0;
    for(i = 0; i < MAX_CSI_ARGS; i++)
        terminal->csi_args[i] = 0;
}

static void start_osc(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_OSC;
    terminal->osc_len = 0;
    terminal->osc[0] = '\0';
}

static void start_dcs(TerminalState *terminal)
{
    if(terminal == NULL)
        return;
    terminal->parser_state = STATE_DCS;
    terminal_dcs_begin(terminal);
}

static void finish_dcs(TerminalState *terminal)
{
    int rows_used;

    if(terminal == NULL)
        return;
    rows_used = terminal_dcs_finish(terminal);
    if(rows_used <= 0)
        return;
    terminal->cursor_col = 0;
    while(rows_used-- > 0)
        line_feed(terminal);
    terminal_clamp_cursor(terminal);
}

static void feed_codepoint(TerminalState *terminal, unsigned int codepoint)
{
    if(terminal->parser_state == STATE_ESCAPE) {
        if(codepoint == '[') {
            start_csi(terminal);
            return;
        }
        if(codepoint == ']') {
            start_osc(terminal);
            return;
        }
        if(codepoint == 'P') {
            start_dcs(terminal);
            return;
        }
        if(codepoint == '#') {
            terminal->parser_state = STATE_HASH;
            return;
        }
        if(codepoint == '(' || codepoint == ')' || codepoint == '*' ||
           codepoint == '+') {
            terminal->pending_charset = codepoint == ')' ? 1 : 0;
            terminal->parser_state = STATE_CHARSET;
            return;
        }
        if(codepoint == 'X' || codepoint == '^' || codepoint == '_') {
            terminal->parser_state = STATE_IGNORE_STRING;
            return;
        }
        if(codepoint == '7') {
            terminal_save_cursor_state(terminal);
        } else if(codepoint == '8') {
            terminal_restore_cursor_state(terminal);
        } else if(codepoint == 'D') {
            line_feed(terminal);
        } else if(codepoint == 'E') {
            terminal->cursor_col = 0;
            line_feed(terminal);
        } else if(codepoint == 'H') {
            if(terminal->tab_stops != NULL)
                terminal->tab_stops[terminal->cursor_col] = 1;
        } else if(codepoint == 'M') {
            if(terminal->cursor_row == terminal->scroll_top)
                terminal_scroll_down(terminal, terminal->scroll_top,
                                     terminal->scroll_bottom, 1);
            else
                terminal->cursor_row--;
        } else if(codepoint == 'Z') {
            terminal_write_text(terminal, "\x1b[?1;2c");
        } else if(codepoint == 'c') {
            terminal_reset_device(terminal);
        } else if(codepoint == '=') {
            terminal->application_keypad = 1;
        } else if(codepoint == '>') {
            terminal->application_keypad = 0;
        }
        terminal->parser_state = STATE_TEXT;
        terminal_clamp_cursor(terminal);
        return;
    }
    if(terminal->parser_state == STATE_CHARSET) {
        if(terminal->pending_charset == 1)
            terminal->g1_charset = terminal_charset_from_designator(codepoint);
        else
            terminal->g0_charset = terminal_charset_from_designator(codepoint);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->parser_state == STATE_HASH) {
        if(codepoint == '8')
            terminal_screen_alignment_test(terminal);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->parser_state == STATE_CSI) {
        if(codepoint == '?' || codepoint == '>') {
            terminal->csi_private = (int)codepoint;
            return;
        }
        if(codepoint >= 0x20 && codepoint <= 0x2f) {
            terminal->csi_intermediate = (int)codepoint;
            return;
        }
        if(codepoint >= '0' && codepoint <= '9') {
            if(terminal->csi_count == 0)
                terminal->csi_count = 1;
            terminal->csi_args[terminal->csi_count - 1] =
                terminal->csi_args[terminal->csi_count - 1] * 10 +
                (int)(codepoint - '0');
            return;
        }
        if(codepoint == ';' || codepoint == ':') {
            if(terminal->csi_count < MAX_CSI_ARGS) {
                if(terminal->csi_count == 0)
                    terminal->csi_count = 1;
                terminal->csi_args[terminal->csi_count] = 0;
                terminal->csi_count++;
            }
            return;
        }
        terminal->parser_state = STATE_TEXT;
        if(codepoint >= '@' && codepoint <= '~')
            apply_csi(terminal, (int)codepoint);
        return;
    }
    if(terminal->parser_state == STATE_OSC) {
        if(codepoint == 7) {
            terminal_finish_osc(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b) {
            terminal->parser_state = STATE_OSC_ESCAPE;
            return;
        }
        if(terminal->osc_len < (int)sizeof(terminal->osc) - 1) {
            terminal->osc[terminal->osc_len++] = (char)codepoint;
            terminal->osc[terminal->osc_len] = '\0';
        }
        return;
    }
    if(terminal->parser_state == STATE_OSC_ESCAPE) {
        if(codepoint == '\\')
            terminal_finish_osc(terminal);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(terminal->parser_state == STATE_DCS) {
        if(codepoint == 0x9c || codepoint == 7) {
            finish_dcs(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b) {
            terminal->parser_state = STATE_DCS_ESCAPE;
            return;
        }
        terminal_dcs_append(terminal, codepoint);
        return;
    }
    if(terminal->parser_state == STATE_DCS_ESCAPE) {
        if(codepoint == '\\') {
            finish_dcs(terminal);
            terminal->parser_state = STATE_TEXT;
            return;
        }
        terminal_dcs_append(terminal, 0x1b);
        terminal_dcs_append(terminal, codepoint);
        terminal->parser_state = STATE_DCS;
        return;
    }
    if(terminal->parser_state == STATE_IGNORE_STRING) {
        if(codepoint == 0x9c || codepoint == 7) {
            terminal->parser_state = STATE_TEXT;
            return;
        }
        if(codepoint == 0x1b)
            terminal->parser_state = STATE_IGNORE_ESCAPE;
        return;
    }
    if(terminal->parser_state == STATE_IGNORE_ESCAPE) {
        terminal->parser_state =
            codepoint == '\\' ? STATE_TEXT : STATE_IGNORE_STRING;
        return;
    }

    if(codepoint == 0x1b) {
        terminal->parser_state = STATE_ESCAPE;
        return;
    }
    if(codepoint == '\r') {
        terminal->cursor_col = 0;
        return;
    }
    if(codepoint == '\n') {
        newline_control(terminal);
        return;
    }
    if(codepoint == '\v' || codepoint == '\f') {
        newline_control(terminal);
        return;
    }
    if(codepoint == '\b') {
        if(terminal->cursor_col > 0)
            terminal->cursor_col--;
        return;
    }
    if(codepoint == '\t') {
        terminal->cursor_col = terminal_next_tab_stop(terminal);
        terminal_clamp_cursor(terminal);
        return;
    }
    if(codepoint == 0x0e) {
        terminal->active_charset = 1;
        return;
    }
    if(codepoint == 0x0f) {
        terminal->active_charset = 0;
        return;
    }
    if(codepoint == 7) {
        terminal->bell_pending = 1;
        return;
    }
    if(codepoint >= 32)
        put_codepoint(terminal, codepoint);
}

static void feed_byte(TerminalState *terminal, unsigned char byte)
{
    if(byte == 0x18 || byte == 0x1a) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        terminal->parser_state = STATE_TEXT;
        terminal->osc_len = 0;
        ResetTerminalPaneDCSBuffer(&terminal->dcs);
        return;
    }
    if(terminal->utf8_remaining > 0) {
        if((byte & 0xc0) == 0x80) {
            terminal->utf8_codepoint =
                (terminal->utf8_codepoint << 6) | (byte & 0x3f);
            terminal->utf8_remaining--;
            if(terminal->utf8_remaining == 0) {
                feed_codepoint(terminal,
                               (unsigned int)terminal->utf8_codepoint);
                terminal->utf8_codepoint = 0;
            }
            return;
        }
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        feed_codepoint(terminal, '?');
    }
    if(byte == 0x84) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        newline_control(terminal);
        return;
    }
    if(byte == 0x85) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        terminal->cursor_col = 0;
        line_feed(terminal);
        return;
    }
    if(byte == 0x88) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        if(terminal->tab_stops != NULL)
            terminal->tab_stops[terminal->cursor_col] = 1;
        return;
    }
    if(byte == 0x8d) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        if(terminal->cursor_row == terminal->scroll_top)
            terminal_scroll_down(terminal, terminal->scroll_top,
                                 terminal->scroll_bottom, 1);
        else
            terminal->cursor_row--;
        terminal_clamp_cursor(terminal);
        return;
    }
    if(byte == 0x9b) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_csi(terminal);
        return;
    }
    if(byte == 0x9d) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_osc(terminal);
        return;
    }
    if(byte == 0x98 || byte == 0x9e || byte == 0x9f) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        terminal->parser_state = STATE_IGNORE_STRING;
        return;
    }
    if(byte == 0x90) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        start_dcs(terminal);
        return;
    }
    if(byte == 0x9c) {
        terminal->utf8_remaining = 0;
        terminal->utf8_codepoint = 0;
        if(terminal->parser_state == STATE_DCS ||
           terminal->parser_state == STATE_DCS_ESCAPE)
            finish_dcs(terminal);
        else if(terminal->parser_state == STATE_OSC ||
                terminal->parser_state == STATE_OSC_ESCAPE)
            terminal_finish_osc(terminal);
        terminal->parser_state = STATE_TEXT;
        return;
    }
    if(byte < 0x80) {
        feed_codepoint(terminal, byte);
    } else if((byte & 0xe0) == 0xc0) {
        terminal->utf8_codepoint = byte & 0x1f;
        terminal->utf8_remaining = 1;
    } else if((byte & 0xf0) == 0xe0) {
        terminal->utf8_codepoint = byte & 0x0f;
        terminal->utf8_remaining = 2;
    } else if((byte & 0xf8) == 0xf0) {
        terminal->utf8_codepoint = byte & 0x07;
        terminal->utf8_remaining = 3;
    } else {
        feed_codepoint(terminal, '?');
    }
}

void terminal_feed(TerminalState *terminal, const void *data, int size)
{
    const unsigned char *bytes = data;
    int i;

    if(terminal == NULL || bytes == NULL || size <= 0)
        return;
    for(i = 0; i < size; i++)
        feed_byte(terminal, bytes[i]);
}
