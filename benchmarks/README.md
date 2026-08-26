# ktrem Terminal Benchmarks

Benchmarks run GUI terminals under Xvfb by default so they do not open windows
on the user's active desktop. The harness prefers the local build at
`build/<platform>-<arch>/bin/ktrem`; set `KTREM_BENCH_KTREM_BIN=/path` to
benchmark another binary. Set `KTREM_BENCH_USE_REAL_DISPLAY=1` only when an
interactive desktop run is intentional.

```sh
benchmarks/run-terminal-benchmarks.sh
```

Pass a second argument to test a different active-output FPS cap for ktrem
while keeping idle FPS unchanged. Pass a third argument to test a different
bounded PTY drain burst in milliseconds. Pass a fourth argument for repeated
runs. Pass a fifth argument to restrict the workload list:

```sh
benchmarks/run-terminal-benchmarks.sh /tmp/ktrem-bench 500
benchmarks/run-terminal-benchmarks.sh /tmp/ktrem-bench 1000 8 3
benchmarks/run-terminal-benchmarks.sh /tmp/ktrem-bench 1000 12 3 dense_sgr
benchmarks/run-terminal-benchmarks.sh /tmp/ktrem-bench 1000 8 3 "paste_burst hyperlink_grid search_corpus"
```

The externally driven live-resize workload uses X window control through
`xdotool`. If the automatic wrapper cannot find the resize target under a
particular Xvfb setup, run the explicit virtual-display form:

```sh
env XDG_RUNTIME_DIR=/tmp/ktrem-runtime KTREM_BENCH_IN_VIRTUAL_DISPLAY=1 \
  xvfb-run -a -s '-screen 0 1280x800x24' \
  sh benchmarks/run-terminal-benchmarks.sh /tmp/ktrem-live-resize 1000 8 3 live_resize
```

The externally driven clipboard workload uses a Tk clipboard owner and
`xdotool`. By default the harness sends a held `Ctrl+Shift+V`, matching the
standard terminal paste accelerator and giving ktrem and xfce4-terminal the
same synthetic shortcut. Override with
`KTREM_BENCH_PASTE_ACCEL=shift_insert`, `ctrl_shift_v`, or `ctrl_shift_V`
when debugging shortcut handling.

The harness appends summary records with min/average/max elapsed time per
terminal and workload.

## Latest Isolated Result

Result file:
`benchmarks/results/terminal-benchmarks-20260821T000833Z.jsonl`

Three isolated runs, `KTREM_ACTIVE_FPS=1000`,
`KTREM_PTY_BURST_MS=8`:

| Workload | ktrem avg | ktrem min/max | xfce4-terminal avg | xfce min/max | Current state |
|---|---:|---:|---:|---:|---|
| startup payload | 0.0 ms | 0/0 | 1.7 ms | 0/5 | matched |
| ANSI flood, 18k lines | 81.7 ms | 76/86 | 187.0 ms | 90/378 | faster |
| Unicode table, 6k lines | 18.0 ms | 18/18 | 93.0 ms | 33/130 | faster |
| alternate-screen redraw | 22.3 ms | 21/24 | 61.0 ms | 30/121 | faster |
| dense SGR styling, 9k lines | 84.0 ms | 83/85 | 91.3 ms | 82/103 | faster average, tied best case |
| long wrapped/reflow-like lines | 42.7 ms | 40/46 | 114.0 ms | 62/216 | faster |
| scrollback flood, 50k lines | 86.7 ms | 81/95 | 107.7 ms | 106/110 | faster average |
| cursor matrix updates | 72.7 ms | 72/73 | 124.3 ms | 83/203 | faster |

The same dense-SGR workload with a 12 ms PTY burst measured ktrem at
75.0 ms average and xfce4-terminal at 79.0 ms average, but a full-suite 12 ms
run produced larger ktrem outliers in other workloads. The default is
therefore 8 ms.

Treat individual runs as directional because Xvfb/GTK scheduling can introduce
outliers. Min/average/max summaries are more useful than a single number.

## Expanded Output Result

Result file:
`benchmarks/results/terminal-benchmarks-20260821T004722Z.jsonl`

Three isolated runs of the newer paste/link/search-corpus workloads,
`KTREM_ACTIVE_FPS=1000`, `KTREM_PTY_BURST_MS=8`:

| Workload | ktrem avg | ktrem min/max | xfce4-terminal avg | xfce min/max | Current state |
|---|---:|---:|---:|---:|---|
| paste-like burst, 12k lines | 69.3 ms | 63/74 | 154.7 ms | 78/303 | faster |
| OSC 8 hyperlink grid, 7k lines | 76.7 ms | 71/84 | 276.7 ms | 212/341 | faster |
| searchable corpus, 18k lines | 47.7 ms | 46/49 | 156.0 ms | 65/277 | faster |

These are command-output benchmarks. The externally driven clipboard and
find-dialog timings are listed below; this section covers the rendering/parser
pressure those features create: large pasted text, hyperlink state, and
search-sized scrollback content.

## Live Resize Result

Result file:
`benchmarks/results/terminal-benchmarks-20260821T115057Z.jsonl`

Three isolated runs under Xvfb. The workload preloads 1200 wrapped Unicode
lines, then drives 120 external window resize operations with `xdotool`.
This measures window resize handling around real X windows rather than
parser-only `terminal_resize()` calls:

| Workload | ktrem avg | ktrem min/max | xfce4-terminal avg | xfce min/max | Current state |
|---|---:|---:|---:|---:|---|
| live external resize, 120 resizes | 158.7 ms | 155/162 | 177.3 ms | 171/185 | faster |

## External Clipboard Paste Result

Result file:
`benchmarks/results/terminal-benchmarks-20260821T122119Z.jsonl`

Three isolated runs under Xvfb. The workload owns the X clipboard from a
separate Tk process, focuses the terminal window, sends the terminal's standard
paste accelerator, and waits for a raw-mode process inside the terminal to
receive the full clipboard payload:

| Workload | ktrem avg | ktrem min/max | xfce4-terminal avg | xfce min/max | Current state |
|---|---:|---:|---:|---:|---|
| external clipboard paste, 29 lines | 206.3 ms | 206/207 | 256.7 ms | 256/257 | faster |

Earlier `20260821T121806Z` measured ktrem's `Shift+Insert` path against
xfce4-terminal's `Ctrl+Shift+V` path. The newer result above uses held
`Ctrl+Shift+V` for both terminals.

## External Find Dialog Result

Result file:
`benchmarks/results/terminal-benchmarks-20260821T123551Z.jsonl`

Three isolated runs under Xvfb. The workload preloads the same 18k-line search
corpus, opens the terminal find UI with held `Ctrl+Shift+F`, types
`needle-critical`, submits the search, closes the find UI, then verifies PTY
input is restored by typing a marker into the running target:

| Workload | ktrem avg | ktrem min/max | xfce4-terminal avg | xfce min/max | Current state |
|---|---:|---:|---:|---:|---|
| external find dialog, 18k-line corpus | 613.0 ms | 611/614 | 635.7 ms | 608/691 | faster average |

This result depends on two parity fixes: ktrem disables raylib's default
Escape-to-close-window behavior, and Kryon prompt dialogs now treat text-field
Enter as confirm while ktrem closes its find prompt on Escape.

Earlier result files before `20260820T230122Z` used `ktrem` from `PATH`.
Those are still useful for installed-binary regressions, but local worktree
performance comparisons should use the corrected harness or explicitly set
`KTREM_BENCH_KTREM_BIN`.

The benchmark harness now fails fast when a terminal process exits before
writing its workload result. ktrem also exits with
`ktrem: graphics backend failed to initialize` instead of continuing into
font upload and draw code when SDL/raylib does not provide a usable GL context.

ktrem now drains PTY output with an adaptive frame cadence: it idles at
`KTREM_TARGET_FPS` (default 60) and temporarily raises to
`KTREM_ACTIVE_FPS` (default 1000) after PTY bytes arrive. During active
output it also performs a bounded PTY drain burst controlled by
`KTREM_PTY_BURST_MS` (default 8, valid range 0-20). This removed most of the
previous shell-output blocking:

| ktrem state | ANSI flood | Unicode table | Alternate redraw |
|---|---:|---:|---:|
| fixed 60 FPS loop | 675 ms | 20 ms | 339 ms |
| adaptive active-output loop | 118 ms | 18 ms | 41 ms |
| adaptive loop + 4 ms PTY burst | 90 ms | 26 ms | 33 ms |
| adaptive loop + 8 ms PTY burst | 81.7 ms avg | 18.0 ms avg | 22.3 ms avg |

PTY burst tuning on the same workload set:

| `KTREM_PTY_BURST_MS` | ANSI flood | Unicode table | Alternate redraw | Dense SGR |
|---:|---:|---:|---:|---:|
| 0 | 118 ms | 20 ms | 39 ms | not measured |
| 1 | 88 ms | 17 ms | 28 ms | not measured |
| 2 | 90 ms | 21 ms | 25 ms | not measured |
| 4 | 84 ms | 19 ms | 23 ms | 108.7 ms avg |
| 8 | 81.7 ms avg | 18.0 ms avg | 22.3 ms avg | 84.7 ms avg |
| 10 | not measured full-suite | not measured full-suite | not measured full-suite | 82.7 ms avg |
| 12 | unstable full-suite | unstable full-suite | unstable full-suite | 75.0 ms avg |

The default is 8 ms because it closes the synthetic throughput gaps without
the wider outliers seen at 12 ms in the full suite. Higher values remain useful
for local tuning via `KTREM_PTY_BURST_MS`.

The previous text-run batching experiment in `app_terminal_view.c` regressed
the same harness (`ansi_flood` 694 ms, `alternate_redraw` 344 ms), so it was
reverted. The bottleneck is not solved by naive grouping at the ktrem view
layer.

Kryon `DrawTexturePro`-based terminal glyph experiments also failed to close
the gap:

| Trial | ANSI flood | Unicode table | Alternate redraw | Result |
|---|---:|---:|---:|---|
| grid primitive, uncached | 749 ms | 22 ms | 345 ms | reverted from ktrem |
| grid primitive, cached | 678 ms | 33 ms | 345 ms | reverted from ktrem |
| single-cell primitive, cached | 566 ms | 21 ms | 328 ms | reverted from ktrem |

These experiments show that wrapping raylib texture draws is not enough. A
direct `rlgl` quad stream inside `DrawTerminalPaneGlyphGrid` also failed when
wired into ktrem: `ansi_flood` measured 578 ms before texture-state caching
and 710 ms after the explicit batch-limit/texture-cache variant, while
`alternate_redraw` stayed around 344 ms. That path was removed from ktrem's
active renderer. The next renderer work needs a real terminal renderer rather
than a lower-level spelling of the same per-glyph work: persistent glyph
instances or vertex buffers, dirty row/cell damage, and batched decorations.

Parser-only replay file:
`make benchmark-parser`

| Workload | Parser replay |
|---|---:|
| startup payload | 0 ms |
| ANSI flood, 18k lines | 29 ms |
| Unicode table, 6k lines | 7 ms |
| alternate-screen redraw | 4 ms |
| dense SGR styling, 9k lines | 14 ms |
| long wrapped/reflow-like lines | 19 ms |
| scrollback flood, 50k lines | 66 ms |
| cursor matrix updates | 3 ms |
| paste-like burst, 12k lines | 27 ms |
| OSC 8 hyperlink grid, 7k lines | 13 ms |
| searchable corpus, 18k lines | 30 ms |
| 500 visible searches over scrollback | 0 ms |
| 120 resize/reflows over 5k-line buffer | 739 ms |

This proves the current parser/screen model is not the dominant blocker for the
measured GUI output cost. The latest PTY scheduling work makes ktrem
competitive with xfce4-terminal on the current synthetic GUI workloads.
Resize/reflow remains the exception, but streaming combined scrollback/main
reflow reduced the measured replay from 2393 ms to 739 ms for this workload by
avoiding a large worst-case temporary output matrix on each resize. More work
is still needed before claiming broad resize parity across real desktop window
managers, but the focused Xvfb live-resize workload currently measures ktrem
ahead of xfce4-terminal.

## Parity Plan

To be a practical xfce4-terminal alternative, ktrem needs parity in these
areas:

- Speed: keep ktrem at or above xfce4-terminal on startup, streaming output,
  Unicode output, alternate-screen redraw, scrollback search, paste,
  resize/reflow, and large scrollback memory behavior. The synthetic repeated
  GUI suite now covers startup, ANSI flood, Unicode, alternate-screen redraw,
  dense styling, wrapped output, large scrollback output, cursor-matrix
  updates, paste-like output, hyperlink-heavy output, and search-corpus output.
  Parser replay now covers visible search and repeated resize/reflow. The Xvfb
  GUI harness now covers live external window resize, external clipboard paste,
  and external find-dialog workflow timing.
- Terminal compatibility: keep expanding parser coverage through real apps:
  `vim`, `nano`, `less`, `htop`, `tmux`, `ssh`, `git`, package managers,
  Python/Node/Rust test output, curses dashboards, and sixel tools.
- UI features: tabs, direct tab switching, command-line tab launch specs,
  close behavior, search, copy/paste,
  primary selection, URL handling, profiles, preferences, font choice,
  scrollback limits, command/working-directory launch, session restore,
  fullscreen/drop-down style operation, command-line single-instance behavior,
  and keyboard shortcut configuration.
- Glyph coverage: keep explicit terminal glyph sets for box drawing, blocks,
  symbols, Powerline, Greek, Cyrillic, and common CJK; add fontconfig-backed
  fallback if static fallback paths are not enough on target systems.
- Packaging: desktop file, icon, install docs, release assets, terminfo if the
  advertised terminal name diverges from xterm-compatible behavior.
- Website: publish the benchmark table, the parity matrix, install command,
  screenshots, and the remaining-feature plan on `ktrem.kryonlabs.com` using
  the Kryon Labs visual style once a website source/project exists.

## Xfce Launch Parity

Current command-line compatibility coverage:

| Xfce surface | ktrem state |
|---|---|
| `--command`, `-e` | implemented for single-window and tab-spec launch |
| `--execute`, `-x` | implemented by shell-quoting the remaining argv |
| `--working-directory`, `--default-working-directory` | implemented |
| `--title`, `-T` | implemented as the initial tab/window title and per-tab title |
| `--hold`, `-H` | accepted; ktrem already keeps command sessions visible after child exit |
| `--geometry COLSxROWS` | implemented for initial terminal grid estimate and window size |
| `--fullscreen`, `--maximize` | implemented through Kryon/raylib window state |
| `--drop-down` | implemented for first-run borderless top-of-screen window shape |
| `--show-menubar`, `--hide-menubar` | accepted; ktrem has fixed minimal chrome today |
| `--show-toolbar`, `--hide-toolbar` | accepted; no separate toolbar exists today |
| `--show-borders`, `--hide-borders` | implemented through window decoration flags where supported |
| `--disable-server` | accepted as a no-op because ktrem has no D-Bus single-instance server |
| `--color-table`, `--version`, `--help` | implemented |
| `--tab` multi-spec launch | implemented for up to ktrem's current 8-tab session limit |
| `--window` multi-spec launch | accepted as a separator and currently opened as another tab |
| drop-down visibility toggle | not implemented yet; requires single-instance/server behavior |

The remaining xfce command-line parity work is real separate-window launch and
server behavior, icon/display/role/startup-id integration beyond accepting the
options, and the repeated-invocation drop-down visibility toggle.

## Backend Direction

xfce4-terminal is based on VTE/GTK. Xfce's own documentation describes it as a
lightweight terminal with tabs, unlimited scrolling, colors, fonts,
transparency, and drop-down mode, and also states that VTE rendering speed with
font antialiasing can be an issue. Xfce's command-line docs define the parity
surface ktrem still needs to cover: tabs/windows, command execution, working
directory, title, hold, geometry, fullscreen/maximize, menubar/borders/toolbar
visibility, and related launch options. Xfce preferences also include
rewrapping content on resize.

Modern high-performance terminal projects point toward retained GPU rendering:
Alacritty identifies itself as a fast OpenGL terminal and requires at least
OpenGL ES 2.0. WezTerm exposes GPU front ends for OpenGL and WebGPU, and its
WebGPU path maps to platform backends including Metal, Vulkan, and DirectX 12.

ktrem should not switch away from Kryon for the app. The parser replay plus
the failed `DrawTexturePro` and `rlgl` stream trials show the next correct
architecture is a Kryon terminal-grid renderer backed by retained GPU data:
fixed-cell glyph instances, foreground/background runs,
underline/overline/strike decorations, cursor shape, and dirty row/cell
damage. ktrem should use that primitive while keeping terminal emulation in
ktrem.

Backend recommendation: keep Kryon as the abstraction, but do not rely on
raylib's immediate-mode-style text helpers for ktrem's terminal surface. Add
a Kryon-owned retained terminal-grid renderer: dirty row/cell tracking,
persistent glyph instances or vertex buffers, foreground/background runs,
custom-drawn box/block glyphs, underline/overline/strike decorations, cursor
shape, and image planes for sixel/graphics. Use OpenGL/GLES instanced quads as
the near-term Linux backend because ktrem already ships on Kryon's current
desktop stack. Design the Kryon renderer API so the backend can later be
implemented on wgpu/WebGPU for Vulkan/Metal/DX portability. Replacing ktrem
with GTK/VTE would meet feature parity quickly but would stop showcasing Kryon.

References:

- Xfce Terminal docs: https://docs.xfce.org/apps/xfce4-terminal/start
- Xfce Terminal command-line options: https://docs.xfce.org/apps/xfce4-terminal/4.16/command-line
- Xfce Terminal preferences: https://docs.xfce.org/apps/xfce4-terminal/preferences
- GNOME VTE docs: https://gnome.pages.gitlab.gnome.org/vte/gtk4/
- Alacritty README: https://github.com/alacritty/alacritty/blob/master/README.md
- WezTerm front-end docs: https://wezterm.org/config/lua/config/front_end.html
