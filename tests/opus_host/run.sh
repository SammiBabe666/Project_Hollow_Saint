#!/bin/sh
# The Opus 2 build on the PC (ASan + UBSan): its checks, and its audio against the kennel build's.
#   KENNEL=.../ext/kennel OPUS_KIT=.../opus/plugin-kit sh tests/opus_host/run.sh
# The kennel side is tests/bfin_sim/sim_main.c's host build (the kennel SDK's API); the Opus side is
# tests/opus_host/opus_main.c through the Opus engine struct. Same workload, so the two raw outputs must be identical.
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
K=${KENNEL:?set KENNEL}/synth_sdk
KIT=${OPUS_KIT:?set OPUS_KIT to the Opus plugin kit (github.com/LizardMan245/opus, plugin-kit)}
B=$ROOT/build/opus_host
mkdir -p "$B"
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
W="-Wall -Wextra -Werror"
python3 "$ROOT/tools/gen_opus_headers.py" --check
cc $SAN $W -DOP1_HOST -DOPUS_HOST -I "$ROOT/opus/sammi.hollow_saint" -I "$KIT/sdk" \
   "$HERE/opus_main.c" -o "$B/opus_host"
cc $SAN -DOP1_HOST -DENGINE=ENGINE_hollow_saint -I "$ROOT/tests/host_sdk" -I "$K" -I "$ROOT/plugin/hollow_saint" \
   -I "$ROOT/tests/bfin_sim" "$ROOT/tests/bfin_sim/sim_main.c" "$ROOT/plugin/hollow_saint/hollow_saint.c" -lm -o "$B/kennel_host"
"$B/opus_host" checks
"$B/opus_host" audio > "$B/audio_opus.raw"
"$B/kennel_host" > "$B/audio_kennel.raw"
cmp "$B/audio_opus.raw" "$B/audio_kennel.raw"
echo "audio: Opus build == kennel build, bit for bit ($(wc -c < "$B/audio_opus.raw") bytes)"
