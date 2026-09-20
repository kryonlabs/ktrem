# t9 `.kry` rewrite plan

## Target

Author t9's terminal model, parser, session behavior, settings, and interface in
`.kry`. Generate the Linux and Plan 9 executables from those sources with
Kryon's supported compiler. The standalone window and Rill host must use the
same app state and rendering code. The installed command remains the real `t9`
executable.

Keep terminal product code in this repository. Kryon may gain small reusable
language or rendering primitives when the rewrite proves they are needed. C
may remain only at the operating system boundary for calls that `.kry` cannot
express, such as creating a PTY, spawning a child, and polling native file
descriptors. Those bindings must expose operations, not terminal behavior or
widgets. Remove the current C implementation of each subsystem when its `.kry`
replacement lands; do not retain a second execution path or command alias.

## Work sequence

| Stage | Work | Exit check |
|---|---|---|
| 1. Compiler and platform proof | Build a small `.kry` program with `k2c` on Linux and with the native Plan 9 toolchain. Prove byte buffers, bounded arrays, UTF-8 iteration, mutable session state, callbacks, generated headers, and calls to the platform boundary. Measure a styled 120×40 terminal frame before choosing the row drawing surface. | Both targets build and run; any missing general Kryon capability has an upstream issue or committed primitive. |
| 2. Terminal engine | Move `Cell`, screen and scrollback storage, parser state, UTF-8/grapheme handling, CSI/OSC/DCS, SGR, modes, alternate screen, reflow, links, selection, search, and Sixel state into `.kry` modules. Replace one C module at a time, using the same byte traces as the existing terminal tests. | Recorded traces produce identical cell contents, colors, cursor state, titles, links, and responses on Linux and Plan 9. All terminal engine C modules are removed. |
| 3. Sessions and persistence | Move launch options, profiles, config, session files, title formatting, tab ordering, child exit handling, and restore policy into `.kry`. Keep the canonical `t9` config and state paths. | A plain launch opens one tab; `exit` removes its tab; the last exit closes the window; closed tabs are absent from saved state; titles show `Terminal - user@host ~` or the current directory. |
| 4. Native process boundary | Define the smallest shared operations for spawn, read, write, resize, exit status, and teardown. Implement Linux PTY and Plan 9 process/file operations behind that boundary, with no parser, session, or UI decisions in C. | Multiple tabs run independently, resize propagates, output and input survive stress tests, and children are reaped on both systems. |
| 5. Interface | Move the app frame, tabs, menus, dialogs, profile editor, search, keyboard/mouse routing, terminal viewport, selection, links, cursor, and images to `.kry`. Use canonical Kryon widgets and props. Establish a reusable styled text row surface in Kryon if ordinary `Text(TextProps)` cannot meet terminal rendering and clipping needs; use `Image(ImageProps)` for Sixel images. | Existing UI behavior and shortcuts pass on a private Xvfb display, with readable glyphs, correct colors, and no regression in scrolling or input latency. No app UI C remains. |
| 6. Rill and build cutover | Generate both standalone and Rill host entry points from the same `.kry` app. Update `Makefile`, `mkfile`, Taiji, packages, and the showcase to build and install only `t9`. Delete obsolete C product sources and generated output from source control. | Linux build, release packaging, native Plan 9 `mk install`, and Rill `host:t9` use one implementation. A fresh clone builds without an old name or a compatibility shim. |

## Verification and cutover rules

- Keep the current C test vectors as behavioral fixtures while moving each
  subsystem. Add `.kry` tests for parser boundaries, long lines, UTF-8,
  combining characters, wide glyphs, colors, Sixel, search, and resize. Retire
  each C test only after the equivalent generated build exercises it.
- Exercise CLI flags, config/state round trips, shell exit, tab close, tab
  restore, clipboard, selection, mouse reporting, and keyboard sequences in
  Linux. Run every visual or window lifecycle test with `DISPLAY` and
  `WAYLAND_DISPLAY` scrubbed and a private Xvfb display.
- Build and run in Taiji's isolated Plan 9 guest. Check direct `t9`, Rill's
  embedded terminal, process cleanup, and tab lifecycle there.
- Compare startup time, parser throughput, scroll throughput, memory use, and
  frame time against the recorded C baseline before removing the final C app
  modules. Fix a regression in the `.kry` path rather than retaining two
  implementations.
- Land reusable Kryon changes on its master first, then update Taiji's Kryon
  submodule pointer. Keep every vendored tree pristine and every submodule URL
  public HTTPS.

The rewrite is complete when no handwritten C code in t9 owns terminal
behavior, app state, or interface rendering; only the minimal native process
boundary remains where required by the operating systems.
