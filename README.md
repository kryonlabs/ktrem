# Kapsule

Kapsule is a standalone terminal application using Kryon for windowing,
rendering, input, and platform integration. The terminal emulator belongs here,
not in Kryon.

```sh
make
make run
make test
```

By default the build uses `../kryon`. Override with:

```sh
make ENGINE_DIR=/path/to/kryon
```

Run options:

```sh
kapsule --working-directory /path/to/project
kapsule --shell /bin/bash
kapsule --command 'make test'
kapsule --font-size 18
kapsule --scrollback 10000
kapsule --cursor-style bar
kapsule --terminal-font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
kapsule --terminal-foreground '#f2f2f2' --terminal-background '#101010'
```

Config is read from `$XDG_CONFIG_HOME/kapsule/config` or
`~/.config/kapsule/config`:

```ini
font_size=16
padding=10
scrollback=5000
cursor_style=block
terminal_font=/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
terminal_foreground=#f2f2f2
terminal_background=#101010
shell=/bin/bash
working_directory=/home/me/project
```

Kapsule restores open tabs on startup when no command is configured. The
restore file lives under `$XDG_STATE_HOME/kapsule/session` or
`~/.local/state/kapsule/session`.

Use the Terminal menu to update profile defaults for shell, working directory,
terminal font, font size, colors, scrollback, and cursor style. Changes are
saved to the same config file and apply to future tabs; font, colors,
scrollback, and cursor changes also apply to the running window.

Shortcuts:

- `Ctrl+Shift+T`: new tab
- `Ctrl+Shift+W`: close tab
- `Ctrl+Tab`: next tab
- `Ctrl+Shift+C`: copy selection
- `Ctrl+Shift+V`: paste
- `Ctrl+Shift+F`: find
- `Ctrl+Shift+G`: find next
- `Ctrl+Shift+B`: find previous
- `Shift+PageUp` / `Shift+PageDown`: scroll

Mouse selection keeps a local primary selection for the terminal; middle click
pastes that selection before falling back to the system clipboard.
OSC 8 terminal hyperlinks are underlined with the theme link color and open
with `Ctrl` + left click.

Terminal graphics:

- Sixel images are decoded from DCS sequences and rendered inline in the
  terminal viewport.
- DEC special graphics character sets are mapped to Unicode line-drawing
  characters for ncurses-style terminal UIs.
- Insert/replace mode (`CSI 4 h/l`) is supported, including terminal mode
  reports.
- Application cursor-key mode covers arrow keys plus Home/End, including
  modified Home/End sequences.
- Mouse reporting covers X10, normal, button-event, any-event, SGR coordinates,
  UTF-8 coordinates, wheel events, motion events, and modifier bits.
- Cursor save/restore keeps cursor position, rendition, active character set,
  origin/wrap mode, insert mode, and hyperlink state.
- OSC palette queries return xterm-style indexed responses and fall back to the
  built-in 256-color palette when a color has not been overridden.
- Terminal foreground, background, and cursor-color resets return to the
  resolved Kryon/system theme colors unless the profile explicitly overrides
  them.
- Sixel color registers, RGB/HLS color definitions, raster attributes, repeat
  commands, row advance, carriage return, transparency, 7-bit DCS, and 8-bit
  DCS/ST are supported.
- Erase-line, erase-display, reset, alternate-screen clear, and scrolling
  operations clean up or move sixel images with the terminal content.
