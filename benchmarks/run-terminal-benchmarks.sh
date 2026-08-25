#!/bin/sh
set -eu

if [ "${KAPSULE_BENCH_USE_REAL_DISPLAY:-0}" != "1" ] &&
   [ "${KAPSULE_BENCH_IN_VIRTUAL_DISPLAY:-0}" != "1" ]; then
    if command -v xvfb-run >/dev/null 2>&1; then
        wrapper_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
        wrapper_out_dir="${1:-$wrapper_root/benchmarks/results}"
        runtime_dir="${KAPSULE_BENCH_XDG_RUNTIME_DIR:-/tmp/ktrem-runtime}"
        mkdir -p "$runtime_dir"
        chmod 700 "$runtime_dir" 2>/dev/null || true
        set +e
        env XDG_RUNTIME_DIR="$runtime_dir" \
            KAPSULE_BENCH_IN_VIRTUAL_DISPLAY=1 \
            xvfb-run -a -s "-screen 0 1280x800x24" sh "$0" "$@"
        status=$?
        set -e
        if [ "$status" -ne 0 ]; then
            latest=$(ls -t "$wrapper_out_dir"/terminal-benchmarks-*.jsonl \
                2>/dev/null | head -n 1 || true)
            if [ -n "$latest" ] && grep -q '"payload":' "$latest" &&
               ! grep -q '"error":' "$latest"; then
                exit 0
            fi
        fi
        exit "$status"
    fi
    printf 'xvfb-run is required for isolated GUI benchmarks\n' >&2
    printf 'set KAPSULE_BENCH_USE_REAL_DISPLAY=1 to use the current display\n' >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
workload_script="$root/benchmarks/workload.sh"
out_dir="${1:-$root/benchmarks/results}"
ktrem_active_fps="${2:-}"
ktrem_pty_burst_ms="${3:-}"
bench_runs="${4:-${KAPSULE_BENCH_RUNS:-1}}"
selected_workloads="${5:-${KAPSULE_BENCH_WORKLOADS:-}}"
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
result_file="$out_dir/terminal-benchmarks-$timestamp.jsonl"
workloads="${selected_workloads:-startup ansi_flood unicode_table alternate_redraw dense_sgr wrap_reflow scrollback_flood cursor_matrix paste_burst hyperlink_grid search_corpus}"
terminals="${KAPSULE_BENCH_TERMINALS:-ktrem xfce4-terminal}"

arch=$(uname -m 2>/dev/null || printf unknown)
case "$arch" in
    amd64) arch=x86_64 ;;
esac
platform=$(uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]')
case "$platform" in
    linux) platform=linux ;;
    freebsd) platform=freebsd ;;
    darwin) platform=macos ;;
esac
ktrem_bin=${KAPSULE_BENCH_KAPSULE_BIN:-$root/build/$platform-$arch/bin/ktrem}
if [ ! -x "$ktrem_bin" ]; then
    ktrem_bin=$(command -v ktrem || true)
fi

mkdir -p "$out_dir"

now_ns() {
    date +%s%N
}

case "$bench_runs" in
    ''|*[!0-9]*)
        printf 'KAPSULE_BENCH_RUNS must be a positive integer\n' >&2
        exit 2
        ;;
esac
if [ "$bench_runs" -lt 1 ]; then
    printf 'KAPSULE_BENCH_RUNS must be a positive integer\n' >&2
    exit 2
fi

launch_terminal() {
    terminal="$1"
    command="$2"
    title="${3:-ktrem benchmark}"
    case "$terminal" in
        ktrem)
            if [ -n "$ktrem_active_fps" ] &&
               [ -n "$ktrem_pty_burst_ms" ]; then
                env KAPSULE_ACTIVE_FPS="$ktrem_active_fps" \
                    KAPSULE_PTY_BURST_MS="$ktrem_pty_burst_ms" \
                    "$ktrem_bin" --title "$title" --geometry 100x30 \
                    --command "$command" >/dev/null 2>&1 &
            elif [ -n "$ktrem_active_fps" ]; then
                env KAPSULE_ACTIVE_FPS="$ktrem_active_fps" \
                    "$ktrem_bin" --title "$title" --geometry 100x30 \
                    --command "$command" >/dev/null 2>&1 &
            elif [ -n "$ktrem_pty_burst_ms" ]; then
                env KAPSULE_PTY_BURST_MS="$ktrem_pty_burst_ms" \
                    "$ktrem_bin" --title "$title" --geometry 100x30 \
                    --command "$command" \
                    >/dev/null 2>&1 &
            else
                "$ktrem_bin" --title "$title" --geometry 100x30 \
                    --command "$command" >/dev/null 2>&1 &
            fi
            ;;
        xfce4-terminal)
            xfce4-terminal --disable-server --hide-menubar \
                --title="$title" --geometry=100x30 --command "$command" \
                >/dev/null 2>&1 &
            ;;
        *)
            return 2
            ;;
    esac
    printf '%s\n' "$!"
}

find_window_for_process() {
    pid="$1"
    title="$2"
    timeout_s="$3"
    waited=0

    while [ "$waited" -lt "$timeout_s" ]; do
        window=$(xdotool search --pid "$pid" 2>/dev/null | tail -n 1 || true)
        if [ -z "$window" ]; then
            window=$(xdotool search --name "$title" 2>/dev/null | tail -n 1 || true)
        fi
        if [ -z "$window" ]; then
            window=$(xdotool search --onlyvisible --name . 2>/dev/null | tail -n 1 || true)
        fi
        if [ -n "$window" ]; then
            printf '%s\n' "$window"
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    return 1
}

wait_for_result_fast() {
    file="$1"
    pid="$2"
    timeout_s="$3"
    waited=0
    max_wait=$((timeout_s * 20))

    while [ "$waited" -lt "$max_wait" ]; do
        if [ -s "$file" ]; then
            return 0
        fi
        if ! kill -0 "$pid" >/dev/null 2>&1; then
            if [ -s "$file" ]; then
                return 0
            fi
            return 2
        fi
        sleep 0.05
        waited=$((waited + 1))
    done
    return 1
}

run_live_resize_benchmark() {
    terminal="$1"
    run="$2"
    tmp="$3"
    title="ktrem-bench-live-resize-$terminal-$run-$$"
    ready="$tmp.ready"
    command="$workload_script live_resize_content $tmp"
    cycles=120
    i=0

    if ! command -v xdotool >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"live_resize","error":"xdotool_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        return 0
    fi
    rm -f "$tmp" "$ready"
    pid=$(launch_terminal "$terminal" "$command" "$title" || true)
    if [ -z "$pid" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"live_resize","error":"launch_failed"}\n' \
            "$terminal" "$run" >> "$result_file"
        return 0
    fi
    window=$(find_window_for_process "$pid" "$title" 30 || true)
    if [ -z "$window" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"live_resize","error":"window_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready"
        return 0
    fi
    wait_status=0
    wait_for_result "$ready" "$pid" 40 || wait_status=$?
    if [ "$wait_status" -ne 0 ]; then
        printf '{"terminal":"%s","run":%s,"workload":"live_resize","error":"content_not_ready"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready"
        return 0
    fi

    start_ns=$(now_ns)
    while [ "$i" -lt "$cycles" ]; do
        case $((i % 6)) in
            0) width=760; height=430 ;;
            1) width=1120; height=680 ;;
            2) width=900; height=520 ;;
            3) width=1240; height=760 ;;
            4) width=680; height=390 ;;
            *) width=1020; height=600 ;;
        esac
        if ! xdotool windowsize "$window" "$width" "$height" \
            >/dev/null 2>&1 ||
           ! xdotool getwindowgeometry "$window" >/dev/null 2>&1; then
            printf '{"terminal":"%s","run":%s,"workload":"live_resize","error":"resize_failed"}\n' \
                "$terminal" "$run" >> "$result_file"
            kill "$pid" >/dev/null 2>&1 || true
            wait "$pid" >/dev/null 2>&1 || true
            rm -f "$tmp" "$ready"
            return 0
        fi
        i=$((i + 1))
    done
    end_ns=$(now_ns)
    elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
    printf '{"terminal":"%s","run":%s,"launch_to_result_ms":%s,"payload":{"workload":"live_resize","elapsed_ms":%s,"lines":1200,"bytes":199200,"resizes":%s}}\n' \
        "$terminal" "$run" "$elapsed_ms" "$elapsed_ms" "$cycles" \
        >> "$result_file"
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" >/dev/null 2>&1 || true
    rm -f "$tmp" "$ready"
}

make_clipboard_payload() {
    payload="$1"
    marker="$2"
    lines=28
    i=1

    : > "$payload"
    while [ "$i" -le "$lines" ]; do
        {
            printf 'clip-%05d ' "$i"
            printf 'abcdefghijklmnopqrstuvwxyz0123456789 '
            printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 '
            printf 'symbols │┼─✓Λ測試 paste path benchmark\n'
        } >> "$payload"
        i=$((i + 1))
    done
    printf '%s\n' "$marker" >> "$payload"
}

start_clipboard_owner() {
    payload="$1"
    ready="$2"

    python3 -c '
import pathlib
import sys
import time
import tkinter as tk

payload = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
ready = pathlib.Path(sys.argv[2])
root = tk.Tk()
root.withdraw()
root.clipboard_clear()
root.clipboard_append(payload)
root.update()
ready.write_text("ready\n", encoding="utf-8")
deadline = time.time() + 60.0
while time.time() < deadline:
    root.update()
    time.sleep(0.05)
' "$payload" "$ready" >/dev/null 2>&1 &
    printf '%s\n' "$!"
}

send_paste_accelerator() {
    terminal="$1"
    accel="${KAPSULE_BENCH_PASTE_ACCEL:-terminal_default}"

    if [ "$accel" = "terminal_default" ]; then
        accel="ctrl_shift_v"
    fi
    case "$accel" in
        ctrl_shift_v)
            xdotool keydown Control_L keydown Shift_L keydown v sleep 0.08 \
                keyup v keyup Shift_L keyup Control_L
            ;;
        ctrl_shift_V)
            xdotool keydown Control_L keydown Shift_L keydown V sleep 0.08 \
                keyup V keyup Shift_L keyup Control_L
            ;;
        shift_insert)
            xdotool keydown Shift_L key Insert keyup Shift_L
            ;;
        *)
            return 2
            ;;
    esac
}

send_find_accelerator() {
    xdotool keydown Control_L keydown Shift_L keydown f sleep 0.08 \
        keyup f keyup Shift_L keyup Control_L
}

run_clipboard_paste_benchmark() {
    terminal="$1"
    run="$2"
    tmp="$3"
    title="ktrem-bench-clipboard-paste-$terminal-$run-$$"
    marker="KAPSULE_PASTE_DONE_${terminal}_${run}_$$"
    ready="$tmp.ready"
    payload="/tmp/ktrem-bench-clipboard-payload-$terminal-$run-$$.txt"
    owner_ready="/tmp/ktrem-bench-clipboard-owner-$terminal-$run-$$.ready"
    command=
    owner_pid=

    if ! command -v xdotool >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"xdotool_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        return 0
    fi
    if ! python3 -c 'import tkinter' >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"tkinter_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        return 0
    fi
    rm -f "$tmp" "$ready" "$payload" "$owner_ready"
    make_clipboard_payload "$payload" "$marker"
    bytes=$(wc -c < "$payload" | tr -d ' ')
    lines=$(wc -l < "$payload" | tr -d ' ')
    command="$workload_script clipboard_paste_target $tmp $marker $bytes"

    pid=$(launch_terminal "$terminal" "$command" "$title" || true)
    if [ -z "$pid" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"launch_failed"}\n' \
            "$terminal" "$run" >> "$result_file"
        rm -f "$tmp" "$ready" "$payload" "$owner_ready"
        return 0
    fi
    window=$(find_window_for_process "$pid" "$title" 30 || true)
    if [ -z "$window" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"window_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready" "$payload" "$owner_ready"
        return 0
    fi
    wait_status=0
    wait_for_result "$ready" "$pid" 30 || wait_status=$?
    if [ "$wait_status" -ne 0 ]; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"target_not_ready"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready" "$payload" "$owner_ready"
        return 0
    fi
    owner_pid=$(start_clipboard_owner "$payload" "$owner_ready")
    owner_wait=0
    while [ "$owner_wait" -lt 100 ] && [ ! -s "$owner_ready" ]; do
        sleep 0.05
        owner_wait=$((owner_wait + 1))
    done
    if [ ! -s "$owner_ready" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"clipboard_owner_not_ready"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$owner_pid" >/dev/null 2>&1 || true
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready" "$payload" "$owner_ready"
        return 0
    fi

    xdotool windowfocus "$window" >/dev/null 2>&1 || true
    xdotool mousemove --window "$window" 40 40 click 1 >/dev/null 2>&1 || true
    sleep 0.2
    start_ns=$(now_ns)
    if ! send_paste_accelerator "$terminal" >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"paste_key_failed"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$owner_pid" >/dev/null 2>&1 || true
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready" "$payload" "$owner_ready"
        return 0
    fi
    wait_status=0
    wait_for_result_fast "$tmp" "$pid" 60 || wait_status=$?
    if [ "$wait_status" -eq 0 ]; then
        received=$(sed -n 's/^.*"bytes": \([0-9][0-9]*\).*$/\1/p' "$tmp" | head -n 1)
        if [ "$received" = "$bytes" ]; then
            end_ns=$(now_ns)
            elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
            printf '{"terminal":"%s","run":%s,"launch_to_result_ms":%s,"payload":{"workload":"clipboard_paste","elapsed_ms":%s,"lines":%s,"bytes":%s}}\n' \
                "$terminal" "$run" "$elapsed_ms" "$elapsed_ms" "$lines" "$bytes" \
                >> "$result_file"
        else
            printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"partial","received_bytes":%s,"expected_bytes":%s}\n' \
                "$terminal" "$run" "${received:-0}" "$bytes" >> "$result_file"
        fi
    elif [ "$wait_status" -eq 2 ]; then
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"exited"}\n' \
            "$terminal" "$run" >> "$result_file"
    else
        printf '{"terminal":"%s","run":%s,"workload":"clipboard_paste","error":"timeout"}\n' \
            "$terminal" "$run" >> "$result_file"
    fi
    kill "$owner_pid" >/dev/null 2>&1 || true
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" >/dev/null 2>&1 || true
    rm -f "$tmp" "$ready" "$payload" "$owner_ready"
}

run_find_dialog_benchmark() {
    terminal="$1"
    run="$2"
    tmp="$3"
    title="ktrem-bench-find-dialog-$terminal-$run-$$"
    marker="ktremfinddone"
    needle="${KAPSULE_BENCH_FIND_NEEDLE:-needle-critical}"
    ready="$tmp.ready"
    command="$workload_script find_dialog_target $tmp $marker $needle"

    if ! command -v xdotool >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"xdotool_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        return 0
    fi
    rm -f "$tmp" "$ready"
    pid=$(launch_terminal "$terminal" "$command" "$title" || true)
    if [ -z "$pid" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"launch_failed"}\n' \
            "$terminal" "$run" >> "$result_file"
        rm -f "$tmp" "$ready"
        return 0
    fi
    window=$(find_window_for_process "$pid" "$title" 30 || true)
    if [ -z "$window" ]; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"window_not_found"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready"
        return 0
    fi
    wait_status=0
    wait_for_result "$ready" "$pid" 40 || wait_status=$?
    if [ "$wait_status" -ne 0 ]; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"content_not_ready"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready"
        return 0
    fi

    xdotool windowfocus "$window" >/dev/null 2>&1 || true
    xdotool mousemove --window "$window" 40 40 click 1 >/dev/null 2>&1 || true
    sleep 0.2
    start_ns=$(now_ns)
    if ! send_find_accelerator >/dev/null 2>&1 ||
       ! xdotool type --delay 1 "$needle" >/dev/null 2>&1 ||
       ! xdotool key Return sleep 0.15 >/dev/null 2>&1 ||
       ! xdotool keydown Escape sleep 0.08 keyup Escape sleep 0.15 >/dev/null 2>&1 ||
       ! xdotool type --delay 1 "$marker" >/dev/null 2>&1 ||
       ! xdotool key Return >/dev/null 2>&1; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"find_key_failed"}\n' \
            "$terminal" "$run" >> "$result_file"
        kill "$pid" >/dev/null 2>&1 || true
        wait "$pid" >/dev/null 2>&1 || true
        rm -f "$tmp" "$ready"
        return 0
    fi
    wait_status=0
    wait_for_result_fast "$tmp" "$pid" 60 || wait_status=$?
    if [ "$wait_status" -eq 0 ]; then
        matched=$(sed -n 's/^.*"matched": \(true\|false\).*$/\1/p' "$tmp" | head -n 1)
        received=$(sed -n 's/^.*"bytes": \([0-9][0-9]*\).*$/\1/p' "$tmp" | head -n 1)
        if [ "$matched" = "true" ]; then
            end_ns=$(now_ns)
            elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
            printf '{"terminal":"%s","run":%s,"launch_to_result_ms":%s,"payload":{"workload":"find_dialog","elapsed_ms":%s,"lines":18000,"bytes":1746000}}\n' \
                "$terminal" "$run" "$elapsed_ms" "$elapsed_ms" >> "$result_file"
        else
            tail_hex=$(sed -n 's/^.*"tail_hex": "\([0-9a-f]*\)".*$/\1/p' "$tmp" | head -n 1)
            marker_hex=$(sed -n 's/^.*"marker_hex": "\([0-9a-f]*\)".*$/\1/p' "$tmp" | head -n 1)
            printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"marker_not_received","received_bytes":%s,"marker_hex":"%s","tail_hex":"%s"}\n' \
                "$terminal" "$run" "${received:-0}" "$marker_hex" "$tail_hex" >> "$result_file"
        fi
    elif [ "$wait_status" -eq 2 ]; then
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"exited"}\n' \
            "$terminal" "$run" >> "$result_file"
    else
        printf '{"terminal":"%s","run":%s,"workload":"find_dialog","error":"timeout"}\n' \
            "$terminal" "$run" >> "$result_file"
    fi
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" >/dev/null 2>&1 || true
    rm -f "$tmp" "$ready"
}

wait_for_result() {
    file="$1"
    pid="$2"
    timeout_s="$3"
    waited=0
    while [ "$waited" -lt "$timeout_s" ]; do
        if [ -s "$file" ]; then
            return 0
        fi
        if ! kill -0 "$pid" >/dev/null 2>&1; then
            if [ -s "$file" ]; then
                return 0
            fi
            return 2
        fi
        sleep 1
        waited=$((waited + 1))
    done
    return 1
}

printf '# ktrem terminal benchmark run %s\n' "$timestamp" > "$result_file"

for terminal in $terminals; do
    if [ "$terminal" = ktrem ]; then
        if [ -z "$ktrem_bin" ] || [ ! -x "$ktrem_bin" ]; then
            printf '{"terminal":"%s","error":"not_found"}\n' "$terminal" \
                >> "$result_file"
            continue
        fi
    elif ! command -v "$terminal" >/dev/null 2>&1; then
        printf '{"terminal":"%s","error":"not_found"}\n' "$terminal" \
            >> "$result_file"
        continue
    fi
    run=1
    while [ "$run" -le "$bench_runs" ]; do
        for workload in $workloads; do
            tmp="/tmp/ktrem-bench-$terminal-$workload-$run-$$.json"
            rm -f "$tmp"
            if [ "$workload" = "live_resize" ]; then
                run_live_resize_benchmark "$terminal" "$run" "$tmp"
                sleep 1
                continue
            fi
            if [ "$workload" = "clipboard_paste" ]; then
                run_clipboard_paste_benchmark "$terminal" "$run" "$tmp"
                sleep 1
                continue
            fi
            if [ "$workload" = "find_dialog" ]; then
                run_find_dialog_benchmark "$terminal" "$run" "$tmp"
                sleep 1
                continue
            fi
            command="$workload_script $workload $tmp"
            launch_start=$(now_ns)
            pid=$(launch_terminal "$terminal" "$command" || true)
            if [ -z "$pid" ]; then
                printf '{"terminal":"%s","run":%s,"workload":"%s","error":"launch_failed"}\n' \
                    "$terminal" "$run" "$workload" >> "$result_file"
                continue
            fi
            wait_status=0
            wait_for_result "$tmp" "$pid" 60 || wait_status=$?
            if [ "$wait_status" -eq 0 ]; then
                launch_end=$(now_ns)
                launch_ms=$(( (launch_end - launch_start) / 1000000 ))
                payload=$(cat "$tmp")
                printf '{"terminal":"%s","run":%s,"launch_to_result_ms":%s,"payload":%s}\n' \
                    "$terminal" "$run" "$launch_ms" "$payload" >> "$result_file"
            elif [ "$wait_status" -eq 2 ]; then
                printf '{"terminal":"%s","run":%s,"workload":"%s","error":"exited"}\n' \
                    "$terminal" "$run" "$workload" >> "$result_file"
            else
                printf '{"terminal":"%s","run":%s,"workload":"%s","error":"timeout"}\n' \
                    "$terminal" "$run" "$workload" >> "$result_file"
            fi
            kill "$pid" >/dev/null 2>&1 || true
            wait "$pid" >/dev/null 2>&1 || true
            rm -f "$tmp"
            sleep 1
        done
        run=$((run + 1))
    done
done

summary_tmp="/tmp/ktrem-bench-summary-$$.jsonl"
awk '
    /^[{]/ && /"payload":/ {
        terminal = $0
        workload = $0
        elapsed = $0
        sub(/^.*"terminal":"?/, "", terminal)
        sub(/".*$/, "", terminal)
        sub(/^.*"workload":"?/, "", workload)
        sub(/".*$/, "", workload)
        sub(/^.*"elapsed_ms":/, "", elapsed)
        sub(/[,}].*$/, "", elapsed)
        elapsed += 0
        key = terminal SUBSEP workload
        count[key]++
        total[key] += elapsed
        if(!(key in min) || elapsed < min[key])
            min[key] = elapsed
        if(!(key in max) || elapsed > max[key])
            max[key] = elapsed
    }
    END {
        for(key in count) {
            split(key, parts, SUBSEP)
            printf("{\"summary\":true,\"terminal\":\"%s\",\"workload\":\"%s\",", parts[1], parts[2])
            printf("\"runs\":%d,\"min_elapsed_ms\":%d,", count[key], min[key])
            printf("\"avg_elapsed_ms\":%.1f,\"max_elapsed_ms\":%d}\n", total[key] / count[key], max[key])
        }
    }
' "$result_file" > "$summary_tmp"
cat "$summary_tmp" >> "$result_file"
rm -f "$summary_tmp"

printf '%s\n' "$result_file"
