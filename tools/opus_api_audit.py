#!/usr/bin/env python3
"""Check Opus plugins against an Opus plugin kit's SDK headers.

    python3 -I tools/opus_api_audit.py KIT_PLUGIN_KIT_DIR PLUGIN_DIR [PLUGIN_DIR ...]

For each plugin folder (the one holding manifest.txt) it lists:
  - the manifest's id, version and `core` line;
  - every opus_* / OPUS_* / fw_* / opl_* name its .c/.h/.S files use, and whether the kit's
    sdk/opus_plugin.h or sdk/op1fw.h still has it (MISSING = would fail to build or load);
  - raw firmware addresses (0xFF8xxxxx, 0xFFBxxxxx, 0x01xxxxxx) the code reads directly: the
    core does not resolve these, so they only stay right while the firmware's memory layout does;
  - inline or file assembly.
Exit status 1 if any name is missing. Reads files only; changes nothing.
"""
import re, sys, pathlib

NAME = re.compile(r'\b(?:opus_|OPUS_|fw_|opl_)[A-Za-z0-9_]+')
ADDR = re.compile(r'\b(0x(?:FF[89AB][0-9A-F]{5}|01[0-9A-F]{6}))[uUlL]*\b', re.I)
ASM = re.compile(r'__asm__|\basm\s*(?:volatile\s*)?\(')

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', ' ', s)
    # #include lines and #if/#ifdef conditions name files and build switches (OPUS_HOST), not imports
    return re.sub(r'^\s*#\s*(?:include|if|ifdef|ifndef|elif|endif)\b[^\n]*', ' ', s, flags=re.M)

def sources(folder):
    """The plugin folder's .c/.h/.S files, plus any file they #include "by/relative/path" from outside it
    (ROOMS and Hollow Saint compile shared DSP from another folder into the plugin)."""
    todo = [f for f in sorted(folder.iterdir()) if f.suffix in ('.c', '.h', '.S')]
    seen = []
    while todo:
        f = todo.pop(0).resolve()
        if f in seen or not f.is_file():
            continue
        seen.append(f)
        for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', f.read_text(errors='replace'), re.M):
            todo.append(f.parent / inc)
    return seen

def main():
    if len(sys.argv) < 3:
        print(__doc__); return 2
    kit = pathlib.Path(sys.argv[1])
    sdk = ''.join(strip_comments((kit / 'sdk' / h).read_text(errors='replace'))
                  for h in ('opus_plugin.h', 'op1fw.h'))
    have = set(NAME.findall(sdk))
    bad = 0
    for p in map(pathlib.Path, sys.argv[2:]):
        man = (p / 'manifest.txt').read_text(errors='replace')
        field = lambda k: (re.search(r'^%s\s+(.+)$' % k, man, re.M) or [None, '?'])[1].strip()
        used, addrs, asm = set(), set(), []
        local = set()
        for f in sources(p):
            code = strip_comments(f.read_text(errors='replace'))
            used |= set(NAME.findall(code))
            addrs |= {a.upper().replace('0X', '0x') for a in ADDR.findall(code)}
            if f.suffix == '.S' or ASM.search(code):
                asm.append(f.name if f.parent == p.resolve() else str(f.relative_to(p.resolve().parent.parent)))
            # names the plugin defines itself (its own macros, statics, structs) are not imports
            local |= set(re.findall(r'#define\s+((?:opus_|OPUS_|fw_|opl_)\w+)', code))
            local |= set(re.findall(r'\b(?:static|struct|typedef)\b[^;{(]*?\b((?:opus_|OPUS_|fw_|opl_)\w+)', code))
        imports = sorted(used - local)
        missing = [n for n in imports if n not in have and n != 'opl_init']
        bad += len(missing)
        print(f'== {field("id")} {field("version")} (core {field("core")}) {p}')
        print('   names: ' + ' '.join(imports))
        print('   MISSING in kit: ' + (' '.join(missing) if missing else 'none'))
        print('   raw firmware addresses: ' + (' '.join(sorted(addrs)) or 'none'))
        print('   assembly: ' + (' '.join(asm) or 'none'))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
