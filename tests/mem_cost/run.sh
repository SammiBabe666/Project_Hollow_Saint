#!/bin/sh
# The OP-1's memory cost, modelled on the PC: Opus's kit says each 32-byte line of ordinary memory an audio block
# touches costs ~107 cycles (twice that while the display sends a frame). This renders a six-voice chord at a few knob
# settings under valgrind's lackey, counts the distinct data and code lines each voice render and each audio block
# (the six voices) touch, and prints them with the 107-cycle model, plus x86 instructions a voice block (callgrind).
#   sh tests/mem_cost/run.sh [engine.c]          (default: plugin/hollow_saint/hollow_saint.c; needs valgrind)
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC=${1:-$ROOT/plugin/hollow_saint/hollow_saint.c}
B=$ROOT/build/mem_cost
mkdir -p "$B"
cc -O2 -g -w -no-pie -fno-pie -DOP1_HOST -I "$ROOT/tests/host_sdk" -I "$ROOT/tests/host_shim" -I "$ROOT/plugin/hollow_saint" \
   "$HERE/probe.c" "$SRC" -lm -o "$B/probe"
nm -S "$B/probe" | grep -E " [bBdDrR] " > "$B/nm.txt"
for set in "16384 16384 12000 0" "16384 16384 12000 16384" "16384 16384 12000 20000" "0 0 0 20000" "32767 32767 32767 32767"; do
    valgrind --tool=lackey --trace-mem=yes --log-file="$B/trace.txt" "$B/probe" $set 12 >/dev/null 2>&1
    valgrind --tool=callgrind --toggle-collect=hs_render --callgrind-out-file="$B/cg.out" "$B/probe" $set 60 >/dev/null 2>&1
    ins=$(grep -E '^(summary|totals):' "$B/cg.out" | head -1 | awk '{print $2}')
    echo "== knobs $set: x86 instructions a voice block $((ins / 360))"
    python3 -I "$HERE/lines.py" "$B/trace.txt" "$B/nm.txt" 4
done
rm -f "$B/trace.txt"
