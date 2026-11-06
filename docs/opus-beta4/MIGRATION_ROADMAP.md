# Migration roadmap

Ordered so that nothing is rebuilt before the new architecture is understood. Steps 0-2 can happen now; the rest
wait for the Beta 4 kit.

## 0. Now, before Beta 4 (no code changes to any engine)

1. **Bring local work into version control.** Bloodmoon 1.5.3 (`~/Documents/OP-1 Projects/archive/
   bloodmoon-1.5.3-build-src.tgz`) and ROOMS 1.1.1 (`rooms-build/rooms-src-v10.tgz`) are newer than their repos.
   Push each to its repo on a branch, or note that they are abandoned.
2. **Tag today's state** in every repo (`pre-beta4`, on `main`), so each migration branch has a fixed checkpoint
   to return to. Tags are cheap and reversible.
3. **Measure real CPU figures on an OP-1 under beta 3.2** for every plugin (developer mode, heaviest case, "audio:
   worst N", into `cpu.txt`). This is the biggest open risk today, and a beta 3.2 figure is the baseline a Beta 4
   figure is judged against. Start with Erdtree 0.6.1 and Bloodmoon 1.5.2/1.5.3, which have already been switched off.
4. Optionally, the low-risk hardening in [RECOMMENDATIONS.md](RECOMMENDATIONS.md) (raw-read self-checks), each on its
   own branch and reviewed, since it touches code.

## 1. The day the Beta 4 kit appears

1. Clone the kit fresh (`git clone https://github.com/LizardMan245/opus`); note the commit.
2. `python3 -I tools/opus_kit_diff.py <old kit at d7f28b0>/plugin-kit <new>/plugin-kit` and read every line of the
   header diff. Also read the new `docs/rules.md`, `docs/manifest.md`, `AGENTS.md` and the site's changelog.
3. Check the compiler: does `plugin-kit/tools/toolchain/build-bfin-toolchain.sh` still say GCC 13.3.0 / binutils
   2.38? If not, **stop and ask Sammi** before changing the shared toolchain.
4. `python3 -I tools/opus_api_audit.py <new>/plugin-kit <all eight plugin folders>` (command in BASELINE.md). Any
   MISSING name is a hard blocker for that plugin.
5. Fill section B of [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md): replace each UNKNOWN with what the evidence
   says, quoting the file and line.
6. Save the result to project memory (the kit commit and what changed).

## 2. Per project, lowest risk first

Order: **Reverb → Sister → ROOMS → Bronze Choir → Bloodmoon → Erdtree → Studder → Hollow Saint**. The first four have
no raw reads and prove the pipeline; the last three carry the raw firmware reads.

For each:

1. Branch `opus-beta4` from the `pre-beta4` tag.
2. Rebuild **unchanged** with the new kit. If it builds and the audit is clean, go to 4.
3. If not, change only what the diff requires, in the glue/shim file only (never the DSP), one commit per change,
   each citing the kit line that forces it.
4. Run [BUILD_TEST_CHECKLIST.md](BUILD_TEST_CHECKLIST.md) sections A (computer) and B (simulator). Audio must stay
   bit-identical to the `pre-beta4` build wherever the DSP didn't change.
5. Raise the manifest's `core` line only if a new service is used.
6. Hardware run (section C), then `cpu.txt` from the device, then a release.

## 3. After all eight

- Update each project's README "platform compatibility" with the Beta 4 status (hardware-verified or not).
- Remove the `pre-beta4` tags only when Sammi says so.
