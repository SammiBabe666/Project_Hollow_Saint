# Cathedral of the Hollow Saint

A haunted hurdy-gurdy for the **original Teenage Engineering OP-1**: a kennel synth plugin (`hollow_saint`) for the
[Hundekraft OP-1 Workshop](https://hundekraft.github.io/op1-workshop/index.html) 3.0.x. Bowed melody strings, a drone that
can hold a key like real bourdons, a buzzing bridge that kicks in on tempo-locked wheel strokes, and a cathedral that
darkens, rings and tolls a distant bell. Pure synthesis, no samples. Version **0.6.0**.

**Install file: `release/hollow_saint.knl`** (31,516 bytes, sha256 in `release/hollow_saint.knl.sha256`) and the presets
folder `release/presets/`.

## Status: what has and has not been done

| Stage | Status |
|---|---|
| Engine written against the real kennel 3.0.3 SDK (`op1_plugin.h`, manifest rules, exported imports) | done |
| **Host-tested engine**: firmware-simulating test suite under AddressSanitizer + UndefinedBehaviorSanitizer | **passes** (results below) |
| Screen art checked with the SDK's `svg_check.py` (decimals, stroke turn-backs) | passes |
| Blackfin toolchain (`bfin-elf-gcc` 13.3.0, binutils 2.42) built from source (`toolchain/build_toolchain.sh`) | done; **reproduces Hundekraft's official build**: 11 of the Workshop's own plugins rebuilt from the 3.0.3 source are byte-for-byte identical to the files it publishes |
| **`.knl` package** built with the SDK's packer (`tools/kennel/pack.py`): SVG, relocation and import checks | **done**: one import, `_op1_eng_page`, exported by the 3.0.3 core |
| **Loader**: the core's own loader code (`kn_load.c`) and the reference loader accept it and relocate it to identical images at two addresses; structure matches a known-good plugin (`erdtree.knl`) | **passes** |
| **The compiled Blackfin code, run** in the Blackfin instruction-set simulator at the firmware's fixed addresses: audio bit-identical to the host-tested build (138,240 samples) | **passes** |
| Loaded in the Workshop / OP1-Emulator | not done |
| **Tested on OP-1 hardware** | **not done** |

Nothing here claims more than the table says. The simulator runs the engine's code, not the OP-1's firmware, so the
Workshop install, the screen on the real display and playing on the instrument are still untested. Details:
`results/blackfin_results.md`.

## Contents

```
plugin/hollow_saint/          the plugin folder (what goes into kennel's synth_sdk/plugins/)
  hollow_saint.c              engine + screen (C, fixed point)
  plugin.json                 kennel manifest
  hollow_saint.svg            screen artwork (generated, 320 x 160)
  hollow_saint_geom.h         screen geometry (generated)
  presets.py, presets/*.aif   8 presets (OP-1 patch files)
tools/gen_screen.py           editable source of the artwork (generates the SVG and header)
tools/render_screen.py        preview rasteriser for the native SVG subset
tools/make_tour.py            builds the tour video from a scripted performance
tests/host.py                 host checks: phrase | suite | bench | cost
tests/suite.c                 firmware-simulating test suite
tests/perform.c               engine + screen from one script: WAV + per-frame screen state
tests/host_sdk/harness/       op1r host mocks (op1_host.h, audio_harness.c), credited
tests/kn_load_host.c          runs the core's own loader (kn_load.c) on a .knl
tests/bfin_sim/               the compiled engine in the Blackfin simulator: cycles + bit-exact audio vs the host build
tools/build_knl.sh            pack + check with both loaders
tools/compare_knl.py          header and metadata of .knl files side by side
toolchain/                    build_toolchain.sh (bfin-elf from git sources), MPC/MPFR shim, timestamp fix
release/                      hollow_saint.knl (+ sha256) and the presets folder, ready to install
results/                      test results, preset audio previews, screen preview
```

## Build the `.knl`

`release/hollow_saint.knl` is already built. To rebuild it you need the matching SDK and a Blackfin cross compiler
(`bfin-elf-gcc` 13.3.0 + binutils). `toolchain/build_toolchain.sh` builds one from git sources (about an hour on two
cores); with GNU release tarballs the SDK's own recipe (`docs/op1r-src-README.md`) is simpler. No original R246,
device key or firmware build is needed. `tools/build_knl.sh` packs and runs both loader checks in one go.

```sh
# 1. get the SDK that matches your OP-1's firmware (here 3.0.3) and verify it
curl -LO https://hundekraft.github.io/op1-workshop/source/hundekraft-3.0.3-source.tar.gz
curl -LO https://hundekraft.github.io/op1-workshop/source/hundekraft-3.0.3-source.tar.gz.sha256
sha256sum -c hundekraft-3.0.3-source.tar.gz.sha256
tar xzf hundekraft-3.0.3-source.tar.gz && cd hundekraft-3.0.3-source/ext/kennel

# 2. put the plugin in and pack it (the folder name, the id and the file name must all be hollow_saint)
cp -r /path/to/cathedral/plugin/hollow_saint synth_sdk/plugins/hollow_saint
python3 tools/kennel/pack.py synth_sdk/plugins/hollow_saint -o build/hollow_saint.knl
#   (toolchain not on PATH: add --toolchain DIR, with DIR/bfin-gcc/bin and DIR/bfin/bin)
```

The result, `build/hollow_saint.knl`, is the file you install. Read the packer's output: it lists imports and refuses
anything the core does not export.

## Install

1. The OP-1 must run Hundekraft **3.0.x** (the kennel core). First installation is a full firmware build from the
   Workshop through the boot menu; see the Workshop's quick start.
2. In the Workshop, open **Your plugins**, add `release/hollow_saint.knl`, connect the OP-1 by USB and press **Update OP-1**.
   Review, send, then press **T1** on the OP-1 to confirm. It installs on the confirmed restart (no hot reload).
3. On the OP-1: **synth**, then **Shift + T1**, choose **hollow saint**.
4. Presets: **Shift + COM**, then **T3** (disk mode). Copy the folder `release/presets/synth/hollow saint` into the
   OP-1's `synth` folder and eject.

If the OP-1 does not start after adding a plugin: power-cycle it; it starts once without plugin files, then remove
the plugin in the Workshop.

## The controls

| Knob | Turn | Push (page 2) |
|---|---|---|
| **Blue: DRONE** | off at 0, then subtle and dark, rich and resonant (middle), dense with a sub-octave (high) | the drone's **key**: FOLLOW (the lowest held note) or a fixed key C … B |
| **Green: STRINGS** | soft, hollow, almost flute-like; nasal hurdy-gurdy (middle); bright and cutting | none |
| **White: WHEEL BUZZ** | smooth; rhythmic buzzing on the wheel strokes (middle); continuous gritty rattle with accents (high) | the **stroke rate**: steady, 1/4, 1/8 (default), 1/8 triplet, 1/16 of the OP-1 tempo |
| **Red: CATHEDRAL** | dry and close; hollow, resonant, old-world (middle); dark, expansive, with a distant bell on each note (high) | none |

How the sound is made, briefly:

- **Melody string:** polyBLEP saw and narrow pulse (the nasal reed), a hollow sine pair for the soft end, bow noise that
  is loud when the string slips under the wheel, a key-tracked tone filter and a nasal formant; slow pitch drift.
- **Drone:** root and fifth bourdons, slightly detuned, through a resonant low-pass, plus a sub-octave at high settings.
  One voice owns it at a time (the lowest held note), so a chord gets one drone; ownership hands over with a 40 ms
  phase-coherent cross-fade, seamless in a fixed key.
- **Wheel buzz:** the trompette string and its buzzing bridge. Once per string cycle the bridge slaps, with per-cycle
  jitter, into a 1.7–3.2 kHz resonance: pitched and harmonic, never plain noise. Its size follows wheel strokes locked
  to the OP-1's beat clock through a pressure threshold, so each stroke kicks the buzz in.
- **Cathedral:** body combs (hollow), diffusers into a damped room, a darkening filter, and a faint church-bell strike
  (hum, minor-third tierce, nominal, upper partial) fed only into the room. When you release a key the bow lifts (faster
  at higher CATHEDRAL) and the space rings on through the T2 release.

Tips: use a **long T2 release** to hear the cathedral ring out, and a **full sustain** so the drone stays even when its
owner changes. For a free-standing drone note, set DRONE to 0, hold a low note and play the melody above it.

## The screen

A hurdy-gurdy in line art inside a gothic arch, in the OP-1's colours. The wheel and crank turn while notes sound
and surge on the wheel strokes; the drone strings (blue), melody strings (green) and trompette (white) shimmer with
their levels; the buzzing bridge rattles; each held note lights its key; the rose window's eight petals fill with
CATHEDRAL; the saint's halo widens and flares and the bell swings at each bell strike. Four meters show the knobs.
Page 2 of blue shows a one-octave keyboard with the drone's key (or the FOLLOW arrow); page 2 of white shows the beat
cut into the chosen stroke rate.

## Test results (this version)

`python3 tests/host.py suite`, ASan + UBSan, firmware simulation (6 voices, stealing, stock-like envelope applied after
render, stale pitch at note-on, beat clock): **all checks pass**, no clicks detected, no DC, no runaway in a 20 s hold at
maximum settings, every scenario within its bus-peak limit. The click detector itself was validated on a planted phase
jump. `python3 tests/host.py phrase` (the SDK harness's standard phrase render) passes.

The compiled Blackfin code, run in the Blackfin instruction-set simulator (`tests/bfin_sim/run.sh`), produces audio
bit-identical to the host build, so these results hold for the code in the `.knl`.

CPU on the Blackfin instruction set: simulator cycles per 128-sample voice block, same harness for every engine:

| Engine | Defaults-like (all 50 %) | Worst knob setting |
|---|---|---|
| hollow_saint | 29,675 | **34,969** (all at maximum) |
| EVA (ships) | 18,877 | 20,286 |
| Wavetable (ships) | 27,437 | 27,437 |
| Taffy (ships) | 39,467 | 39,467 |

Six voices at Hollow Saint's worst setting come to about 72 M cycles a second, roughly 18 % of the OP-1's 400 MHz
core by the simulator's model, which does not model every stall. That sits between two engines the Workshop ships.
On the instrument, check peak load in the resource monitor (clock screen, T1) with six voices, a heavy effect and the
knobs at maximum. (An earlier x86 instruction-count estimate put it at 1.12 × EVA; on the real instruction set it is
about 1.7 × EVA.)

Loudness at defaults sits about 1 dB under EVA, with more chord headroom (6-voice chord peak 25,199 vs EVA 29,281).
Memory: 13,372 bytes of static state (six voices' delay lines and filters), 11.5 KB of code, the 14 KB SVG; the
`.knl` is 31.5 KB, far under the Workshop's 512 KiB limit. No allocation at run time. Stack in `render`: 700 bytes
(EVA's 740, MBIRA's 800).

## Known limitations

- **Not yet run on an OP-1 or loaded through the Workshop.** The package, the loader checks and the compiled code are
  verified on the PC and in the Blackfin simulator; the install, the real screen and the real CPU load are not.
- **The drone needs a sounding note.** The OP-1 gives an engine only its voices, each multiplied by its own envelope
  after render, so no engine can run a drone on its own. The drone sounds while any note sounds, follows its owner's
  envelope, and stops when the last release ends. A fixed key ignores octave, transpose and global tune.
- **The cathedral's tail is cut by the voice's T2 release**, and by voice stealing.
- **22.05 kHz internal rate** (as EVA): content above about 10 kHz rolls off; faint interpolation images appear only
  at the very top of the keyboard.
- **Velocity** shapes bow pressure and level, but the OP-1's own keys send no velocity; it applies to MIDI input.
- **The screen** has only been rendered by the stand-in rasteriser, not by R246's renderer. The SVG (13.7 KB) is above
  Hundekraft's 8 KiB artwork target, though well under shipped screens (49–73 KB). It uses `op1_eng_page`, exported by
  the 3.0.3 core.
- **Presets** stay within the factory ranges Hundekraft measured, but their envelope and FX settings have not been
  auditioned on an OP-1. The tour's host renders approximate the ADSR and leave out the OP-1's FX.

## Tour video

`build/tour/hollow_saint_tour.mp4` (made by `python3 tools/make_tour.py`): every control swept, both page-2 controls,
then the 8 presets. Audio and screen come from the plugin's own C code on a PC, driven by one script; it is not a
recording of an OP-1.

## Licence and credits

GPL-3.0-or-later with the OP-1 firmware permission, as the kennel SDK it builds on (`LICENSE`, `NOTICE`,
`NOTICE.kennel`). Built on kennel by Hundekraft and op1r by pcppcp. The polyBLEP formulation follows Hundekraft's EVA;
the preset writer is adapted from Hundekraft's MBIRA; the host mocks come from op1r's harness. Artwork, synthesis
design and presets are original. Not affiliated with Teenage Engineering.
