#!/bin/sh
# One command from source to an installable, checked release:
#   1. host tests (ASan + UBSan, firmware simulation; the 16 x 16 products' operand ranges are checked on every call)
#   2. the compiled Blackfin code in the instruction-set simulator: cycles, and audio bit-identical to the host build
#   3. pack the .knl with the SDK's packer and check it with both loaders (tools/build_knl.sh)
#   4. copy it to release/ with its sha256, and the presets folder
#
#   KENNEL=/path/to/hundekraft-3.0.3-source/ext/kennel BFIN_RUN=/path/to/sim/bfin/run PATH=/path/to/bfin/bin:$PATH \
#       sh tools/release.sh
# Stops at the first failure. BFIN_RUN unset: step 2 is skipped (and says so).
#
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${KENNEL:?set KENNEL to the kennel folder of the matching SDK bundle}"
export KENNEL
cd "$ROOT"

echo "== 1. host tests"
python3 tests/host.py phrase
python3 tests/host.py suite

echo "== 2. Blackfin simulator"
if [ -n "${BFIN_RUN:-}" ]; then
    sh tests/bfin_sim/run.sh
else
    echo "skipped: BFIN_RUN not set"
fi

echo "== 3. pack and check"
sh tools/build_knl.sh "$ROOT/build/knl"

echo "== 4. release/"
cp build/knl/hollow_saint.knl release/hollow_saint.knl
(cd release && sha256sum hollow_saint.knl > hollow_saint.knl.sha256 && cat hollow_saint.knl.sha256)
rm -rf "release/presets/synth/hollow saint"
mkdir -p "release/presets/synth/hollow saint"
cp plugin/hollow_saint/presets/*.aif "release/presets/synth/hollow saint/"
echo "release: $(wc -c < release/hollow_saint.knl) bytes, version $(python3 -c 'import json; print(json.load(open("plugin/hollow_saint/plugin.json"))["version"])')"
