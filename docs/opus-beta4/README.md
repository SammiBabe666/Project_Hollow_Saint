# Opus Beta 4 readiness pack

An audit of every Opus plugin Sammi has made, done on 2026-11-05 before Opus Beta 4 exists, so that the move to it is
a controlled one. Nothing in any project's source was changed for this. Each file here answers one question:

| File | What it holds |
|---|---|
| [ARCHITECTURE_AUDIT.md](ARCHITECTURE_AUDIT.md) | How Opus 2 (beta 3.2, core 2.13) works today, how the mods are built, packaged, installed and tested, and what each one depends on |
| [MOD_INVENTORY.md](MOD_INVENTORY.md) | Every project: source layout, engine/DSP, controls, presets, screen, build, known bugs, risks |
| [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) | Project by project: what a Beta 4 change would touch, what is UNKNOWN until Beta 4 ships |
| [MIGRATION_ROADMAP.md](MIGRATION_ROADMAP.md) | The ordered steps for the day Beta 4's kit appears, and for each project after it |
| [BUILD_TEST_CHECKLIST.md](BUILD_TEST_CHECKLIST.md) | The repeatable validation run, with computer checks and hardware checks kept apart |
| [RISK_REGISTER.md](RISK_REGISTER.md) | Compatibility risks with severity, likelihood and what would retire each one |
| [RECOMMENDATIONS.md](RECOMMENDATIONS.md) | Improvements that can be made now without touching any project's sound or look |
| [BASELINE.md](BASELINE.md) | The reference build of every plugin on the current kit, with sizes and SHA-256, to compare Beta 4 builds against |

Two tools come with it, both read-only:

- `tools/opus_api_audit.py KIT/plugin-kit PLUGIN_DIR...` lists every core (`opus_*`) and firmware (`fw_*`) name each
  plugin uses, flags any the kit no longer has, and lists raw firmware addresses and assembly.
- `tools/opus_kit_diff.py OLD/plugin-kit NEW/plugin-kit` compares two kits' SDK headers with the comments stripped,
  so a new beta's real API changes show (names added and removed, changed declarations) apart from wording changes.

## What is known about Beta 4

**Nothing verified.** On 2026-11-05 the Opus site (opus-op1.pages.dev) lists "opus 2 · public beta 3.2", core dated
2026-10-30, and says nothing about a beta 4, a new architecture, a new plugin format or breaking changes. The kit
repository (github.com/LizardMan245/opus) is still at `d7f28b0` (a README line on top of `1de3ff4`, the beta 3.2
kit). The site says the emulator and the USB dev port's tools "come after the beta", and that the core's source will
be published with no date set.

Everything in this pack about Beta 4 is marked **UNKNOWN** with the evidence that would settle it. No project's code
was changed in anticipation of it.

## The short version

- All eight plugins (six of them the ones in the brief, plus The Bronze Choir and ROOMS) **build cleanly today** with
  the shared toolchain (GCC 13.3.0, binutils 2.38) and the current kit, and every core/firmware name they use is still
  in the kit's headers. Five rebuilds are byte-identical to the shipped files.
- **None has a measured CPU figure from an OP-1**, and only Erdtree has been loaded on one under Opus (0.6.0, silent,
  most likely switched off for CPU). That is the biggest gap, and it doesn't depend on Beta 4 at all.
- The most fragile code is not the `opus_*` API, which the core checks at load, but the **raw firmware #246 addresses**
  Hollow Saint, Stutter and Erdtree read directly (beat clock, tape speed, voice velocity, envelope gains, the voice
  objects in L1). The loader cannot see these, so if Beta 4 moves any of them nothing will warn: they would read wrong
  values silently. Each is listed with its fallback.
- Two pieces of local work are not in their repositories yet: Bloodmoon 1.5.3 and ROOMS 1.1.1. Push them (or decide
  not to) before branching for Beta 4, so the migration starts from what is really current.
