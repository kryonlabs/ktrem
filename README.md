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
```

Config is read from `$XDG_CONFIG_HOME/kapsule/config` or
`~/.config/kapsule/config`:

```ini
font_size=16
padding=10
scrollback=5000
shell=/bin/bash
working_directory=/home/me/project
```

Shortcuts:

- `Ctrl+Shift+T`: new tab
- `Ctrl+Shift+W`: close tab
- `Ctrl+Tab`: next tab
- `Ctrl+Shift+C`: copy selection
- `Ctrl+Shift+V`: paste
- `Shift+PageUp` / `Shift+PageDown`: scroll

