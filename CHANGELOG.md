# Changelog

Each version is a git tag in this repository. Every step was host-tested; none has run on an OP-1 (see README).

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
