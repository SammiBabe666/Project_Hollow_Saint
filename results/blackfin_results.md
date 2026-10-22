# hollow_saint 0.6.0: Blackfin build and verification (2026-10-22T15:33Z)

## Toolchain
bfin-elf-gcc (GCC) 13.3.0; GNU ld (GNU Binutils) 2.42. Built from git sources by toolchain/build_toolchain.sh.
Reproduces Hundekraft's official build: these plugins, packed from the hundekraft-3.0.3 source bundle with this
toolchain, are byte-for-byte identical to the files the Workshop publishes (op1-workshop/variants/7ff/content_*.knl):
mbira, eva, supersaw, oxbow, taffy, wavetable, fmbell, father, crush, plate, stutter (11 of 11 tried).

## The package
release/hollow_saint.knl: 31,516 bytes, sha256 43f9805b1f21c34209e4adab4cf93bb0a47747693d40545a333aaae6e173da5a
text 11,468 + rodata 1,188 + data 12 bytes, bss 13,372 bytes, SVG 14,060 bytes, 381 load-time relocations,
one import: _op1_eng_page (exported by the 3.0.3 core, kn_exports.c). Flags: page-2 defaults. Packed by the SDK's
tools/kennel/pack.py (SVG checks, relocation self-checks, import checks all passed).

## Loader
The core's own loader code (synth_sdk/kennel/kn_load.c, compiled for the PC, tests/kn_load_host.c) accepts the file
and relocates it to exactly the same image as the reference loader (tools/kennel/reloc.py) at 0x02980000 and
0x0299F000, with the core's full export table. Structural fields match erdtree.knl (format 1, API 1, header 144,
synth, order 0, align 4, SVG-prefix layout): tools/compare_knl.py.

## The compiled code, run
In the Blackfin instruction-set simulator (binutils-gdb sim/bfin, model bf524), bare-metal, with the engine at the
firmware's fixed addresses (tests/bfin_sim/): its audio for 6 voices x 3 knob settings x 60 blocks (138,240 samples)
is bit-identical to the host build of the same source, so the host test suite's results carry over to the Blackfin
code. The simulated object's .text/.rodata/.data are identical to the packer's (build/knl/objects).
Stack: hs_render 700 bytes (EVA's render 740, MBIRA's 800).

## Cycles (simulator cycle count per 128-sample voice block, same harness)
| engine | mid | max | zero |
|---|---|---|---|
| hollow_saint | 29,675 | 34,969 | 19,626 |
| eva (ships) | 18,877 | 20,286 | 17,319 |
| wavetable (ships) | 27,437 | 27,437 | 27,437 |
| taffy (ships) | 39,467 | 18,997 | 23,176 |
Six voices at hollow_saint's worst setting: ~72 M cycles/s, ~18 % of a 400 MHz core by the simulator's model (which
does not model every stall). Between Wavetable and Taffy, which ship.

## Not done
Not loaded through the Workshop, not run on an OP-1.
