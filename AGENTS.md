# AGENTS.md - ktrem

ktrem is a standalone terminal application built on Kryon.

- Keep terminal emulator code in this repository.
- Do not add ktrem product code, parser behavior, terminal UI, or app state to
  `../kryon`.
- Add code to Kryon only when it is a small reusable primitive that more than
  one project needs.
- Use direct names for ktrem APIs and types: `Terminal`, `Cell`, `Session`,
  `Palette`. Do not add artificial `Kry*` prefixes.

