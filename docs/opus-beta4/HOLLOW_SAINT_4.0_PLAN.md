# Hollow Saint: the 4.0 work, ready to run

Sammi's four asks for Hollow Saint (2026-11-05), written up as a plan and **not yet applied**: on Sammi's word, nothing
is built or changed until Opus 4.0 (Beta 4) is here. Each step says what changes, where, and the check that proves it
didn't change the sound. Do them in this order, each as its own commit on the migration branch, after PR #7 is merged
(every step touches files #7 also changes).

## 1. CPU first

**What is known.** A device measurement exists: after PR #6, at around 50 % CATHEDRAL, Opus reported **audio worst
857,629 cycles**. That is more than the whole 580,525-cycle audio block, which is why it clicks. `cpu.txt`'s 230,000 is an
estimate from before that and is far too low.

**Simulator, measured 2026-11-05** (instructions only, no memory timing; GCC 13.3.0 / binutils 2.38, Opus kit flags),
cycles per voice per block:

| Setting | main (`b31b1ff`) | PR #7 (`6055bb0`) |
|---|---|---|
| mid (all knobs 50 %) | 20,702 | 21,037 |
| max (all 100 %) | 22,732 | 29,715 |
| zero (all 0 %) | 21,313 | 34,144 |
| max + relic | 29,149 | 36,365 |

**Memory, modelled** (`tests/mem_cost`, the kit's figure of ~107 cycles for each 32-byte line an audio block
touches), at CATHEDRAL 20000 (about 61 %):

| | main | PR #7 |
|---|---|---|
| data lines, summed over 6 voices | 1,600 (171k cycles) | 906 (97k cycles) |
| code lines, summed over 6 voices | ~1,420 (152k cycles) | ~1,370 (146k cycles) |

**An estimate, not a measurement:** take six voices' instructions plus twice the memory cost (the kit says memory costs
about twice as much while the display sends a frame). That model gives about 770k for main, close to the 857k Sammi
measured. For PR #7 it gives about **640k at 61 % CATHEDRAL and about 710k at the heaviest settings**. So #7 probably
halves the data traffic but still runs over the block while the screen draws. The engine's own code, fetched again for
each voice, is now the biggest memory cost.

**To do (needs the OP-1):**
1. Build PR #7's `.opmod` with the shared toolchain and measure it: developer mode, six-note chords, CATHEDRAL at 25 /
   50 / 75 / 100 %, the relic on, the wheel at 1/16, screen showing. Record "audio: worst N" for each.
2. If it is still over the block, the next options are (in order of how little they change the sound): keep the
   per-voice code path smaller (fewer distinct functions in render, with the knob-dependent sections behind cheaper
   gates), move more per-block tables into `opus_fast`, then cheaper reverb taps at high CATHEDRAL. Each one is a CPU
   change that needs Sammi's ear, so none is planned blind.
3. Write the measured figure into `opus/sammi.hollow_saint/cpu.txt` as `cycles N` with the date and what was played,
   only once it fits the block.

Two test-harness fixes found while measuring, to include in the migration branch:
- `tests/bfin_sim/opus_sim.c` doesn't stub `opus_alloc_audio` and `opus_fast`, so on PR #7 the simulator build fails to
  link. Add stubs that return 0 (the engine's static memory then serves).
- `tests/bfin_sim/run.sh` and `run_opus.sh` pass `-Wl,--no-warn-rwx-segments`, which binutils 2.38 (the shared
  toolchain's) rejects. Drop the flag.

## 2. Remove `toolchain/` (the second compiler recipe)

Delete `toolchain/build_toolchain.sh` and its two helpers, `toolchain/mpfr4_compat.c` and
`toolchain/touch_generated.sh` (they exist only for that script). Git history keeps them. Then point these lines at the
shared toolchain (`. ~/mnt/Documents/op1-toolchain/env.sh`, GCC 13.3.0 + binutils 2.38):

- `README.md` line 39: the status row "Blackfin toolchain ... built from source (`toolchain/build_toolchain.sh`)".
- `docs/BUILDING.md` line 29 (the `toolchain/` row of the contents) and line 39 (how to get a compiler).
- `tools/build_knl.sh` line 14, the "not on PATH" message.

Leave `CHANGELOG.md` and `results/blackfin_results.md` as they are: they record what was true then. No build is needed
for this step.

## 3. Replace the kennel compatibility layer with direct Opus code

**Today.** `opus/sammi.hollow_saint/op1_plugin.h` pretends to be kennel's SDK. It recreates kennel's types
(`op1_engine_t`, `op1_screen_t`, `op1_beat_t`), its globals (`op1_eng_page`) and its macros, so that
`plugin/hollow_saint/hollow_saint.c` compiles unchanged. `hollow_saint_opus.c` then calls the engine through kennel's
struct (`ENGINE_hollow_saint.render(...)`).

**After.** The engine talks to a small seam of its own, and each platform fills it in directly:

- New `plugin/hollow_saint/hs_port.h`: the few things the engine needs from its host, as `HS_*` names (voice index,
  pitch increments, velocity gain, beat clock, the page flags, the screen's node calls, the knobs for the screen, the
  sine table for the screen).
  - Without `HS_OPUS` (the kennel `.knl`), they map 1:1 onto kennel's `op1_*` names, so the kennel build compiles to
    the same code as today.
  - With `HS_OPUS`, it includes `opus/sammi.hollow_saint/hs_port_opus.h`, which maps them to `opus_*`, `fw_*` and the
    glue's own state. It has no kennel types and no `OP1_*` names.
- `hollow_saint.c` uses `HS_*` instead of `op1_*` (about 30 places: `hs_note_on`, `hs_note_off`, `hs_render` and the screen functions `hs_screen_init` / `hs_screen_update`). Kennel's
  `ENGINE_hollow_saint` / `SCREEN_hollow_saint` structs stay, but only in the kennel build.
- `hollow_saint_opus.c` calls `hs_note_on`, `hs_render`, `hs_note_off`, `hs_screen_init` and `hs_screen_update`
  directly. It keeps its own page flags and press table instead of kennel's.
- `opus/sammi.hollow_saint/op1_plugin.h` is deleted. The tests that include it (`tests/opus_host/opus_main.c`,
  `tests/bfin_sim/opus_sim.c`) move to the new names.

**Proof it changed nothing:**
1. Kennel side: compile `hollow_saint.c` for Blackfin with kennel's flags against `tests/host_shim` before and after.
   The `.text` and `.data` must be byte-identical. The host suite (`tests/suite.c`, one scenario per process) and
   `tests/declick.c` must give byte-identical output.
2. Opus side: `tests/opus_host` (ASan + UBSan) and the simulator's audio must be byte-identical to the pre-change Opus
   build in every scenario. The only exception is step 4's intentional change, which gets its own commit and its own
   comparison.
3. `tools/opus_api_audit.py` on the new folder: no `OP1_*` or kennel names left, and the raw addresses as step 4
   leaves them.

**Kept:** the `.knl` (nothing removed), the type "hollow saint" (kennel presets still load), every control, the
screen.

## 4. The four direct firmware reads

| Read | Today | 4.0 plan | Change to sound |
|---|---|---|---|
| Voice memory, `0xFFB00000 + 0x24 v` (`OPUS_ENGINE_OS_VOICES`) | the voice index is worked out from the OS object's address | **Replace:** drop the flag and give the core a small per-voice state (`.voice = sizeof` a struct holding the index), filled in by `init(voice, index)`, as Erdtree does. No address left. | none to the DSP. One behaviour change to confirm with Sammi: without the flag the core passes the knobs with the LFO in (as for every native Opus engine), so an LFO aimed at a knob starts working on Hollow Saint |
| Velocity, `0xFF801064` | read per render, `v > 0 && v <= 127 ? v * 258 : 32767` | **Guard:** keep the read, which is already range-checked, and also check the voice index. **Open question:** `opus_plugin.h` says the OS applies velocity around every engine's render. If that is also true with OS voices, Hollow Saint applies velocity twice on Opus. That needs one device check (soft vs hard notes against a stock engine) before anything changes | none until the device check answers it |
| Beat clock, `0x01AADABC` (+0x14, +0x1C, +0x20) | read once per block; the engine already clamps the length to 4,410-176,400 samples and `next` to 0..length | **Guard:** reject an implausible clock (length outside 0.1-4 s, `follows_tape` not 0 or 1, `next` outside 0..length). Then behave as "no clock yet": the wheel runs free at 120 BPM, as it does today before the metronome starts | none while the address is right |
| Tape, `0xFF802970` (speed) and `0xFF802974` (position) | speed sets the clock's step when it follows the tape; **position is never used** by the engine | **Drop** the position read. **Guard** the speed: the engine already clamps the step to 0..4x; also treat an implausible value as normal speed | none |

For each guard there is a host test that feeds a bad value and checks the fallback, and the output with good values is
byte-identical to today's.

## When 4.0 lands

Run `tools/opus_kit_diff.py` against the new kit before any of the above. If the new core offers a service for the
tempo, the tape speed or the velocity, use it instead of the guarded read. If it changes the engine struct, step 3 is
where that change goes. Then follow steps 1-4 and the device sheet ([DEVICE_TEST_SHEET.md](DEVICE_TEST_SHEET.md)).
