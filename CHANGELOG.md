# Changelog

Each version is a git tag in this repository. Every step was host-tested; none has run on an OP-1 (see README).

## Unreleased: CPU on the OP-1 (CATHEDRAL's bells, the flute pair; Opus 3.2 kit)
- Measured on the OP-1 (Opus, developer mode): "audio: worst 857629" with CATHEDRAL around 50 %, over the whole
  580,525-cycle audio block, so the audio overran and clicked. The kit's rule: each 32-byte line of ordinary memory an
  audio block touches costs ~107 cycles, twice that while the display sends a frame. The engine's own work is ~25k
  cycles a voice in the simulator; the rest was memory.
- **The sine is computed, not read.** The bells (struck from just above 50 % CATHEDRAL), the flute pair (low STRINGS)
  and the sub-octave stepped across the 16 KB sine table: up to 120-210 lines a voice a block. `sine()` is now a
  folded 7th-order polynomial, five 16 x 16 products, within 8 LSB of the exact sine (the table's own phase steps
  were 26 LSB off). Host model (lackey trace, distinct lines per audio block x 107): ~78-85k -> ~34k cycles. The
  screen still uses the table.
- **Opus: the voices in the audio pool, the scratch in fast memory.** `opl_init` asks `opus_alloc_audio` for the six
  voices' state (~13 KB, the pool the display does not share) and `opus_fast` for the render's scratch (768 bytes, L1),
  through the engine's new `hs_use_memory`; without them the static memory serves. The manifest's `core` is 2.8 (for
  `opus_alloc_audio`), which the Opus 3.2 kit's core 2.13 covers.
- Sound: renders that use no sine are byte-identical to the declick release (phrase, single, chord6, range, sweeps of
  CATHEDRAL and WHEEL BUZZ, steal, vibrato, handover, tail). Elsewhere within 3-24 LSB (-60 to -79 dB); with the Lo-Fi
  Relic the tape's wow drifts the phase slowly apart from the old render (-34 to -52 dB difference), not its tone.
- Host cost (x86 instructions a voice block) rises ~20-35 % where the sine runs (CATHEDRAL 60 %: 19.6k -> 23.4k): the
  trade for no table traffic. Not measured on the OP-1 yet: cpu.txt still holds the 230000 estimate.

## Unreleased: declicking (poly, unison, CATHEDRAL)
- **Re-triggered voices no longer click.** A note_on on a voice that is still sounding (a stolen voice in poly, unison
  re-triggering all six voices on every note, a repeated note) used to restart the string at a random phase with its
  filters zeroed and its gains snapped, while the OP-1's envelope carried on from where it was. Such a voice now keeps
  its phase, filters and smoothed knobs, and its gains glide to the new note's across the first block. A ringing bell
  keeps its phase and rings on. Voices that were silent start exactly as before.
- **Drone hand-over:** a new owner whose own drone was still fading out (it lost the drone moments ago) keeps its
  running oscillators instead of having them overwritten mid-sound.
- **CATHEDRAL:** the body and room comb feedbacks glide sample by sample between blocks instead of stepping once per
  block while the knob turns. The output stage's hard clamp at +-65535, which the body's resonance reached at mid
  CATHEDRAL with a strong drone (up to ~40 clipped samples a second), is now a soft knee: untouched up to 57343, a
  parabola to flat at 65535, no corner.
- Unchanged sound otherwise: host suite renders run one scenario at a time are byte-identical to 0.7.8 for every
  scenario without a re-trigger, a CATHEDRAL sweep or the knee (phrase, single, chord6, sweep1, sweep2, tail, the relic
  and tremor scenarios); where the knee engages the largest difference is 176 LSB on a few hundred samples.
- Tests: `tests/declick.c` (poly / unison / mono voice modes, encoder-stepped knob turns, a sample-accurate click
  detector): 22 clicks before, 0 after, on the soft-sound runs. `tests/host_shim/op1_plugin.h`: a stand-in for the kennel
  SDK header so the host suite and declick build without the hundekraft bundle.
- Not yet run on an OP-1, and not yet measured for CPU on the device.

## 0.7.8 download zip
- `release/hollow_saint-0.7.8.zip`: the prebuilt `hollow_saint.knl`, `mods/hollow_saint.opmod`, the presets as
  `synth/hollow saint/`, `INSTALL.txt` and `SHA256SUMS.txt`. Nothing to build to install it. Made by `tools/make_zip.py`
  from `release/` (same plugin files as before, byte for byte).
- README: a Download section and install steps from the zip for kennel and Opus 2. Building from source moved to
  `docs/BUILDING.md`.

## 0.7.8 (0.7.0 with 0.7.7's screen and default)
- 0.7.7 was released as a .knl only (`releases/v0.7.7/`); its source was lost. Its disassembly shows its sound and screen
  code were rewritten, so it can't be rebuilt from 0.7.0. 0.7.8 is 0.7.0 with the two changes 0.7.7's notes name:
- Screen: 0.7.7's artwork, the whole drawing at 7/8 size and centred (`tools/gen_screen.py` maps it; the SVG is byte
  for byte the one inside 0.7.7's .knl). `hollow_saint_geom.h` keeps the drawing's own units and adds the map
  (`HS_S`, `HS_TX`, `HS_TY`); the screen code turns pivots and moves through it.
- White page 2 starts at steady on a fresh load (manifest default 0, was 1/8). Presets set their own rate.
- Audio unchanged from 0.7.0.

## 0.7.8 Opus 2 port
- `opus/sammi.hollow_saint`: the engine as an Opus 2 plugin (`.opmod`, core 2.6). It wraps `hollow_saint.c` unchanged
  through a kennel-API header (`op1_plugin.h` in that folder). It uses OS voices, the "hollow saint" type (so the kennel
  presets load) and its own OS screen. The encoder pushes (page 2) are now handled by the engine's own key and turn
  handlers. A new sound's wheel starts at steady, as in 0.7.7.
- `tools/build_opmod.sh` and `tools/gen_opus_headers.py` (the SVG as a C string, the sine table computed).
  `release/opus/hollow_saint.opmod`.
- Tests: `tests/opus_host/run.sh` (the glue's checks; audio == the kennel build) and `tests/bfin_sim/run_opus.sh` (the
  compiled `.opmod` code in the Blackfin simulator; audio == the kennel build).
- Built from 0.7.8: the same screen art as the kennel build (`hs_svg.h` regenerated), version 0.7.8.

## 0.7.0 (Lo-Fi Relic, Crank & Tremor)
- **Lo-Fi Relic** on the green press (page 2; green then sets its age, word 6): wow and flutter on every pitch (shared
  tape), cubic saturation, sample-rate and bit reduction with dither, dust, a two-pole tape-head low-pass, hiss and a
  sagging transport, before the cathedral. ~35 ms cross-fade on switching; unity gain for small signals.
- **Crank & Tremor**: the wheel strokes also pulse the level and the nasal tone, smooth (breathing) low on WHEEL BUZZ,
  mechanical from the middle, deep at the top. Stroke rates: steady, 1 bar, 1/2, 1/4, 1/8, 1/8T, 1/16 (adds the two slow
  ones; beats counted from the clock).
- Screen: a worn cassette with turning reels and an age meter while the relic is on, dust in the cathedral; 7 rate icons.
- Presets rewritten for 7 rates (midnight vigil breathes once a bar, crypt drone and drowned bell every half bar) and
  each given a relic age. Manifest page-2 defaults: age 25 %, rate 1/8.
- Relic off and wheel steady: audio bit-identical to 0.6.1. Cycles: worst 22,209 (relic off), 28,602 (relic on, oldest).
- Tests: 6 new suite scenarios (relic tone and level, 8 switches without a click, age sweep, 20 s at maximum, tremor
  over knob and rates, both together); the simulator workload adds a relic-on setting.

## 0.6.1 (speed-up, same sound)
- Blackfin cycles per voice block (simulator): worst setting 34,969 -> 21,715 (-38 %), defaults-like 29,675 -> 19,705,
  all knobs at zero 19,626 -> 13,556. Six voices at the worst setting: ~11 % of the core (was ~18 %); 1.07 x EVA.
- Audio bit-identical to 0.6.0: the 14 host-suite renders and the simulator's 138,240 samples compare equal byte for
  byte, so presets and recordings are unchanged.
- How: 16 x 16 products where both operands provably fit int16 (host test builds trap otherwise); the string loop
  specialised on the sine pair and the grit; the darkening and the bell sum tested per block, not per sample; delay
  lines in wrap-free runs; the string, drone and buzz loops as separate small functions (short branches to the rare
  polyBLEP corrections); per-block divisions memoised; branch-free room saturation; interpolator two samples a pass.
- Build: `-O2` kept (`-O3` measured no faster, larger). New `tools/release.sh`: host tests, simulator bit-exactness,
  pack, both loaders, refresh `release/` in one command.

## 0.6.0 build (the first .knl)
- `release/hollow_saint.knl` built with a Blackfin toolchain (GCC 13.3.0, binutils 2.42) compiled from source
  (`toolchain/build_toolchain.sh`); that toolchain rebuilds 11 of Hundekraft's published plugins byte for byte.
- Checks: SDK packer; the core's loader code and the reference loader at two addresses; structure compared with
  erdtree.knl; the compiled code run in the Blackfin simulator, bit-identical to the host build; Blackfin cycle counts
  against shipped engines.
- `release/presets/`: the 8 presets as a folder to copy into the OP-1's synth folder.
- No change to the engine since 0.6.0 (the file's version string stays 0.6.0).

## 0.6.0
- 8 presets: hollow saint, midnight vigil, howling dog, ash procession, bone flute, crypt drone, plague dance,
  drowned bell. OP-1 patch files with type "hollow saint", page-2 words for drone key and stroke rate, values inside
  the factory ranges.
- Screen: a TE-style line drawing of a hurdy-gurdy in a gothic arch, animated (wheel, crank, strings, buzzing bridge,
  lit keys, rose window, halo, bell), four meters, page-2 icons for drone key and stroke rate.
- Levels: master −1.2 dB for chord headroom; more dry signal at high CATHEDRAL.
- Manifest: `-fno-tree-loop-distribute-patterns`, so GCC cannot turn clearing loops into `memset`, which the core
  does not export.

## 0.5.0
- CATHEDRAL: body combs, two diffusers into a damped room, darkening filter, a distant bell at each note; bow lift
  after note-off so the space rings through the release.
- The whole voice now runs at 22.05 kHz (room and bell at 11.025 kHz) with a 4-tap half-band 2× interpolator: cost
  fell from 2.24× to 1.12× EVA (worst setting), matching the full-rate reference spectrally. The full-rate version is
  kept as tag `v0.5.0-fullrate-ref`.
- Fixes found on the way: the fixed-key drone increment at 22.05 kHz; the trompette fold threshold moved off C3 (it sat
  exactly on a note, so rounding decided its octave).

## 0.4.0
- WHEEL BUZZ: the trompette and its buzzing bridge, slapping once per cycle into a bridge resonance; wheel strokes
  locked to the OP-1's beat clock through a pressure threshold; melody grit at high settings. Page 2: stroke rate.

## 0.3.0
- DRONE: root and fifth bourdons with a sub-octave, owned by the lowest held note, with phase-coherent hand-over.
  Page 2: FOLLOW or a fixed key.

## 0.2.0
- STRINGS: the bowed melody string (polyBLEP saw and pulse, hollow sine pair, slip-gated bow noise, tone filter,
  nasal formant, drift), loudness-balanced across the knob.

## 0.1.0
- Scaffold: the SDK's synth template under the plugin's own id and manifest; host test infrastructure.
