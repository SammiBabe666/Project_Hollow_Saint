# sammi.hollow_saint: Hollow Saint on Opus 2

Cathedral of the Hollow Saint as an [Opus 2](https://github.com/LizardMan245/opus) plugin (`.opmod`) for the original
OP-1 on firmware 246. It is the same engine as the kennel `.knl`: `hollow_saint_opus.c` includes
`plugin/hollow_saint/hollow_saint.c` unchanged, and this folder's `op1_plugin.h` gives it the kennel API on Opus 2.

**Not yet run on an OP-1.** It builds cleanly with the Opus kit, and its audio is bit-identical to the kennel build on
the PC and in the Blackfin simulator (see the top-level README). The install, the screen and the real CPU load are
untested.

## Install

1. The OP-1 runs Opus 2 with core 2.6 or later (the Opus site's patcher).
2. Opus's menu (hold synth + drum + tape + mixer), T1, switch **developer mode** on for the first tries.
3. Disk mode (shift + COM, then T3). From the [download zip](../../README.md#download), copy `mods/hollow_saint.opmod`
   into the disk's **mods** folder. For the presets, copy the folder `synth/hollow saint` into the disk's **synth**
   folder. Eject, switch off and on.
4. The menu, T2 (the mods list): the **hollow saint** row should say "loaded".
5. **synth**, **shift + T1**, choose **hollow saint**.

Don't load it together with the kennel `hollow_saint.knl` through opus.bridge: both claim the sound type
"hollow saint", so the later one is refused.

## Controls

The same as on kennel. Plain turns are the OS's (knobs 0-3). The pushes are taken only on the engine's T1 page
(`modal` in the manifest, so they never clash with another plugin):

| Encoder | Turn | Push |
|---|---|---|
| blue | DRONE | page 2: the drone key, FOLLOW or C to B, one key per detent |
| green | STRINGS | Lo-Fi Relic on or off; while it's on, green sets the tape's age, 3 % per detent |
| white | WHEEL BUZZ | page 2: the stroke rate, steady / 1 bar / 1/2 / 1/4 / 1/8 / 1/8T / 1/16, one per detent |
| orange | CATHEDRAL | none (the OS's) |

The page words (5, 6, 7) are saved with the sound through `opus_engine_knob_set`, as kennel's dispatcher saved them.
A new sound starts at FOLLOW, age 8192 and a steady wheel (0.7.7's default).

## What the port does on Opus 2

- **Voices**: `OPUS_ENGINE_OS_VOICES` (core 2.6). The voices are iter's own objects, as on kennel, so the engine finds
  its voice index and velocity from the object's address. Its state stays in `hollow_saint.c`'s static arrays.
- **Pitch**: the render's `pitch` argument stands in for kennel's `OP1_PITCH_INC`.
- **Sine table**: `hs_sine.h` is computed (`round(32767 sin)`) by `tools/gen_opus_headers.py`. It has the same values
  as the firmware's table, so nothing is read from the firmware for it.
- **Presets**: the engine's `type` is "hollow saint", the type the kennel presets and sounds carry, so they load as
  this engine.
- **Screen**: `hollow_saint.svg` travels inside the plugin (`hs_svg.h`). It is written to the plugin's folder with
  `opus_art_text` and shown as the engine's own OS screen (`svg`, `screen_init`, `screen_draw`). The node calls go
  through Opus's `fw_*` names: `fw_svg_node_find`, `fw_node_set_matrix` and `fw_screen_draw`. If the file can't be
  written, a plain page with four bars takes its place.
- **Deep reads** (no `opus_*` service gives these): the voice velocity (0xFF801064), the metronome's beat clock
  (0x01AADABC) and the tape speed and position (0xFF802970/74). These are firmware #246 addresses as kennel's SDK
  documents them. They are only read, never written, exactly as the kennel build reads them.
- **CPU**: `cpu.txt` holds an **estimate** (230,000 cycles: six voices at the simulator's worst, plus a margin for cache
  misses), not a measurement. Measure it on the OP-1 before sharing: developer mode, six-note chords with every knob at
  maximum, the relic on and the wheel at 1/16. Then read "audio: worst N" in the plugin's details (orange push on its
  row) and put `cycles N` in `cpu.txt`.

## Build

```sh
OPUS_KIT=/path/to/opus/plugin-kit PATH=/path/to/bfin/bin:$PATH sh tools/build_opmod.sh      # -> build/opus/hollow_saint.opmod
python3 tools/gen_opus_headers.py                                                           # after changing the SVG
```

Tests: `tests/opus_host/run.sh` (ASan + UBSan: the glue's checks, and audio identical to the kennel build) and
`tests/bfin_sim/run_opus.sh` (the Opus build's compiled code in the Blackfin simulator: cycles, and audio identical to
the kennel build).

GPL-3.0-or-later with the OP-1 firmware permission, as the rest of this repository.
