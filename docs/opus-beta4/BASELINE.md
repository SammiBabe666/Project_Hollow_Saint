# Baseline: every plugin on the beta 3.2 kit (2026-11-05)

The reference a Beta 4 build is compared with. Built on Sammi's Mac in the Claude Linux shell, with the shared
toolchain (`~/Documents/op1-toolchain`: bfin-elf-gcc 13.3.0, GNU assembler 2.38) and the Opus plugin kit
`LizardMan245/opus` `plugin-kit/` at `d7f28b0` (code identical to `1de3ff4`, "beta 3.2", core 2.13). No toolchain or
kit was modified. Sources were each repository's `main` at the commit below, copied out of iCloud into the VM first.

```
. ~/mnt/Documents/op1-toolchain/env.sh
python3 plugin-kit/plugin_tool.py build <plugin folder> --out out/<id>.opmod
```

| Plugin | Repo @ commit | Size (B) | SHA-256 | Same as the shipped file? |
|---|---|---|---|---|
| sammi.hollow_saint | Project_Hollow_Saint @ b31b1ff | 64,496 | `eb2e662cc1b6f9d335c8646e6dfe5795a8ca9c1228770aefac609f03986282b3` | no: `main` has the unreleased declick work; `release/opus` is 0.7.8 (63,860 B) |
| noladrift.bloodmoon | Project_Bloodmoon @ 32c4a04 | 52,956 | `854d06e7e7712bbd399ab1f6a805fab82cb54de53a38dff25dcab279630dd74e` | **identical** (`download/opus/bloodmoon.opmod`) |
| sammi.erdtree | Project_Erdtree @ daa167c | 51,004 | `b9ffb3ad90f1d483da86bf1735468cb5430d85602284b6e5e7af182a401046c6` | no: same size, manifest and imports; 4,247 bytes differ (another compiler build) |
| sammi.sister | -Sister-Opus-OP-1-Port @ 44cf2f5 | 57,736 | `15de206e43aaad9883219d72157b4b73ffa6e2d7bfca6c2301f8ad4019fb34c0` | **identical** (`dist/darksister.opmod`) |
| sammi.reverb | Studder-and-Reverb-port-to-Opus @ b6e4877 | 7,080 | `54e6e311b5c0d90b521e253ae033a0a878bf533b735ccaad6869eeb62e527d64` | **identical** |
| sammi.stutter | Studder-and-Reverb-port-to-Opus @ b6e4877 | 11,568 | `3a6f268df050071c66278a09758c70e838632788673759327328519eee2ae8ff` | **identical** |
| noladrift.bronzechoir | The-Bronze-Choir @ b1868ff | 21,108 | `ddf8e215f7dea7f304bdc3843fe04808e238a29461eaeb1ca6854ba10fd5e30b` | no: the shipped file was linked with binutils 2.42 (known) |
| noladrift.rooms | Rooms-inspo-... @ f6d137e | 27,028 | `5484311dc1ae68222f768eea1837648b42e0768f2458656488564525950ecc11` | **identical** (`builds/noladrift.rooms.opmod`) |

`plugin_tool.py show` for each build:

| Plugin | deep (fw_* names) | ram image / zero | imports | cpu |
|---|---|---|---|---|
| sammi.hollow_saint | fw_node_set_matrix fw_screen_draw fw_svg_node_find | 56,408 / 14,188 | 11 | 230,000 |
| noladrift.bloodmoon | fw_node_set_matrix fw_screen_draw fw_snprintf fw_svg_node_find | 50,380 / 732 | 11 | 120,000 |
| sammi.erdtree | none | 47,664 / 504 | 21 | 200,000 |
| sammi.sister | fw_shift_held | 50,976 / 460 | 18 | 96,000 |
| sammi.reverb | fw_snprintf | 4,708 / 192 | 10 | unmeasured |
| sammi.stutter | fw_snprintf | 8,652 / 248 | 12 | unmeasured |
| noladrift.bronzechoir | fw_shift_held fw_snprintf | 16,548 / 1,928 | 15 | 140,000 |
| noladrift.rooms | none | 22,540 / 1,744 | 12 | unmeasured |

Note that `deep` lists only `fw_*` imports: Erdtree's and Stutter's raw address reads don't appear there.

## API audit against the same kit

```
python3 -I tools/opus_api_audit.py plugin-kit \
  Project_Hollow_Saint/opus/sammi.hollow_saint \
  Project_Bloodmoon/platforms/opus/src/noladrift.bloodmoon \
  Project_Erdtree/platforms/opus/src/sammi.erdtree \
  -Sister-Opus-OP-1-Port/plugin/sammi.sister \
  Studder-and-Reverb-port-to-Opus/plugins/sammi.reverb \
  Studder-and-Reverb-port-to-Opus/plugins/sammi.stutter \
  The-Bronze-Choir/engine/noladrift.bronzechoir \
  Rooms-inspo-from-Death-by-Audio-op-1-opus2/source/noladrift.rooms
```

Result: exit 0, "MISSING in kit: none" for all eight. Raw firmware addresses: Hollow Saint `0x01AADABC 0xFF801064
0xFF802970 0xFF802974 0xFFB00000`; Erdtree `0xFF801D50`; Stutter `0x01AADABC 0xFF802970 0xFF802974`; the rest
none. Assembly: Erdtree (inline), Sister (`sister_asm.S` + inline), ROOMS (inline).

The kit diff tool was checked against known history: `d9b0d2e` (beta 3) → `1de3ff4` (beta 3.2) reports both headers
"identical (code only)", and `65f50e5` (beta 2) → `d9b0d2e` reports the eight names beta 3 added (`opus_called`,
`opus_key_post`, `opus_on_key_filter`, `opus_replace`, `opus_sample*`, `OPUS_ENGINE_SAMPLE`), removed none.

## Not run in this pass

Host test suites, simulator cycle benchmarks and screen renders were not re-run here (the projects' own results
files record their last runs). They're section B of the checklist, to run on each migration branch.
