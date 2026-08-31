# ktrem

ktrem is a standalone terminal application using Kryon for windowing,
rendering, input, and platform integration. The terminal emulator belongs here,
not in Kryon.

```sh
make
make run
make test
make install
```

`make install` installs `ktrem`, the Rill host module, and a `kterm`
compatibility command name under the selected `PREFIX` (`~/.local` by
default).

Release builds are covered by `.github/workflows/release.yml`: pull requests
and pushes run the Linux build/test path, and published GitHub Releases upload
Linux `tar.gz`, `.deb`, and `.AppImage` assets plus checksums. The remaining
feature and release-readiness plan is tracked in
`docs/REMAINING_FEATURE_PLAN.md`.

By default the build uses `../kryon`. Override with:

```sh
make ENGINE_DIR=/path/to/kryon
```

Run options:

```sh
ktrem --working-directory /path/to/project
ktrem --default-working-directory /path/to/project
ktrem --shell /bin/bash
ktrem --command 'make test'
ktrem --command='make test'
ktrem -e 'make test'
ktrem -x make test
ktrem --title 'Build'
ktrem --command 'make test' --tab --title 'Logs' --command 'tail -f build.log'
ktrem --drop-down --title 'System' --command htop
ktrem --geometry 120x40
ktrem --fullscreen
ktrem --maximize
ktrem --hold
ktrem --font-size 18
ktrem --scrollback 10000
ktrem --cursor-style bar
ktrem --terminal-font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
ktrem --terminal-foreground '#f2f2f2' --terminal-background '#101010' --terminal-cursor '#f2f2f2'
ktrem --terminal-selection-foreground '#101010' --terminal-selection-background '#5a8fd8'
```

ktrem accepts common xfce4-terminal launch aliases:
`--command`/`-e`, `--execute`/`-x`, `--default-working-directory`,
`--title`/`-T`, `--hold`/`-H`, `--geometry`, `--fullscreen`, `--maximize`,
`--drop-down`, `--disable-server`, `--tab`, `--window`, and
menubar/toolbar/border visibility flags. `--tab` starts additional tabs from
command-line specs. `--drop-down` starts a borderless top-of-screen terminal
window. `--window` is accepted as a separator and currently opens another tab
because ktrem does not have multi-window process/server support yet.

Config is read from `$XDG_CONFIG_HOME/ktrem/config` or
`~/.config/ktrem/config`:

```ini
font_size=16
scrollback=5000
cursor_style=block
terminal_font=/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
terminal_foreground=#f2f2f2
terminal_background=#101010
terminal_cursor=#f2f2f2
terminal_selection_foreground=#101010
terminal_selection_background=#5a8fd8
shell=/bin/bash
working_directory=/home/me/project
```

String values saved by ktrem use backslash escapes for tabs, newlines, and
literal backslashes.

ktrem restores open tabs on startup when no command is configured, including
the active tab plus each tab's working directory, shell, command, scroll
position, and manual title override. The restore file lives under
`$XDG_STATE_HOME/ktrem/session`
or `~/.local/state/ktrem/session`.

ktrem chrome and default terminal foreground/background/cursor colors follow
the Kryon system theme. Explicit profile colors and OSC color changes stay in
control until reset.

Use the Terminal menu to update profile defaults for shell, working directory,
terminal font, font size, colors, selection colors, scrollback, cursor color,
and cursor style. Changes are saved to the same config file and apply to
future tabs; font, colors, scrollback, and cursor changes also apply to the
running window.

Shortcuts:

- `Ctrl+Shift+T`: new tab
- `Ctrl+Shift+W`: close tab
- `Ctrl+Tab`: next tab
- `Ctrl+Shift+Tab`: previous tab
- `Ctrl+PageUp` / `Ctrl+PageDown`: previous tab, next tab
- `Alt+1` through `Alt+8`: switch directly to that tab
- `Ctrl+Shift+C`: copy selection
- `Ctrl+Shift+V`: paste
- `Ctrl+Insert` / `Shift+Insert`: copy selection, paste
- `Ctrl+Shift+F`: find
- `Ctrl+Shift+G`: find next
- `Ctrl+Shift+B`: find previous
- `Ctrl++` / `Ctrl+-` / `Ctrl+0`: zoom in, zoom out, reset size
- `Shift+PageUp` / `Shift+PageDown`: scroll

Find searches scrollback plus the visible terminal. Lowercase queries match
case-insensitively; queries containing uppercase letters are case-sensitive.

Mouse selection keeps a local primary selection for the terminal; middle click
pastes that selection before falling back to the system clipboard, and the
right-click menu exposes both clipboard paste and primary-selection paste.
Copied soft-wrapped lines are joined without adding artificial newlines.
Bracketed paste wraps pasted text and strips embedded terminal control
sequences so copied text cannot break out of the paste bracket.
OSC 52 clipboard queries are synced from the host clipboard before terminal
output is parsed, and OSC 52 clipboard writes update the host clipboard.
OSC 8 terminal hyperlinks are underlined with the theme link color and open
with `Ctrl` + left click.

Terminal graphics:

- Sixel images are decoded from DCS sequences and rendered inline in the
  terminal viewport.
- DEC special graphics character sets are mapped to Unicode line-drawing
  characters for ncurses-style terminal UIs.
- Insert/replace mode (`CSI 4 h/l`) is supported, including terminal mode
  reports.
- ANSI newline mode (`CSI 20 h/l`) controls whether LF/VT/FF also return to
  column zero, including terminal mode reports.
- SGR rendering covers bold, faint, italic, underline, underline color, blink,
  inverse, conceal, strike, overline, indexed colors, and truecolor.
- Application cursor-key mode covers arrow keys plus Home/End, including
  modified Home/End sequences.
- Function-key output covers F1-F24 with xterm-style modifier sequences, and
  XTGETTCAP advertises those emitted key capabilities.
- Application keypad mode covers digits, operators, center/corner keys, and
  Enter, with matching XTGETTCAP keypad capability reports.
- Printable-key input maps Ctrl letters and common Ctrl punctuation, including
  `Ctrl+/` and `Ctrl+-`, to terminal control bytes, with Alt preserved as an
  ESC prefix.
- xterm `modifyOtherKeys` mode (`CSI > 4 ; n m`) is supported for modified
  printable-key reporting, including the xterm disable sequence (`CSI > 4 n`).
- Mouse reporting covers X10, normal, button-event, any-event, SGR coordinates,
  SGR-Pixels coordinates, UTF-8 coordinates, urxvt coordinates, wheel events,
  motion events, and modifier bits.
- Cursor save/restore keeps cursor position, rendition, active character set,
  origin/wrap mode, insert mode, and hyperlink state.
- Alternate-screen mode presents only the alternate grid while preserving normal
  scrollback for restoration after full-screen apps exit.
- xterm alternate-scroll mode (`DECSET 1007`) sends wheel movement as cursor
  keys in the alternate screen when mouse reporting is not active.
- xterm focus reporting (`DECSET 1004`) sends focus changes for both window
  focus and active-tab changes.
- 8-bit C1 CSI/OSC/DCS plus IND/NEL/HTS/RI controls are parsed, and APC/PM/SOS
  control strings are ignored so their payloads do not leak into the terminal
  grid.
- BEL triggers a short theme-colored visual bell without writing text into the
  terminal grid.
- CAN/SUB cancel active control sequences and return the parser to text mode.
- Common CSI aliases are supported, including horizontal/vertical relative
  moves, horizontal absolute moves, and repeat preceding graphic character.
- DECSTR soft reset (`CSI ! p`) resets terminal modes, margins, rendition, and
  character sets without clearing the visible grid.
- DECRQSS status-string reports answer SGR, scroll-margin, cursor-style, and
  character-protection queries with valid DCS responses.
- XTGETTCAP (`DCS + q`) reports the terminal name, color capabilities, and the
  special-key sequences ktrem actually emits.
- Horizontal tab set/clear controls are supported, including clearing all tab
  stops for terminal UI alignment.
- DEC private cursor-position reports and DEC screen-alignment test are
  handled.
- DEC private cursor visibility and blink modes are reported and rendered.
- DECSCUSR cursor-shape controls distinguish blinking and steady block,
  underline, and bar cursors.
- XTWINOPS text-area and screen character-size reports return the active
  terminal grid dimensions.
- VT next-line and DECID escape controls are handled.
- OSC palette queries return xterm-style indexed responses and fall back to the
  built-in 256-color palette when a color has not been overridden.
- OSC color parsing accepts compact and wide X-style hash forms in addition to
  `rgb:` values.
- OSC dynamic colors cover default foreground/background, cursor, selection
  background, selection foreground, and indexed palette entries.
- OSC title handling distinguishes window and icon titles, and includes
  target-aware save/restore stack controls for terminal programs that
  temporarily replace titles.
- OSC 52 clipboard sequences can set the host clipboard and answer bounded
  clipboard queries from the current host clipboard.
- Terminal foreground, background, and cursor-color resets return to the
  resolved Kryon/system theme colors unless the profile explicitly overrides
  them.
- Sixel color registers, RGB/HLS color definitions, raster attributes, repeat
  commands, row advance, carriage return, transparency, 7-bit DCS, and 8-bit
  DCS/ST are supported.
- Erase-line, erase-display, reset, alternate-screen clear, scrolling, and
  scrollback eviction keep sixel images attached to the terminal content.
