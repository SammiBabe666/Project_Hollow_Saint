#!/usr/bin/env python3
"""Compare two Opus plugin kits' SDK headers, comments stripped, so a new beta's real API changes show.

    python3 -I tools/opus_kit_diff.py OLD_KIT/plugin-kit NEW_KIT/plugin-kit

Prints, for sdk/opus_plugin.h and sdk/op1fw.h: the opus_*/OPUS_*/fw_*/opl_* names added and removed,
then a unified diff of the declarations (whitespace normalised). It also says whether the build tool
(tools/op1mod/*.py, plugin_tool.py) and docs/manifest.md changed. Reads files only.
"""
import difflib, hashlib, pathlib, re, sys

NAME = re.compile(r'\b(?:opus_|OPUS_|fw_|opl_)[A-Za-z0-9_]+')

def code(path):
    if not path.exists():
        return []
    s = path.read_text(errors='replace')
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', ' ', s)
    lines = [re.sub(r'\s+', ' ', l).strip() for l in s.splitlines()]
    return [l for l in lines if l]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()[:12] if path.exists() else 'absent'

def main():
    if len(sys.argv) != 3:
        print(__doc__); return 2
    old, new = map(pathlib.Path, sys.argv[1:])
    changed = 0
    for h in ('sdk/opus_plugin.h', 'sdk/op1fw.h'):
        a, b = code(old / h), code(new / h)
        na, nb = set(NAME.findall(' '.join(a))), set(NAME.findall(' '.join(b)))
        print(f'== {h}: {"identical (code only)" if a == b else "CHANGED"}')
        if a != b:
            changed = 1
            print('   added:   ' + (' '.join(sorted(nb - na)) or 'none'))
            print('   removed: ' + (' '.join(sorted(na - nb)) or 'none'))
            for l in difflib.unified_diff(a, b, 'old/' + h, 'new/' + h, n=0, lineterm=''):
                print('   ' + l)
    for f in ['plugin_tool.py', 'docs/manifest.md', 'docs/rules.md'] + \
             sorted({p.relative_to(k).as_posix() for k in (old, new) for p in (k / 'tools/op1mod').glob('*.py')}):
        da, db = digest(old / f), digest(new / f)
        print(f'-- {f}: {"same" if da == db else f"differs ({da} -> {db})"}')
    return changed

if __name__ == '__main__':
    sys.exit(main())
