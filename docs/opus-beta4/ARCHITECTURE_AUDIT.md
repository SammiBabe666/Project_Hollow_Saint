# Architecture audit: Opus 2 as the mods use it today

Checked against the code on 2026-11-05, not against older notes. Sources: the Opus plugin kit at
github.com/LizardMan245/opus `plugin-kit/` (head `d7f28b0`, the beta 3.2 kit; its code is `1de3ff4`'s), each
project's repository at its current `main`, and builds run on Sammi's Mac with the shared toolchain.

## 1. The platform

| Layer | What it is | Version the mods target |
|---|---|---|
| Hardware | Original OP-1 (not the Field), Analog Devices BF524 Blackfin, no FPU, 580,525 cycles per 64-sample audio block | — |
| Firmware | Teenage Engineering OP-1 firmware #246 | #246 only |
| Firmware mod | Opus 2 (LizardMan), "public beta 3.2", core 2.13, dated 2026-10-30 | manifests declare core 2.0 to 2.8 as their minimum |
| Plugin format | `.opmod` 1 (`opmod 1` in every manifest): a relocatable Blackfin image plus the manifest, imports resolved by name at load | 1 |
| SDK | `sdk/opus_plugin.h` (core calls, `OPUS_PLUGIN_API 1`) and `sdk/op1fw.h` (firmware #246 functions by name) | identical (code only) from `d9b0d2e` (beta 3) to `d7f28b0` |
| Build tool | `plugin_tool.py build <folder>` (tools/op1mod: compile, link, relocate, write the manifest's `deep`, `ram`, `imports`, `cpu` lines) | beta 3.2 kit |
| Compiler | bfin-elf GCC 13.3.0 + binutils 2.38, C only, no C library, on Sammi's Mac at `~/Documents/op1-toolchain` (Linux/aarch64 binaries, run from the Claude Linux shell on the Mac) | the kit's own recipe |
| Simulator | `bfin-elf-run` from gdb-14.2 (instruction-set only, no cache or SDRAM timing) | — |

Older targets some projects still carry, for reference: **kennel / Hundekraft Workshop** (`.knl`, Hollow Saint,
Bloodmoon, Erdtree, and the upstream SISTER, reverb and STUTTER code), and **op1r** (`.op1p`, Bloodmoon v1.3 and
Erdtree v0.5). Opus Beta 4 does not touch those builds.

## 2. How a plugin plugs in

- **Entry:** `int opl_init(struct opl_self *self)` runs once on the main thread at start, before any sound loads. It
  registers everything and allocates everything; a negative return refuses to start. `struct opl_self` starts with
  `size` so a newer core may grow it ("read size first").
- **Engines:** `opus_engine_add(self, &struct opus_engine)` with render, voice init and (core 2.6) `type`, `svg`,
  `screen_init`, `screen_draw`, and the `OPUS_ENGINE_OS_VOICES` flag, which makes the voices the OS's own objects at
  `0xFFB00000 + 0x24 v` in L1. Knobs come from `opus_engine_knobs` / `opus_engine_live` (core 2.8, follows the LFO);
  extra saved words through `opus_engine_knob_set` / `opus_engine_data_set`.
- **Effects:** `opus_effect_register` (the module model, "version 2, 2026-10-23"; version 1's `opus_effect_add` is
  gone). The core makes instances for the synth, drum and master slots. Pages are drawn under the OS's screen with
  `opus_on_frame_under`, and `opus_fx_owner` / `opus_fx_page_shown` / `opus_instrument_now` (core 2.2) say whose page
  is up.
- **Screen:** either the engine's own SVG (written to the plugin's folder with `opus_art_text`, then driven through
  `fw_svg_node_find`, `fw_node_set_matrix`, `fw_screen_draw`), or the core's drawing calls (`opus_page_*`), with
  `opus_page_still` (core 2.8) to keep the parts that don't move.
- **Controls:** every taken key or encoder is a `uses` line; the core refuses clashes at start.
- **Settings:** `setting` lines and `opus_setting` / `opus_on_setting` (Erdtree's "screen").
- **No cleanup call:** plugins are loaded once per start and never unloaded; memory from `opus_alloc` is never
  freed. There is no lifecycle teardown to port today. If Beta 4 adds hot reload or unloading, every plugin would need
  one (UNKNOWN).

## 3. Resources the core holds plugins to

| Resource | Rule today (docs/rules.md) |
|---|---|
| Audio CPU | The `cpu` line (from `cpu.txt`) is the plugin's worst block. 25 % over it in 8 of 64 blocks switches it off until the next start. No figure: held to 58,052 cycles (a tenth of the block). Developer mode never switches off. |
| Memory | Pools shared by all plugins; image at most 1 MB; `opus_alloc` zeroed, 32-byte aligned, never freed; `opus_alloc_audio` (core 2.8) from the audio pool for what the audio reads every block |
| Fast memory | `opus_fast`: about 25 KB of L1 for all plugins together, first come. Bloodmoon, Erdtree, Bronze Choir and ROOMS ask for some and fall back to SDRAM when it's gone. |
| Threads | start, main, key, display, sequencer, audio interrupt (IVG11). Audio code never waits, allocates, logs or draws. |
| Screen | 320 x 160, visible to about y 147; lowercase text (capitals are icons) |
| Files | only the plugin's own folder, never from display or audio |

## 4. Build, package, install, test: as the projects actually do it

**Build** (all eight verified 2026-11-05, see [BASELINE.md](BASELINE.md)):

```
# in the Claude Linux shell on the Mac, sources copied out of iCloud first
. ~/mnt/Documents/op1-toolchain/env.sh
python3 <kit>/plugin_tool.py build <plugin folder> --out <name>.opmod
```

Each project also has its own wrapper: Hollow Saint `tools/build_opmod.sh`, Sister `build.sh`, Studder & Reverb
and ROOMS `tools/build.sh`, Bloodmoon `tools/build-platform/build-opus.sh`, Erdtree
`scripts/build-platform/build-opus.sh KIT TC`; Bronze Choir calls the vendored kit's `plugin_tool.py` directly. Hollow Saint's needs `tools/gen_opus_headers.py`, which reads
`tests/bfin_sim/tables.h`, to regenerate `hs_sine.h` / `hs_svg.h` (both are checked in).

**Package:** the `.opmod` alone, plus presets (`synth/<engine>/*.aif` for Hollow Saint, `sound.json` folders for
Bloodmoon and Erdtree), zipped with SHA256SUMS.

**Install:** disk mode (shift + COM, then T3), copy into **mods**, restart; the mods list (hold synth + drum + tape +
mixer, T2) shows "loaded". Developer mode for first runs. Hold Stop at power-on to start with no plugins.

**Test (computer only today):**

- Host builds of the real DSP against a stand-in OS (`tests/suite.c` + `tests/host_sdk` / `host_shim` in Hollow Saint;
  `host/os_sim.c` in Bronze Choir; `tests/host` in ROOMS and Studder & Reverb; `tests/opus_host_test.c` in Sister;
  `platforms/opus/tests/compare.c` in Bloodmoon).
- Blackfin simulator runs for bit-exactness against the host and for cycle counts (`tests/bfin_sim`, `sim/`,
  `tests/cycles`).
- Screen renders from the same drawing code (`tools/render_screen.py`, `page_preview.py`, `render_frames.py`).
- **Missing:** the Opus emulator (announced, "after the beta") and the USB dev port tools. Without them nothing between
  the simulator and a real OP-1 can be tested, and no CPU figure can be measured off-device.

## 5. Dependencies by kind, and how stable each is

| Kind | Who uses it | Checked by the loader? | Stability across a core update |
|---|---|---|---|
| `opus_*` calls | all | yes, by name ("needs opus_x") | high: the core has only added names so far (2.0 to 2.13), and refuses rather than misbehaves |
| `OPUS_*` constants and struct layouts (`struct opus_engine`, colours) | all | no | medium: a field moved in a struct would compile and misbehave; `opus_kit_diff.py` shows any change |
| `fw_*` firmware functions (`op1fw.h`) | Hollow Saint, Bloodmoon, Sister, Reverb, Stutter, Bronze Choir | yes, by name; addresses are firmware #246's | high while the firmware stays #246 |
| **Raw firmware addresses** read directly | Hollow Saint (`0x01AADABC` beat clock, `0xFF802970/74` tape speed and position, `0xFF801064` velocity, `0xFFB00000` voices), Stutter (beat clock, tape), Erdtree (`0xFF801D50` envelope gains) | **no**: the build's `deep` line even says "none" for Erdtree and Stutter | lowest: right only while the firmware and the core leave those structures where they are |
| Blackfin assembly | Sister (`sister_asm.S`, inline), Erdtree (`CYCLES`, a 16x16 multiply), ROOMS (`CYCLES`) | n/a | high: the CPU doesn't change; only a change of compiler or ABI would matter |
| The "type" bridge to kennel/op1r sounds | Hollow Saint, Bloodmoon, Erdtree | n/a | Bloodmoon's docs note Opus's author says such sounds may not load; unverified |

## 6. Patterns to watch (not bugs today)

- **Raw addresses with no runtime check** (above). A reader with a sanity check (beat length in range, tape speed
  plausible) and a safe fallback would make a moved address harmless; see RECOMMENDATIONS.md.
- **CPU figures are all estimates** from the simulator plus a margin. Erdtree 0.6.0 and Bloodmoon 1.5.0/1.5.1 were
  switched off on hardware because of exactly this.
- **Shipped files not rebuilt from the repo with the shared toolchain:** Erdtree's and Bronze Choir's shipped
  `.opmod` differ from a rebuild (same size and imports; different compiler/binutils). Hollow Saint's `main` is ahead
  of its 0.7.8 release.
- **A second compiler recipe in two repos**, against the standing toolchain rule: Hollow Saint's `toolchain/` folder
  (kennel-era `build_toolchain.sh`, GCC 13.3.0 with binutils **2.42**, not 2.38) and Bloodmoon's CI (`.github/workflows/build-opus.yml` builds the kit's toolchain on
  GitHub's runners). Flagged only; nothing was changed.
- **Vendored kits** in Bronze Choir and ROOMS (`third_party/opus-plugin-kit` at `d7f28b0`) will need re-vendoring for
  Beta 4; the other projects take the kit from outside (`OPUS_KIT`).
