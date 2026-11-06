# Development recommendations

Improvements that can be made without changing any engine's sound, controls or look. None of them has been applied:
each is a proposal for its own reviewed branch.

## 1. Guard the raw firmware reads (Hollow Saint, Studder, Erdtree)

The only code that could break silently on a new core. Keep the reads exactly as they are today, but check that
what they return is plausible once per block, and fall back when it isn't:

- **Beat clock** (`0x01AADABC`): beat length within the range 30-300 BPM gives, `follows_tape` 0 or 1. Otherwise run
  the stutter / stroke rate free at the last good (or a default 120 BPM) length.
- **Tape speed** (`0xFF802970`): within the tape's real speed range; otherwise treat as normal speed.
- **Velocity** (`0xFF801064`, "velocity << 8"): within the range kennel's SDK documents; otherwise full velocity.
- **Envelope gains** (`0xFF801D50`): within 0..32767; otherwise use the knob alone for the brass timbre.

Put each reader in one place (Hollow Saint's `op1_plugin.h` already does; Studder and Erdtree each have one
function/macro), log once at start if a check fails, and add a host test that feeds garbage and checks the fallback.
Cost: a few compares per block. On beta 3.2 the output must stay bit-identical.

## 2. Measure the CPU figures on hardware, now

Every `cpu.txt` is an estimate; two engines have already been switched off because of that. A beta 3.2 measurement
also gives the baseline that tells whether Beta 4 got faster or slower. Reverb, Studder and ROOMS have no figure and
are held to 58,052 cycles, so they are the most likely to be switched off on a heavy set.

## 3. One shared toolchain, everywhere

- Hollow Saint's `toolchain/build_toolchain.sh` (binutils 2.42) and Bloodmoon's CI toolchain build duplicate the
  shared compiler. With Sammi's OK: remove them, and point docs at `~/Documents/op1-toolchain/env.sh`.
- Rebuild Erdtree's and Bronze Choir's shipped `.opmod` with the shared toolchain at their next release so the
  shipped file is reproducible from the repo.

## 4. Keep the DSP free of Opus names

Hollow Saint (shim), Sister (upstream DSP untouched), ROOMS (`dsp/` + `interface/` + glue) and Bronze Choir
(`bc_dsp.c` vs `bronzechoir.c`) already separate DSP from platform glue. Erdtree's `erdtree.c` and Bloodmoon's
`bloodmoon.c` mix the voice DSP with registration; a split into `*_dsp.c` + glue would make a future API change touch
one file. Only worth doing together with another change to those files, with bit-identical host output as the gate.

## 5. Make each audit repeatable

- Run `tools/opus_api_audit.py` in each project's test script so a new `fw_*` name or raw address shows up in review.
- Record in each `cpu.txt` the kit commit and date with the figure.
- Keep a `pre-beta4` tag in every repo.

## 6. Good-neighbour notes for future engines

- Ask for `opus_fast` only for what the audio touches every block, and always work without it (all four current users
  do).
- No `cpu.txt` means a 58,052-cycle cap: write a measured figure before sharing.
- Prefer `opus_page_still` for static art (Erdtree, Bronze Choir) so the display thread does less each frame.
- ROOMS' load governor (lean mode, then rest, instead of being switched off) is a good pattern for heavy effects.
