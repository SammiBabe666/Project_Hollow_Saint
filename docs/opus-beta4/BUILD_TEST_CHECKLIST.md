# Build & test checklist (per plugin, per kit)

Copy this into the migration PR of each project and tick it. Sections A and B run on a computer and prove the code;
only section C says anything about the OP-1. **Never mark a plugin "compatible with Beta 4" until C is done on a real
OP-1.** Record the kit commit, the toolchain versions and the date at the top.

```
Plugin:            Kit commit:            Toolchain: bfin-elf-gcc ___ / binutils ___
Branch / commit:   Date:                  Tester:
```

## A. Computer: build and static checks

- [ ] Toolchain is the shared one: `. ~/mnt/Documents/op1-toolchain/env.sh && bfin-elf-gcc --version` (13.3.0) and
      `bfin-elf-as --version` (2.38). No other compiler on PATH.
- [ ] `python3 -I tools/opus_api_audit.py KIT/plugin-kit PLUGIN_DIR` → "MISSING in kit: none"; raw addresses and
      assembly match the list in MOD_INVENTORY.md (a new one is a finding).
- [ ] `python3 KIT/plugin_tool.py build PLUGIN_DIR --out X.opmod` with no warnings (the kit builds with warnings as
      errors).
- [ ] `python3 KIT/plugin_tool.py show X.opmod`: id, version, `core`, `deep`, `ram`, `imports`, `cpu` as expected;
      image and zero-fill within ±10 % of BASELINE.md unless the change explains it.
- [ ] SVG art (if any) passes `python3 KIT/plugin_tool.py svg FILE`.
- [ ] Build twice: byte-identical (reproducible).

## B. Computer: behaviour (host tests and simulator)

- [ ] The project's host suite passes (Hollow Saint `tests/suite.c`; Bloodmoon `platforms/opus/tests/run.sh`;
      Sister `tests/opus_host_test.c`; Studder & Reverb `tests/host/run.sh`; Bronze Choir `host/` + `stress`; ROOMS
      `tests/host`).
- [ ] **Signal integrity:** audio renders are bit-identical to the `pre-beta4` build for every scenario where the DSP
      did not change. Any difference is explained in the PR.
- [ ] Silence in → silence out (effects); no NaN-like overflow, no DC drift over a long render.
- [ ] Click detector run (Hollow Saint `tests/declick.c`, Bronze Choir and ROOMS click checks) on held chords, fast
      knob sweeps and preset changes: no new clicks.
- [ ] Every parameter swept 0 → max → 0: output changes smoothly and in the documented direction.
- [ ] Every preset renders, and its knob values round-trip (saved = loaded).
- [ ] Screen render (`render_screen.py`, `page_preview.py`, `render_frames.py`) matches the previous one, inside
      y 2-146, lowercase text.
- [ ] Blackfin simulator (`bfin-elf-run`): the compiled code's output equals the host's; worst-block cycles at
      heaviest settings recorded, compared with the previous kit's.

## C. Hardware: a real OP-1 on firmware #246 with the Opus version under test

Install and start:

- [ ] Developer mode on. Only this plugin added to the usual set. Disk mode, copy into **mods**, restart.
- [ ] Mods list (synth + drum + tape + mixer, T2): row says **loaded**, not "needs ..." or "skipped".
- [ ] Engines: shift + T1 lists it; effects: appears in synth, drum and master effect lists.

Function:

- [ ] Sound on first note / first block; low, middle, high notes; six-note chords (engines).
- [ ] Each encoder does what CONTROLS says; every `uses` push/turn works on its page only; no clash reported.
- [ ] Presets load and sound as their previews; a sound saved, the OP-1 restarted, the sound reloaded is the same.
- [ ] Screen: draws, animates (if it should), nothing off the visible area, theme colours follow the user's theme.
- [ ] Raw-read features specifically: Hollow Saint stroke rate follows the metronome and tape speed; Studder locks to
      the beat; Erdtree brass follows the envelope; velocity changes Hollow Saint's level.

Resources and stability:

- [ ] Heaviest case for 5 minutes; details show **audio: worst N**. Put N in `cpu.txt` with the date and what was
      played, and rebuild.
- [ ] Developer mode **off**, same case: not switched off; no dropouts.
- [ ] Next to the heaviest neighbours you use (e.g. DX, another reverb): no dropouts, no switch-off.
- [ ] Memory share in the mods list's details recorded.
- [ ] No crash in 15 minutes of normal use; if one happens, decode the crash log with `KIT/tools/crashlog.py`.
- [ ] Stop held at power-on still starts clean (recovery path works).

Sign-off: only with all of C ticked is the plugin "verified on Opus ___ hardware". Otherwise say "builds and passes
computer tests on Opus ___; not yet run on an OP-1".
