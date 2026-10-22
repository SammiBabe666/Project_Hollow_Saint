# Changelog

Each version is a git tag in this repository. Every step was host-tested; none has run on an OP-1 (see README).

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
