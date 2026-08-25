# ktrem Remaining Feature Plan

ktrem is currently faster than xfce4-terminal on the saved synthetic and
externally driven Xvfb benchmark set. The full xfce4-terminal alternative goal
is still open until the feature and release-readiness gaps below are complete.

## Current Measured Status

Saved benchmark results show ktrem ahead on average for:

- Startup payload, ANSI flood, Unicode output, alternate-screen redraw, dense
  SGR styling, wrapped output, scrollback flood, and cursor matrix updates.
- Paste-like output, OSC 8 hyperlink-heavy output, and search-corpus output.
- External live resize, external clipboard paste, and external find workflow.

The benchmark files are in `benchmarks/results/`. These are directional Xvfb
benchmarks, not a substitute for broader real-desktop validation.

## Must Finish Before Calling ktrem a Full Alternative

### 1. Release And Installability

- Keep GitHub Actions green for `make test` and `make all` from a clean
  checkout.
- Publish Linux release assets from GitHub Releases.
- Add a `.desktop` file, app icon install target, and documented install path.
- Decide whether ktrem should advertise a custom `TERM` and ship terminfo, or
  keep xterm-compatible defaults.
- Add changelog/version automation before the first numbered release.

### 2. Xfce Launch And Process Parity

- Implement real multi-window launch semantics for `--window`.
- Add single-instance command routing equivalent to xfce4-terminal server
  behavior, while keeping `--disable-server` as an opt-out.
- Make repeated `--drop-down` invocation toggle an existing drop-down window
  instead of launching another instance.
- Preserve working directory, title, hold, geometry, and command specs across
  mixed `--tab` and `--window` launch requests.

### 3. Preferences And Profiles

- Add a complete preferences UI for font, colors, scrollback, cursor, command,
  working directory, shortcuts, and behavior toggles.
- Add import/export or profile files that can be copied between machines.
- Add shortcut remapping while keeping terminal-standard defaults.

### 4. Glyph And Font Coverage

- Finish fontconfig-backed fallback for missing glyphs beyond the current
  static fallback paths.
- Verify box drawing, blocks, Powerline, Nerd Font symbols, Greek, Cyrillic,
  emoji samples, and common CJK on a clean Linux install.
- Add a glyph smoke workload that screenshots representative cells and fails on
  tofu/missing-glyph rendering.

### 5. Terminal Compatibility

- Keep expanding parser/input coverage with real applications: `vim`, `nano`,
  `less`, `htop`, `tmux`, `ssh`, `git`, package managers, Python/Node/Rust test
  output, curses dashboards, and sixel tools.
- Add automated scenario tests for bracketed paste, OSC 52, OSC 8, mouse modes,
  modifyOtherKeys, keypad/application cursor modes, and title/color stacks.
- Validate close behavior with stubborn child processes and held sessions.

### 6. Real-Desktop Benchmark Validation

- Repeat the Xvfb benchmark suite on a real desktop without killing or
  controlling the user's active xfce4-terminal.
- Add memory/RSS measurements for large scrollback and sixel workloads.
- Add startup cold/warm measurements from an installed binary.
- Keep benchmark docs honest: measured categories only, no broad claims from
  narrow tests.

### 7. Kryon Renderer Direction

Kryon's current raylib/SDL backend is good enough for the measured parity set.
The next major performance step should be a Kryon terminal-grid primitive, not
a ktrem-only renderer and not a move away from Kryon.

The renderer should use:

- Persistent glyph atlas and cached glyph metrics.
- Retained row/cell state with dirty-region damage.
- Batched quads for glyphs, backgrounds, underlines, cursor, and selections.
- A backend-neutral Kryon API first, with the current raylib/SDL backend as the
  first implementation.
- A future `wgpu-native`/WebGPU backend only after the abstraction is proven.

This keeps ktrem as the showcase for Kryon while improving Kryon for other
applications too.
