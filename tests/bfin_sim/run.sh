#!/bin/sh
# Run the compiled Blackfin engine in the instruction-set simulator and compare its audio with the host build.
#   KENNEL=.../ext/kennel BFIN_RUN=/path/to/sim/bfin/run PATH=/path/to/bfin/bin:$PATH sh tests/bfin_sim/run.sh
# BFIN_RUN: binutils-gdb's Blackfin simulator (configure --target=bfin-elf --enable-sim; make all-sim -> sim/bfin/run).
# Prints Blackfin cycles per voice block (mid / max / zero knobs) and whether the audio is bit-identical.
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
K=${KENNEL:?set KENNEL}/synth_sdk
RUN=${BFIN_RUN:?set BFIN_RUN to the simulator}
P=$ROOT/plugin/hollow_saint
B=$ROOT/build/sim
mkdir -p "$B"
F="-O2 -mcpu=bf524 -ffreestanding -fno-builtin -nostdlib -fno-common -fno-tree-loop-distribute-patterns"   # = pack.py
M="--environment virtual --model bf524 --memory-region 0x0,0x4000000 --memory-region 0xFF800000,0x8000"
bfin-elf-gcc $F -I "$K" -I "$P" -c "$P/hollow_saint.c" -o "$B/hs.o"
bfin-elf-gcc $F -c "$HERE/start.S" -o "$B/start.o"
for mode in dump cycles; do
    D=""; [ $mode = cycles ] && D=-DSIM_CYCLES
    bfin-elf-gcc $F $D -DENGINE=ENGINE_hollow_saint -I "$K" -I "$HERE" -c "$HERE/sim_main.c" -o "$B/main_$mode.o"
    bfin-elf-gcc $F -T "$HERE/sim.ld" "$B/start.o" "$B/main_$mode.o" "$B/hs.o" -lgcc -o "$B/hs_$mode.elf" 2>/dev/null
done
"$RUN" $M "$B/hs_cycles.elf"
"$RUN" $M "$B/hs_dump.elf" > "$B/audio_bfin.raw"
cc -O2 -DOP1_HOST -DENGINE=ENGINE_hollow_saint -I "$ROOT/tests/host_sdk" -I "$K" -I "$P" -I "$HERE" \
   "$HERE/sim_main.c" "$P/hollow_saint.c" -o "$B/host_dump"
"$B/host_dump" > "$B/audio_host.raw"
cmp "$B/audio_bfin.raw" "$B/audio_host.raw" && echo "audio: Blackfin == host, bit for bit ($(wc -c < "$B/audio_host.raw") bytes)"
