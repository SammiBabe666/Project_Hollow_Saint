#!/usr/bin/env python3
"""Count distinct 32-byte lines per voice render and per block from a lackey trace (render segments between mark calls).
usage: lines.py <trace> <nm output> <warmup blocks>  (tests/mem_cost/run.sh)"""
import sys, collections, re
trace, nmf, warm = sys.argv[1], sys.argv[2], int(sys.argv[3])
syms = {}
for l in open(nmf):
    p = l.split()
    if len(p) == 4: syms[p[3]] = (int(p[0], 16), int(p[1], 16))
mark_addr = syms['probe_mark'][0]
regions = [(n, a, a + s) for n, (a, s) in syms.items() if s >= 64]
def region(a):
    for n, lo, hi in regions:
        if lo <= a < hi: return n
    return 'stack' if a > 0x7f0000000000 else 'other'
inrender = False; cur = None; per_voice = []; blocks = collections.defaultdict(lambda: (set(), set()))
for line in open(trace):
    if line.startswith('=='): continue
    t = line[:2].strip(); rest = line[3:].strip()
    if not rest: continue
    a = int(rest.split(',')[0], 16)
    if t in ('S', 'M') and a == mark_addr:
        if cur is not None:
            per_voice.append(cur)
            cur = None
        else:
            cur = {'mark': None, 'I': set(), 'D': collections.defaultdict(set)}
        continue
    if cur is None: continue
    if t == 'I': cur['I'].add(a >> 5)
    else:
        r = region(a)
        cur['D'][r].add(a >> 5)
# per_voice: list in order; group 6 per block
nv = 6
pv = per_voice[warm * nv:]
tot = collections.Counter(); totI = 0; blockD = []; blockI = []
for i in range(0, len(pv) - nv + 1, nv):
    bd = set(); bi = set()
    for v in pv[i:i + nv]:
        bi |= v['I']
        for r, s in v['D'].items():
            if r != 'stack': bd |= s
    blockD.append(len(bd)); blockI.append(len(bi))
for v in pv:
    totI += len(v['I'])
    for r, s in v['D'].items(): tot[r] += len(s)
n = len(pv)
print('per voice render, distinct 32-byte lines (mean):', ' '.join(f'{r} {c / n:.0f}' for r, c in tot.most_common()), f'| code {totI / n:.0f}')
print(f'per block (6 voices, union): data {sum(blockD) / len(blockD):.0f} (max {max(blockD)}), code {sum(blockI) / len(blockI):.0f}')
print(f'model at 107 cycles a line: per-voice-sum data {sum(c for r, c in tot.items() if r != "stack") / n * 6 * 107:.0f}, block-union data {max(blockD) * 107}')
