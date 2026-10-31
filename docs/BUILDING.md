# Building Hollow Saint from source

You don't need any of this to play it: the [download zip](../README.md#download) has the plugin ready to install.
This page is for changing the code or checking the build.

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
tools/build_opmod.sh          the Opus 2 .opmod (opus/sammi.hollow_saint)
tools/make_zip.py             the download zip from release/
tools/compare_knl.py          header and metadata of .knl files side by side
toolchain/                    build_toolchain.sh (bfin-elf from git sources), MPC/MPFR shim, timestamp fix
release/                      hollow_saint-0.7.8.zip (the download), and the files in it: hollow_saint.knl,
                              opus/hollow_saint.opmod (each + sha256), presets/
results/                      test results, preset audio previews, screen preview
docs/media/                   README screenshots, screen GIF and the demo video (tools/make_tour.py --stills)
```

## Build the `.knl`

`release/hollow_saint.knl` is already built (and in the zip). To rebuild it you need the matching SDK and a Blackfin cross compiler
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

The result, `build/hollow_saint.knl`, is the file you install (or put in the zip). Read the packer's output: it lists imports and refuses
anything the core does not export.

## Build the Opus 2 `.opmod`

```sh
OPUS_KIT=/path/to/opus/plugin-kit PATH=/path/to/bfin/bin:$PATH sh tools/build_opmod.sh      # -> build/opus/hollow_saint.opmod
```

Copy it to `release/opus/hollow_saint.opmod` (and its sha256). Details and tests:
[opus/sammi.hollow_saint/README.md](../opus/sammi.hollow_saint/README.md#build).

## Make the download zip

```sh
python3 tools/make_zip.py      # -> release/hollow_saint-<version>.zip
```

It packs what is in `release/` (the `.knl`, the `.opmod`, the presets) with an install note, `SHA256SUMS.txt` and the
licence, with fixed timestamps, so the same files give the same zip. Run it after `tools/release.sh` and the `.opmod`
build, and commit the zip with them.

## Demo video and screenshots

`docs/media/hollow_saint_0.7.8_demo.mp4` (960 × 540, compressed for the repository) and the screenshots in
`docs/media/` come from one command, from the 0.7.8 code:

```sh
KENNEL=.../hundekraft-3.0.3-source/ext/kennel python3 tools/make_tour.py --demo --stills docs/media
ffmpeg -i build/tour/hollow_saint_0.7.8_demo.mp4 -vf scale=960:-2 -c:v libx264 -preset slow -crf 30 \
       -c:a aac -b:a 128k -movflags +faststart docs/media/hollow_saint_0.7.8_demo.mp4
```

The full-size 1280 × 720 video lands in `build/tour/`. `python3 tools/make_tour.py` alone makes the original tour (every
control swept, then the 8 presets) and `--features` the 0.7.0 features. Audio and screen come from the plugin's own C
code on a PC, driven by one script; none of it is a recording of an OP-1.

