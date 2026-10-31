# hollow_saint 0.7.8 on Opus 2: build and verification (2026-10-31)

Not run on an OP-1 or in an Opus 2 emulator. What was checked:

## Build
`tools/build_opmod.sh` with the Opus 2 plugin kit (github.com/LizardMan245/opus, plugin-kit, Opus 2.0 beta 3 / core
2.13) and bfin-elf-gcc 13.3.0 + binutils 2.42 (the same toolchain still rebuilds `release/hollow_saint.knl` byte for byte,
sha256 a3feff47...). The kit compiles with `-Wall -Wextra -Werror`: no warnings.

    release/opus/hollow_saint.opmod: sammi.hollow_saint 0.7.8, 63,860 bytes
    image 55,844 bytes, zero fill 14,116, 561 relocations
    imports: fw_node_set_matrix, fw_screen_draw, fw_svg_node_find, opus_art_text, opus_engine_add,
             opus_engine_knob_set, opus_engine_knobs, opus_engine_live, opus_log, opus_page_bar, opus_page_text
    cpu 230000 (an estimate, cpu.txt; not measured on a device)

`plugin_tool.py svg plugin/hollow_saint/hollow_saint.svg`: 0 errors. 34 warnings, all paths with several M commands.
The art is the same one the kennel screen draws (0.7.8: 0.7.7's drawing); the moving parts on it are checked in
the kennel frames of build-0.7.8 (the Opus build runs the same screen code).

## The glue, on the PC (tests/opus_host/run.sh, ASan + UBSan)
26 checks pass: registration (name, OS voices, type "hollow saint", the svg path, new-sound knobs), the three pushes,
the page-2 turns (drone key FOLLOW..B, the stroke rate steady..1/16, the relic age 1..32767), orange left to the OS, and
the plain-page fallback when the svg cannot be written. Audio of sim_main.c's workload (6 voices x 4 settings x 60
blocks, the relic switched on by a green push) through the Opus engine struct: identical to the kennel build, 368,640
bytes.

## The compiled code (tests/bfin_sim/run_opus.sh)
Compiled with the Opus kit's flags (`-mcpu=bf524-any`), run in the Blackfin simulator: audio identical to the kennel
host build, bit for bit. Cycles per 128-sample voice block:

| build | mid | max | zero | max + relic |
|---|---|---|---|---|
| Opus 2 (`-mcpu=bf524-any`) | 20,158 | 22,097 | 13,651 | 28,490 |
| kennel (`-mcpu=bf524`) | 20,254 | 22,209 | 13,810 | 28,602 |

The same as kennel's, within 1 %. (A first version used `#pragma GCC optimize` to keep GCC from emitting memset /
memcpy; it cost the whole engine ~10 %, so only the plugin's own memset and memcpy carry that option now.)
