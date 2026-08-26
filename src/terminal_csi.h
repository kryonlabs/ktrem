#ifndef KTREM_TERMINAL_CSI_H
#define KTREM_TERMINAL_CSI_H

#include "terminal.h"

int terminal_csi_arg(const TerminalState *terminal, int index, int fallback);
int terminal_csi_cursor_row_from_arg(const TerminalState *terminal, int index,
                                     int fallback);
void terminal_csi_set_modifier_key_mode(TerminalState *terminal);
void terminal_csi_disable_modifier_key_mode(TerminalState *terminal);
void terminal_csi_set_mode(TerminalState *terminal, int mode, int enabled);
void terminal_csi_apply_cursor_style(TerminalState *terminal);
void terminal_csi_send_private_mode_report(TerminalState *terminal, int mode);
void terminal_csi_send_mode_report(TerminalState *terminal, int mode);
void terminal_csi_send_device_status_report(TerminalState *terminal);

#endif
