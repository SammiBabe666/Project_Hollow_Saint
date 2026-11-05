# Beta 4 compatibility matrix

**Status of Beta 4 on 2026-11-05: not released. Every Beta 4 cell is UNKNOWN until its kit is published.** What *is*
known is what each project depends on, so the matrix says which Beta 4 change would affect which project and how to
check it in minutes once the kit exists.

## A. What each project depends on (verified today)

| Area | Hollow Saint | Bloodmoon | Erdtree | Sister | Reverb | Studder | Bronze Choir | ROOMS |
|---|---|---|---|---|---|---|---|---|
| Kind | engine | engine | engine | effect | effect | effect | engine | effect |
| Min core | 2.6 | 2.6 | 2.8 | 2.0 | 2.2 | 2.2 | 2.8 | 2.2 |
| Engine/effect registration | `opus_engine_add`, OS voices | `opus_engine_add` | `opus_engine_add` | `opus_effect_register` | same | same | `opus_engine_add` | same |
| Knob / data saving | `knob_set` | knobs only | `engine_live` | effect params | params | params | `knob_set`, `data_set` | params |
| Screen | own SVG via `fw_*` | own SVG via `fw_*` | `opus_page_*`, `page_still`, art layer | `opus_page_*`, art, under OS | `opus_page_*` under OS | `opus_page_*` under OS | `opus_page_*`, `page_still` | page under OS |
| Memory | statics (14 KB zero) | `opus_fast` 4.2 KB | `opus_fast`, `opus_alloc_audio`, `opus_alloc` | `opus_alloc` x2 | effect state | effect state | `opus_fast` | effect bulk 66 KB, `opus_fast` 1.5 KB |
| `fw_*` names | 3 | 4 | 0 | 1 | 1 | 1 | 2 | 0 |
| Raw firmware reads | **5 addresses** | none | **1** | none | none | **3** | none | none |
| Assembly | none | none | inline | `.S` + inline | none | none | none | inline |
| `uses` lines | 6 | 0 | 0 (+1 setting) | 3 | 0 | 2 | 6 | 4 |
| Sound-type bridge | "hollow saint" (kennel) | "bloodmoon" (op1r) | "erdtree" (op1r/kennel) | n/a | n/a | n/a | n/a | n/a |
| Builds on beta 3.2 kit | yes | yes, byte-identical | yes | yes, byte-identical | yes, byte-identical | yes, byte-identical | yes | yes, byte-identical |

## B. If Beta 4 changes X, who is affected

| Possible Beta 4 change | Status | Affects | How to tell, once the kit is out |
|---|---|---|---|
| An `opus_*` name removed or renamed | UNKNOWN | anyone using it (`opus_api_audit.py` lists each) | `opus_api_audit.py NEW_KIT ...` prints MISSING; the core would refuse to load ("needs opus_x") |
| `struct opus_engine` / effect struct fields moved | UNKNOWN | the 4 engines / the 4 effects | `opus_kit_diff.py` shows the struct's changed lines; then a rebuild and host test |
| `struct opl_self` grown | UNKNOWN | none expected: no plugin reads its fields directly, they only pass `self` back to the core | the diff; the struct starts with `size` so it can grow safely |
| New `core` minimum or `opmod 2` format | UNKNOWN | all | kit's `docs/manifest.md` and `tools/op1mod/opmod.py` differ; rebuild and `plugin_tool.py show` |
| Firmware other than #246, or the core moving OS structures | UNKNOWN | **Hollow Saint, Studder, Erdtree** (raw reads); then everything using `fw_*` | site's release notes; a changed `op1fw.h` address list; on device, the self-checks proposed in RECOMMENDATIONS.md |
| `OPUS_ENGINE_OS_VOICES` layout changed (0x24-byte voices at 0xFFB00000) | UNKNOWN | Hollow Saint | the header comment for the flag; host test of voice index / velocity |
| CPU guard rule changed (25 % in 8/64) | UNKNOWN | all, most the unmeasured ones | `docs/rules.md` diff |
| Fast memory budget changed (about 25 KB) | UNKNOWN | Bloodmoon, Erdtree, Bronze Choir, ROOMS (all fall back to SDRAM) | `docs/rules.md`; mods list's details on device |
| Drawing calls / page model changed | UNKNOWN | all pages; SVG engines also on `fw_screen_draw` | header diff; screen renders; on device |
| Lifecycle: unload, hot reload, a cleanup call | UNKNOWN | all (none has teardown today; memory is never freed) | new `opus_on_*` / struct field in the diff |
| Sound-type bridge changed | UNKNOWN | Hollow Saint, Bloodmoon, Erdtree presets | on device: load each preset |
| Compiler requirement changed (GCC / binutils) | UNKNOWN | all | the kit's `tools/toolchain/build-bfin-toolchain.sh` versions; any change must be raised with Sammi before rebuilding the shared toolchain |
| Emulator released | expected "after the beta" | all: first off-device CPU figures | the site / kit README |

## C. Expected migration effort per project (if Beta 4 keeps API 1)

| Project | Preserve as is | May need adaptation | Effort if the API only grows |
|---|---|---|---|
| Hollow Saint | `plugin/hollow_saint/hollow_saint.c` (all DSP), presets, SVG | `opus/.../op1_plugin.h` (the shim: raw reads, voice layout) | rebuild + test |
| Bloodmoon | `bloodmoon.c` DSP, art | `moon.c`'s own `fw_screen_draw` declaration | rebuild + test |
| Erdtree | voices, tables, page | the `0xFF801D50` read (brass timbre) | rebuild + test |
| Sister | everything (DSP is upstream's, untouched) | none known | rebuild + test |
| Reverb | everything | none known | rebuild + test |
| Studder | DSP | the beat-clock / tape reader | rebuild + test |
| Bronze Choir | everything | re-vendor the kit | re-vendor + rebuild + test |
| ROOMS | everything | re-vendor the kit | re-vendor + rebuild + test |
