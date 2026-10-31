# Cathedral of the Hollow Saint

A haunted hurdy-gurdy for the **original Teenage Engineering OP-1**: a kennel synth plugin (`hollow_saint`) for the
[Hundekraft OP-1 Workshop](https://hundekraft.github.io/op1-workshop/index.html) 3.0.x. Bowed melody strings, a drone that
can hold a key like real bourdons, a buzzing bridge that kicks in on tempo-locked wheel strokes, and a cathedral that
darkens, rings and tolls a distant bell. Two performance transformations: **Lo-Fi Relic** (press green: an old tape
in a forgotten room) and **Crank & Tremor** (the wheel's strokes pulse the level and the string's tone). Pure synthesis,
no samples. Version **0.7.0**.

**Install file: `release/hollow_saint.knl`** (40,548 bytes, sha256 in `release/hollow_saint.knl.sha256`) and the presets
folder `release/presets/`.

**Opus 2:** the same engine also builds as an [Opus 2](https://github.com/LizardMan245/opus) plugin,
`release/opus/hollow_saint.opmod` (id `sammi.hollow_saint`, core 2.6 or later; the same presets). How to install it,
its controls and what the port does: [opus/sammi.hollow_saint/README.md](opus/sammi.hollow_saint/README.md). Build it
with `tools/build_opmod.sh`. The kennel build below is unchanged.

## Status: what has and has not been done

| Stage | Status |
|---|---|
| Engine written against the real kennel 3.0.3 SDK (`op1_plugin.h`, manifest rules, exported imports) | done |
| **Host-tested engine**: firmware-simulating test suite under AddressSanitizer + UndefinedBehaviorSanitizer | **passes** (results below) |
| Screen art checked with the SDK's `svg_check.py` (decimals, stroke turn-backs) | passes |
| Blackfin toolchain (`bfin-elf-gcc` 13.3.0, binutils 2.42) built from source (`toolchain/build_toolchain.sh`) | done; **reproduces Hundekraft's official build**: 11 of the Workshop's own plugins rebuilt from the 3.0.3 source are byte-for-byte identical to the files it publishes |
| **`.knl` package** built with the SDK's packer (`tools/kennel/pack.py`): SVG, relocation and import checks | **done**: one import, `_op1_eng_page`, exported by the 3.0.3 core |
| **Loader**: the core's own loader code (`kn_load.c`) and the reference loader accept it and relocate it to identical images at two addresses; structure matches a known-good plugin (`erdtree.knl`) | **passes** |
| **The compiled Blackfin code, run** in the Blackfin instruction-set simulator at the firmware's fixed addresses: audio bit-identical to the host-tested build (184,320 samples, Lo-Fi Relic included) | **passes** |
| **Opus 2 `.opmod`** built with the Opus plugin kit (`plugin_tool.py build`: every warning an error, imports checked); its audio bit-identical to the kennel build on the PC (ASan + UBSan) and as compiled Blackfin code in the simulator (`results/opus_results.md`) | **passes** |
| Loaded in the Workshop / OP1-Emulator / on Opus 2 | not done |
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
tools/release.sh              one command: host tests, simulator, pack + loader checks, copy to release/
tools/compare_knl.py          header and metadata of .knl files side by side
toolchain/                    build_toolchain.sh (bfin-elf from git sources), MPC/MPFR shim, timestamp fix
release/                      hollow_saint.knl (+ sha256) and the presets folder, ready to install
results/                      test results, preset audio previews, screen preview
```

## Build the `.knl`

`release/hollow_saint.knl` is already built. To rebuild it you need the matching SDK and a Blackfin cross compiler
(`bfin-elf-gcc` 13.3.0 + binutils). `toolchain/build_toolchain.sh` builds one from git sources (about an hour on two
cores); with GNU release tarballs the SDK's own recipe (`docs/op1r-src-README.md`) is simpler. No original R246,
device key or firmware build is needed. `tools/build_knl.sh` packs and runs both loader checks in one go;
`tools/release.sh` runs every check (host suite, simulator bit-exactness, both loaders) and refreshes `release/`.

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
| **Green: STRINGS** | soft, hollow, almost flute-like; nasal hurdy-gurdy (middle); bright and cutting | **Lo-Fi Relic** on while page 2 is open; turning green then sets the relic's **age** (STRINGS stays where it was). Push again: off |
| **White: WHEEL BUZZ** | smooth; breathing swells (low), steady mechanical tremolo and buzz on the strokes (middle); aggressive rattle and deep pulse (high) | the **stroke rate**: steady, 1 bar, 1/2, 1/4, 1/8 (default), 1/8 triplet, 1/16 of the OP-1 tempo |
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
- **Crank & Tremor (0.7.0):** the same wheel strokes also move the voice's level and the string's nasal formant. Low on
  WHEEL BUZZ the stroke is a smooth raised cosine (slow, breathing swells, best at 1 bar or 1/2); from the middle it takes
  the wheel's mechanical stroke shape (a steady tremolo); high, a deep pulse under the aggressive buzz. The tone pulse is
  restrained (at most ±27 % of the formant). Steady (no strokes) leaves the sound still; WHEEL BUZZ at 0 turns it off.
- **Lo-Fi Relic (0.7.0):** press green. The voice goes through a worn tape before the cathedral, so the room hears the
  relic: wow (two slow, uneven sines) and flutter on the pitch of the string, drone and trompette; a warm cubic
  saturation; a lower sample rate (22 kHz down to 7.5 kHz) and fewer bits (12 down to 8, dithered, so fading notes turn to
  grain rather than gating); dust pops; a two-pole tape-head low-pass (5.6 down to 2.4 kHz); a little hiss; and a
  transport that sags now and then. The age (green turned while the relic is on) moves all of it together; small
  signals stay at about their dry level, so it is darker and older, not quieter. Switching cross-fades in ~35 ms and
  the pitch wobble fades in and out with it: no click.

Tips: use a **long T2 release** to hear the cathedral ring out, and a **full sustain** so the drone stays even when its
owner changes. For a free-standing drone note, set DRONE to 0, hold a low note and play the melody above it.

## The screen

A hurdy-gurdy in line art inside a gothic arch, in the OP-1's colours. The wheel and crank turn while notes sound
and surge on the wheel strokes; the drone strings (blue), melody strings (green) and trompette (white) shimmer with
their levels; the buzzing bridge rattles; each held note lights its key; the rose window's eight petals fill with
CATHEDRAL; the saint's halo widens and flares and the bell swings at each bell strike. Four meters show the knobs.
Page 2 of blue shows a one-octave keyboard with the drone's key (or the FOLLOW arrow); page 2 of white shows the beat
cut into the chosen stroke rate (a slur over the bar for 1 bar and 1/2). While the Lo-Fi Relic is on, the green meter
becomes a worn cassette whose reels turn with the tape's wobble, with the relic's age beside it, and dust specks drift
in the cathedral.

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
| hollow_saint 0.7.0 | 20,254 | 22,209 (all at maximum); **28,602** with the Lo-Fi Relic at its oldest |
| hollow_saint 0.6.1 | 19,705 | 21,715 |
| hollow_saint 0.6.0 | 29,675 | 34,969 |
| EVA (ships) | 18,877 | 20,286 |
| Wavetable (ships) | 27,437 | 27,437 |
| Taffy (ships) | 39,467 | 39,467 |

Six voices at Hollow Saint's worst setting come to about 46 M cycles a second, roughly 11.5 % of the OP-1's 400 MHz
core by the simulator's model, which does not model every stall; with the Lo-Fi Relic on at its oldest, 59 M, about
15 % (0.6.0 without any of it: 72 M, 18 %). That is 1.09 × EVA at the worst setting (1.41 × with the relic), below
Wavetable and Taffy, which ship. On the instrument, check peak load in the resource monitor (clock screen, T1) with six
voices, a heavy effect, the knobs at maximum and the relic on.

With the Lo-Fi Relic off and the wheel steady (or WHEEL BUZZ at 0), 0.7.0's audio is still bit-identical to 0.6.1
and 0.6.0: the host suite's 14 original renders compare equal byte for byte. The new scenarios check the relic (darker:
HF share x0.6; level within ±1 dB of dry), 8 relic switches under a chord with no click, the age swept both ways, a
20 s hold at maximum everything with the oldest relic (no runaway), the tremor at three WHEEL BUZZ settings and all 7
rates (the pulse deepens with the knob; steady stays still), and both together under a full cathedral.

0.6.1 is a speed-up only: its audio is bit-identical to 0.6.0 (the host suite's 14 renders and the simulator's
138,240 samples compare equal byte for byte), so presets and recordings sound exactly as before. Where the cycles
went: 16 × 16 products in place of 32 × 32 ones wherever both operands provably fit int16 (each use says why; the
host test builds trap if one ever does not), loops specialised so per-sample tests on the sine pair, the grit and the
darkening become per-block ones, delay lines run in wrap-free stretches instead of testing every sample, the
string, drone and buzz loops as small functions of their own (in one large function the polyBLEP's rare branches sat
behind a 5-cycle jump per test), per-block divisions memoised, and a branch-free saturation. `-O2` stays: `-O3` was
measured no faster and bigger.

Loudness at defaults sits about 1 dB under EVA, with more chord headroom (6-voice chord peak 25,199 vs EVA 29,281).
Memory: 14,088 bytes of static state (six voices' delay lines and filters), 16.5 KB of code, the 16.9 KB SVG; the
`.knl` is 40.5 KB, far under the Workshop's 512 KiB limit. No allocation at run time. Stack: `render` 672 bytes plus
at most 60 in the loop it calls (EVA's render 740, MBIRA's 800).

## Known limitations

- **Lo-Fi Relic and the green knob.** Synth engines get one kind of knob press from the kennel core: "page 2". So while
  the relic is on, turning green sets its age, not STRINGS; push green again to turn the relic off and STRINGS back
  to the knob. The relic's on/off is not saved with the patch (the age is: word 6). Red has no press of its own (its
  page word is white's).
- **The relic is per voice**, like everything here: each voice is degraded on its own (the hiss and dust follow the
  notes and their envelopes), and the tape's wow and flutter are shared, so a chord wobbles as one tape.
- **Crank & Tremor needs the wheel moving**: a stroke rate other than steady, and the OP-1's beat clock (or, when it
  stands still, the free-running wheel at the same tempo). 1 bar and 1/2 count beats from the clock, so they pulse
  every 4 or 2 beats but not necessarily on the bar's first beat.
- **Preset stroke rates moved** in 0.7.0 (7 zones instead of 5): the 8 presets were rewritten for it; a patch saved
  with 0.6 keeps its word but may land on a neighbouring rate.

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

## 0.7.0 demos

`results/relic_demo.mp3`: one phrase dry, then with the Lo-Fi Relic at 25 % and 82 % age, then with a full cathedral.
`results/crank_demo.mp3`: a held chord with WHEEL BUZZ at 25 % (1 bar, 1/2: breathing), 37 % (1/4), 55 % (1/8,
mechanical) and 90 % (1/16, aggressive), at 100 BPM. `results/screen_relic.png`: the screen with the relic on and the
1-bar rate. All made on a PC by the plugin's own C code (tests/perform.c), not recorded on an OP-1; the 0.6 preset
previews in `results/preset_previews/` predate the 0.7.0 rate changes.

## Tour video

`build/tour/hollow_saint_tour.mp4` (made by `python3 tools/make_tour.py`): every control swept, both page-2 controls,
then the 8 presets. Audio and screen come from the plugin's own C code on a PC, driven by one script; it is not a
recording of an OP-1.

## Licence and credits

GPL-3.0-or-later with the OP-1 firmware permission, as the kennel SDK it builds on (`LICENSE`, `NOTICE`,
`NOTICE.kennel`). Built on kennel by Hundekraft and op1r by pcppcp. The polyBLEP formulation follows Hundekraft's EVA;
the preset writer is adapted from Hundekraft's MBIRA; the host mocks come from op1r's harness. Artwork, synthesis
design and presets are original. Not affiliated with Teenage Engineering.
