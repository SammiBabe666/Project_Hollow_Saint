#!/bin/sh
# The Opus 2 build's compiled code in the Blackfin simulator: cycles, and its audio against the kennel host build's.
#   KENNEL=.../ext/kennel OPUS_KIT=.../opus/plugin-kit BFIN_RUN=/path/to/sim/bfin/run PATH=/path/to/bfin/bin:$PATH \
#       sh tests/bfin_sim/run_opus.sh
# Compiled with the Opus kit's own flags (tools/op1mod/build.py CFLAGS, -fno-common), as plugin_tool.py build does.
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
K=${KENNEL:?set KENNEL}/synth_sdk
KIT=${OPUS_KIT:?set OPUS_KIT}
RUN=${BFIN_RUN:?set BFIN_RUN to the simulator}
O=$ROOT/opus/sammi.hollow_saint
B=$ROOT/build/sim_opus
mkdir -p "$B"
F="-mcpu=bf524-any -O2 -ffreestanding -fno-builtin -Wall -Wextra -Werror -fno-common"   # = the Opus kit's
M="--environment virtual --model bf524 --memory-region 0x0,0x4000000 --memory-region 0xFF800000,0x8000"
bfin-elf-gcc $F -c "$HERE/start.S" -o "$B/start.o"
for mode in dump cycles; do
    D=""; [ $mode = cycles ] && D=-DSIM_CYCLES
    bfin-elf-gcc $F $D -I "$O" -I "$KIT/sdk" -I "$HERE" -c "$HERE/opus_sim.c" -o "$B/main_$mode.o"
    bfin-elf-gcc $F -nostdlib -Wl,--no-warn-rwx-segments -T "$HERE/sim.ld" "$B/start.o" "$B/main_$mode.o" -lgcc -o "$B/hs_$mode.elf"
done
"$RUN" $M "$B/hs_cycles.elf"
"$RUN" $M "$B/hs_dump.elf" > "$B/audio_bfin_opus.raw"
cc -O2 -DOP1_HOST -DENGINE=ENGINE_hollow_saint -I "$ROOT/tests/host_sdk" -I "$K" -I "$ROOT/plugin/hollow_saint" -I "$HERE" \
   "$HERE/sim_main.c" "$ROOT/plugin/hollow_saint/hollow_saint.c" -o "$B/kennel_host"
"$B/kennel_host" > "$B/audio_kennel_host.raw"
cmp "$B/audio_bfin_opus.raw" "$B/audio_kennel_host.raw" && echo "audio: Opus build on the Blackfin == kennel host build, bit for bit ($(wc -c < "$B/audio_bfin_opus.raw") bytes)"
