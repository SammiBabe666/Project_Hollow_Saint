# hollow_saint 0.7.0: Blackfin build and verification (2026-10-22T19:25Z; 0.6.x figures kept for comparison)

## Toolchain
bfin-elf-gcc (GCC) 13.3.0; GNU ld (GNU Binutils) 2.42. Built from git sources by toolchain/build_toolchain.sh.
Reproduces Hundekraft's official build: these plugins, packed from the hundekraft-3.0.3 source bundle with this
toolchain, are byte-for-byte identical to the files the Workshop publishes (op1-workshop/variants/7ff/content_*.knl):
mbira, eva, supersaw, oxbow, taffy, wavetable, fmbell, father, crush, plate, stutter (11 of 11 tried).

## The package
release/hollow_saint.knl: 40,548 bytes, sha256 a3feff471728cc199aabda0ba2fd931b5433b33ec71b65c84a2d80ca176ddd58
text 16,552 + rodata 1,652 + data 20 bytes, bss 14,088 bytes, SVG 16,935 bytes, 431 load-time relocations,
(0.6.1: 33,556 bytes, sha256 bf125663a8843cf78ba50efc82144ef7f2e929200a1e1c2a9347ea0c0e01b3ac)
one import: _op1_eng_page (exported by the 3.0.3 core, kn_exports.c). Flags: page-2 defaults. Packed by the SDK's
tools/kennel/pack.py (SVG checks, relocation self-checks, import checks all passed).

## Loader
The core's own loader code (synth_sdk/kennel/kn_load.c, compiled for the PC, tests/kn_load_host.c) accepts the file
and relocates it to exactly the same image as the reference loader (tools/kennel/reloc.py) at 0x02980000 and
0x0299F000, with the core's full export table. Structural fields match erdtree.knl (format 1, API 1, header 144,
synth, order 0, align 4, SVG-prefix layout): tools/compare_knl.py.

## The compiled code, run
In the Blackfin instruction-set simulator (binutils-gdb sim/bfin, model bf524), bare-metal, with the engine at the
firmware's fixed addresses (tests/bfin_sim/): its audio for 6 voices x 4 knob settings x 60 blocks (184,320 samples; the
fourth: all knobs at maximum with the Lo-Fi Relic on at its oldest and the wheel at 1/16)
is bit-identical to the host build of the same source, so the host test suite's results carry over to the Blackfin
code. The simulated object's .text/.rodata/.data are identical to the packer's (build/knl/objects).
0.6.1's audio was bit-identical to 0.6.0's on the same workload; 0.7.0 differs there only through the wheel (now 7
rates, so the workload's word 7 lands on 1/4 with Crank & Tremor) and the relic setting.
Stack: hs_render 672 bytes + at most 60 in a called loop (-fstack-usage; 0.6.0: 700; EVA's render 740, MBIRA's 800).

## Cycles (simulator cycle count per 128-sample voice block, same harness)
| engine | mid | max | zero |
|---|---|---|---|
| hollow_saint 0.7.0 | 20,254 | 22,209 | 13,810 |
| hollow_saint 0.7.0, relic on (oldest), max | | 28,602 | |
| hollow_saint 0.6.1 | 19,705 | 21,715 | 13,556 |
| hollow_saint 0.6.0 | 29,675 | 34,969 | 19,626 |
| eva (ships) | 18,877 | 20,286 | 17,319 |
| wavetable (ships) | 27,437 | 27,437 | 27,437 |
| taffy (ships) | 39,467 | 18,997 | 23,176 |
Six voices at hollow_saint 0.7.0's worst setting: ~46 M cycles/s, ~11.5 % of a 400 MHz core by the simulator's model
(which does not model every stall); relic on at its oldest: ~59 M, ~15 % (0.6.0: ~72 M, ~18 %). 1.09 x EVA worst,
1.41 x with the relic; below Wavetable and Taffy, which ship.

## Not done
Not loaded through the Workshop, not run on an OP-1.
