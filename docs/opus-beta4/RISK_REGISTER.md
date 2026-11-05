# Risk register

Severity: what happens if it goes wrong. Likelihood: how likely with Beta 4, given what is known (mostly UNKNOWN).

| # | Risk | Projects | Severity | Likelihood | Evidence today | Mitigation / what retires it |
|---|---|---|---|---|---|---|
| R1 | Raw firmware addresses (beat clock `0x01AADABC`, tape `0xFF802970/74`, velocity `0xFF801064`, envelope `0xFF801D50`) move or change meaning; reads go wrong **silently** (the loader can't see them) | Hollow Saint, Studder, Erdtree | **High**: wrong tempo lock, wrong velocity, wrong brass timbre, possibly a stall | UNKNOWN (low if firmware stays #246 and the core doesn't relocate OS state) | addresses come from kennel's SDK for #246; build `deep` line misses them | Self-check + fallback in each reader (RECOMMENDATIONS 1); test on device in checklist C; prefer an `opus_*` service if Beta 4 adds one for tempo/tape/velocity |
| R2 | No plugin has a CPU figure measured on hardware; estimates wrong → switched off or dropouts | all | **High** | **Already happened** (Erdtree 0.6.0, Bloodmoon 1.5.0/1.5.1) | `cpu.txt` files say "estimate" | Measure on beta 3.2 now (roadmap 0.3); re-measure on Beta 4 |
| R3 | `OPUS_ENGINE_OS_VOICES` voice layout (0x24 bytes at `0xFFB00000`) changes | Hollow Saint | High | UNKNOWN | documented in `opus_plugin.h` as the OS's own objects | header diff; host test of voice index |
| R4 | Engine/effect struct layouts change without a name change: compiles, misbehaves | engines / effects | High | UNKNOWN (the core has kept them stable 2.0-2.13 by appending) | `opus_kit_diff.py` shows the lines | read the struct diff before building; host tests |
| R5 | An `opus_*` / `fw_*` name removed | whoever uses it | Medium: refused at load with a clear message, nothing breaks | UNKNOWN (none removed so far) | audit tool lists every name | audit tool MISSING check |
| R6 | Plugin format / manifest grammar changes (`opmod 2`, new required lines) | all | Medium | UNKNOWN | `opmod 1` everywhere | kit docs diff; rebuild |
| R7 | Compiler requirement changes (new GCC/binutils) | all | Medium: rebuild of the shared toolchain needed | UNKNOWN | kit recipe pins 13.3.0 / 2.38 | ask Sammi first; rebuild once from `op1-toolchain/sources` |
| R8 | Sound-type bridge ("type" for kennel/op1r sounds) changes or never worked: old sounds and presets don't load | Hollow Saint, Bloodmoon, Erdtree | Medium | Already doubtful for Bloodmoon (Opus's author says such sounds may not load) | Bloodmoon `docs/platform-compatibility.md` | test on device; ship native Opus-saved presets |
| R9 | Fast memory (about 25 KB shared) shrinks or is contended | Bloodmoon, Erdtree, Bronze Choir, ROOMS | Low-Medium: SDRAM fallback costs CPU | UNKNOWN | all four fall back | CPU figure measured with the fallback path too |
| R10 | Lifecycle change (unload / reload) needs teardown no plugin has | all | Medium | UNKNOWN | no cleanup API today | add teardown only when the API exists |
| R11 | Local work not in git (Bloodmoon 1.5.3, ROOMS 1.1.1) lost or diverges | Bloodmoon, ROOMS | Medium | Present now | tgz/opmod on the Mac only | push to a branch (roadmap 0.1) |
| R12 | Shipped binaries not reproducible from the repo (Erdtree, Bronze Choir built with another binutils) | Erdtree, Bronze Choir | Low | Present now | rebuild differs (BASELINE.md) | rebuild releases with the shared toolchain at the next version |
| R13 | Second compiler recipes in repos (Hollow Saint `toolchain/`, Bloodmoon CI) drift from the shared toolchain | Hollow Saint, Bloodmoon | Low | Present now | binutils 2.42 in Hollow Saint's recipe | remove or point at the shared one, with Sammi's OK |
| R14 | Hand-declared firmware signature (`fw_screen_draw(scr, dt)` in Bloodmoon's `moon.c`) diverges from op1fw.h | Bloodmoon | Low-Medium | UNKNOWN | comment in `moon.c` | check against the new op1fw.h on migration |
| R15 | Vendored kits (Bronze Choir, ROOMS at `d7f28b0`) left stale | Bronze Choir, ROOMS | Low | Certain once Beta 4 ships | `VENDORED.txt` | re-vendor on migration |
| R16 | No emulator: nothing between simulator and hardware | all | Medium | Present until it ships | kit README | hardware checklist C; adopt the emulator when released |
| R17 | Hardware compatibility claimed without a device run | all | High (users' music at stake) | Avoidable | READMEs mostly say "not tested" honestly | checklist sign-off wording |
