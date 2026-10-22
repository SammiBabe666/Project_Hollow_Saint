#!/bin/sh
# Pack plugin/hollow_saint into hollow_saint.knl with the kennel SDK's packer, then check it the way the OP-1 will:
# the core's own loader (kn_load.c) and the reference loader (reloc.py) must both accept it and produce the same
# relocated image at two load addresses, with the core's real export table.
#
#   KENNEL=/path/to/hundekraft-3.0.3-source/ext/kennel PATH=/path/to/bfin/bin:$PATH sh tools/build_knl.sh [OUT_DIR]
#
# Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
K=${KENNEL:?set KENNEL to the kennel folder of the matching SDK bundle}
OUT=${1:-$ROOT/build/knl}
mkdir -p "$OUT"
command -v bfin-elf-gcc >/dev/null || { echo "bfin-elf-gcc not on PATH (toolchain/build_toolchain.sh builds one)"; exit 1; }

# 1. pack (the plugin folder must sit in the SDK's plugins/ under its id)
rm -rf "$K/synth_sdk/plugins/hollow_saint"
mkdir -p "$K/synth_sdk/plugins/hollow_saint"
cp "$ROOT"/plugin/hollow_saint/plugin.json "$ROOT"/plugin/hollow_saint/hollow_saint.c \
   "$ROOT"/plugin/hollow_saint/hollow_saint_geom.h "$ROOT"/plugin/hollow_saint/hollow_saint.svg \
   "$K/synth_sdk/plugins/hollow_saint/"
(cd "$K" && python3 tools/kennel/pack.py synth_sdk/plugins/hollow_saint -o "$OUT/hollow_saint.knl" --keep "$OUT/objects")

# 2. both loaders, two bases, the core's export table (names from kn_exports.c, addresses arbitrary)
cc -O2 -Wall -Werror -I "$K/synth_sdk" -o "$OUT/kn_load_host" "$ROOT/tests/kn_load_host.c" "$K/synth_sdk/kennel/kn_load.c"
EXPORTS=$(python3 - "$K" <<'EOF'
import re, sys
names = re.findall(r'\{ "(\w+)",\s*\(uint32_t\)', open(sys.argv[1] + '/synth_sdk/kennel/kn_exports.c').read())
print(' '.join(f'{n}={0x01E00100 + 0x40 * i:#x}' for i, n in enumerate(names)))   # as kn_boot.c: names with the C underscore
EOF
)
for BASE in 0x02980000 0x0299F000; do
    "$OUT/kn_load_host" "$OUT/hollow_saint.knl" $BASE $EXPORTS -o "$OUT/img_c.bin"
    python3 "$K/tools/kennel/reloc.py" "$OUT/hollow_saint.knl" $BASE $EXPORTS -o "$OUT/img_py.bin"
    cmp "$OUT/img_c.bin" "$OUT/img_py.bin"
    echo "base $BASE: core loader image == reference loader image"
done
ls -l "$OUT/hollow_saint.knl"
sha256sum "$OUT/hollow_saint.knl"
