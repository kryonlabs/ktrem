#!/bin/sh
set -eu

workload="${1:-startup}"
out="${2:-/tmp/ktrem-bench-result.json}"

now_ns() {
    date +%s%N
}

json_result() {
    name="$1"
    start_ns="$2"
    end_ns="$3"
    lines="$4"
    bytes="$5"
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    printf '{"workload":"%s","elapsed_ms":%s,"lines":%s,"bytes":%s}\n' \
        "$name" "$elapsed_ms" "$lines" "$bytes" > "$out"
}

startup() {
    start_ns=$(now_ns)
    printf 'ktrem benchmark startup\n'
    end_ns=$(now_ns)
    json_result "startup" "$start_ns" "$end_ns" 1 26
}

ansi_flood() {
    start_ns=$(now_ns)
    lines=18000
    i=1
    while [ "$i" -le "$lines" ]; do
        color=$((30 + (i % 8)))
        bright=$((90 + (i % 8)))
        printf '\033[%sm%06d\033[0m ' "$color" "$i"
        printf '\033[%smcolored terminal throughput sample\033[0m ' "$bright"
        printf 'abcdefghijklmnopqrstuvwxyz 0123456789\n'
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "ansi_flood" "$start_ns" "$end_ns" "$lines" 1422000
}

unicode_table() {
    start_ns=$(now_ns)
    lines=6000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf '│ %06d │ Kryon Λambda │ ktrem ✓ │ width 測試 │ box ──┼── │\n' "$i"
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "unicode_table" "$start_ns" "$end_ns" "$lines" 456000
}

alternate_redraw() {
    start_ns=$(now_ns)
    frames=180
    frame=1
    printf '\033[?1049h'
    while [ "$frame" -le "$frames" ]; do
        printf '\033[H'
        row=1
        while [ "$row" -le 28 ]; do
            printf 'frame %03d row %02d ' "$frame" "$row"
            printf '▁▂▃▄▅▆▇█ kryon terminal redraw path █▇▆▅▄▃▂▁\n'
            row=$((row + 1))
        done
        frame=$((frame + 1))
    done
    printf '\033[?1049l'
    end_ns=$(now_ns)
    json_result "alternate_redraw" "$start_ns" "$end_ns" "$((frames * 28))" 430000
}

dense_sgr() {
    start_ns=$(now_ns)
    lines=9000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf '\033[1;3%dm%05d\033[0m' "$((i % 8))" "$i"
        printf ' \033[4;38;5;%smunder\033[0m' "$((16 + (i % 216)))"
        printf ' \033[48;5;%sm block \033[0m' "$((232 + (i % 24)))"
        printf ' \033[3mitalic\033[0m \033[9mstrike\033[0m \033[53mover\033[55m'
        printf ' truecolor \033[38;2;%d;%d;%dmrgb\033[0m\n' \
            "$((i % 255))" "$(((i * 3) % 255))" "$(((i * 7) % 255))"
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "dense_sgr" "$start_ns" "$end_ns" "$lines" 1530000
}

wrap_reflow() {
    start_ns=$(now_ns)
    lines=7000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf 'wrap-%05d ' "$i"
        printf 'abcdefghijklmnopqrstuvwxyz0123456789'
        printf ' / terminal reflow candidate / '
        printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
        printf ' / kryon ktrem xfce compatibility width sample '
        printf '測試✓Λ │ ──┼── end\n'
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "wrap_reflow" "$start_ns" "$end_ns" "$lines" 1050000
}

scrollback_flood() {
    start_ns=$(now_ns)
    lines=50000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf 'scrollback %05d abcdefghijklmnopqrstuvwxyz 0123456789 ktrem Kryon\n' "$i"
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "scrollback_flood" "$start_ns" "$end_ns" "$lines" 3450000
}

cursor_matrix() {
    start_ns=$(now_ns)
    frames=240
    frame=1
    printf '\033[?1049h\033[2J'
    while [ "$frame" -le "$frames" ]; do
        cell=1
        while [ "$cell" -le 120 ]; do
            row=$((1 + ((cell + frame) % 28)))
            col=$((1 + (((cell * 7) + frame) % 84)))
            color=$((31 + ((cell + frame) % 7)))
            printf '\033[%s;%sH\033[%sm%02x\033[0m' \
                "$row" "$col" "$color" "$((cell % 255))"
            cell=$((cell + 1))
        done
        frame=$((frame + 1))
    done
    printf '\033[?1049l'
    end_ns=$(now_ns)
    json_result "cursor_matrix" "$start_ns" "$end_ns" "$((frames * 120))" 720000
}

paste_burst() {
    start_ns=$(now_ns)
    lines=12000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf 'paste-%05d ' "$i"
        printf 'abcdefghijklmnopqrstuvwxyz0123456789'
        printf ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
        printf ' symbols │┼─✓Λ測試 '
        printf 'repeated-paste-payload-for-terminal-input-path\n'
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "paste_burst" "$start_ns" "$end_ns" "$lines" 1440000
}

hyperlink_grid() {
    start_ns=$(now_ns)
    lines=7000
    i=1
    while [ "$i" -le "$lines" ]; do
        printf 'link-%05d ' "$i"
        printf '\033]8;id=item-%05d;https://ktrem.kryonlabs.com/item/%05d\033\\' "$i" "$i"
        printf 'ktrem benchmark link %05d' "$i"
        printf '\033]8;;\033\\'
        printf ' status=%03d path=/tmp/ktrem/%05d\n' "$((i % 200))" "$i"
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "hyperlink_grid" "$start_ns" "$end_ns" "$lines" 980000
}

search_corpus() {
    start_ns=$(now_ns)
    lines=18000
    i=1
    while [ "$i" -le "$lines" ]; do
        case $((i % 9)) in
            0) marker='needle-critical' ;;
            1) marker='NeedleMixed' ;;
            *) marker='background' ;;
        esac
        printf 'search-%05d %s package=core module=terminal result=%05d text=abcdefghijklmnopqrstuvwxyz\n' \
            "$i" "$marker" "$((i * 17))"
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    json_result "search_corpus" "$start_ns" "$end_ns" "$lines" 1746000
}

live_resize_content() {
    lines=1200
    i=1
    while [ "$i" -le "$lines" ]; do
        printf 'resize-%05d ' "$i"
        printf 'abcdefghijklmnopqrstuvwxyz0123456789'
        printf ' / live terminal resize content / '
        printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
        printf ' / wrapped wide glyphs 測試✓Λ │ ──┼── '
        printf 'blocks ▁▂▃▄▅▆▇█ end\n'
        i=$((i + 1))
    done
    printf '\033[H'
    printf 'live resize content ready; benchmark harness owns window resizing\n'
    printf 'ready\n' > "$out.ready"
    sleep 45
}

clipboard_paste_target() {
    expected_bytes="${4:-0}"

    python3 -c '
import json
import os
import select
import sys
import termios
import tty

out = sys.argv[1]
expected = int(sys.argv[2])
fd = sys.stdin.fileno()
old = termios.tcgetattr(fd)
received = 0

try:
    tty.setraw(fd)
    with open(out + ".ready", "w", encoding="utf-8") as ready:
        ready.write("ready\n")
    while received < expected:
        readable, _, _ = select.select([fd], [], [], 10.0)
        if not readable:
            break
        chunk = os.read(fd, min(4096, expected - received))
        if not chunk:
            break
        received += len(chunk)
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old)

with open(out, "w", encoding="utf-8") as result:
    result.write(json.dumps({
        "workload": "clipboard_paste_target",
        "bytes": received,
        "expected_bytes": expected
    }) + "\n")
' "$out" "$expected_bytes"
    sleep 1
}

find_dialog_target() {
    marker="${3:-KTREM_FIND_DONE}"
    needle="${4:-needle-critical}"
    lines=18000
    i=1

    while [ "$i" -le "$lines" ]; do
        case $((i % 9)) in
            0) text="$needle" ;;
            1) text="NeedleMixed" ;;
            *) text="background" ;;
        esac
        printf 'find-%05d %s package=core module=terminal result=%05d text=abcdefghijklmnopqrstuvwxyz\n' \
            "$i" "$text" "$((i * 17))"
        i=$((i + 1))
    done
    printf 'find dialog target ready; open find UI and type marker when done\n'

    python3 -c '
import json
import os
import select
import sys
import termios
import tty

out = sys.argv[1]
marker = sys.argv[2].encode("utf-8")
fd = sys.stdin.fileno()
old = termios.tcgetattr(fd)
buf = b""
matched = False

try:
    tty.setraw(fd)
    with open(out + ".ready", "w", encoding="utf-8") as ready:
        ready.write("ready\n")
    while len(buf) < 4096:
        readable, _, _ = select.select([fd], [], [], 20.0)
        if not readable:
            break
        chunk = os.read(fd, 256)
        if not chunk:
            break
        buf += chunk
        printable = bytes(b for b in buf if 32 <= b <= 126)
        if marker in printable:
            matched = True
            break
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old)

with open(out, "w", encoding="utf-8") as result:
    result.write(json.dumps({
        "workload": "find_dialog",
        "matched": matched,
        "bytes": len(buf),
        "marker_hex": marker.hex(),
        "tail_hex": buf[-80:].hex()
    }) + "\n")
' "$out" "$marker"
    sleep 1
}

case "$workload" in
    startup) startup ;;
    ansi_flood) ansi_flood ;;
    unicode_table) unicode_table ;;
    alternate_redraw) alternate_redraw ;;
    dense_sgr) dense_sgr ;;
    wrap_reflow) wrap_reflow ;;
    scrollback_flood) scrollback_flood ;;
    cursor_matrix) cursor_matrix ;;
    paste_burst) paste_burst ;;
    hyperlink_grid) hyperlink_grid ;;
    search_corpus) search_corpus ;;
    live_resize_content) live_resize_content ;;
    clipboard_paste_target) clipboard_paste_target "$@" ;;
    find_dialog_target) find_dialog_target "$@" ;;
    *)
        printf 'unknown workload: %s\n' "$workload" >&2
        exit 2
        ;;
esac
