#!/bin/sh
# Build the Opus 2 plugin (opus/sammi.hollow_saint -> hollow_saint.opmod) with the Opus plugin kit's own tool, then
# read it back as the core will and check its screen art:
#
#   OPUS_KIT=/path/to/opus/plugin-kit PATH=/path/to/bfin/bin:$PATH sh tools/build_opmod.sh [OUT_DIR]
#
# OPUS_KIT: the plugin-kit folder of https://github.com/LizardMan245/opus. The compiler is found as the kit finds it
# (OP1_BFIN_TOOLCHAIN, a bfin-toolchain folder beside the kit, or bfin-elf-gcc on PATH). The kennel build
# (tools/build_knl.sh) is separate and unchanged: both build the same plugin/hollow_saint/hollow_saint.c.
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
KIT=${OPUS_KIT:?set OPUS_KIT to the Opus plugin kit (github.com/LizardMan245/opus, plugin-kit)}
OUT=${1:-$ROOT/build/opus}
mkdir -p "$OUT"
python3 "$ROOT/tools/gen_opus_headers.py" --check
python3 "$KIT/plugin_tool.py" build "$ROOT/opus/sammi.hollow_saint" --out "$OUT/hollow_saint.opmod"
python3 "$KIT/plugin_tool.py" show "$OUT/hollow_saint.opmod"
python3 "$KIT/plugin_tool.py" svg "$ROOT/plugin/hollow_saint/hollow_saint.svg" | head -1
ls -l "$OUT/hollow_saint.opmod"
sha256sum "$OUT/hollow_saint.opmod"
