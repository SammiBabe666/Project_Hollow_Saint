#!/usr/bin/env python3
"""Compare kennel plugin files field by field with the SDK's reference parser (tools/kennel/reloc.py).

    KENNEL=/path/to/ext/kennel python3 tools/compare_knl.py reference.knl new.knl [...]

Prints the header and metadata of each file side by side and flags the fields that must agree for any plugin of the
same kind and core (format, API, header size, kind, legacy order, alignment, SVG-prefix layout).
Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
"""
import os, sys
K = os.environ.get('KENNEL', '/home/claude/ref/bundle/hundekraft-3.0.3-source/ext/kennel')
sys.path.insert(0, os.path.join(K, 'tools', 'kennel'))
import reloc

MUST = ['format_version', 'api_version', 'header_size', 'kind', 'legacy_order', 'image_align']
SHOW = MUST + ['type_name', 'flags', 'file_size', 'svg_size', 'header_offset', 'text_size', 'rodata_size', 'data_size',
               'bss_size', 'image_size', 'load_size', 'entry_offset', 'screen_offset', 'reloc_count', 'import_count']


def info(path):
    p = reloc.parse(open(path, 'rb').read())
    row = {k: p.get(k) for k in SHOW}
    row['imports'] = ', '.join(p['imports']) or '-'
    row['defaults'] = p['defaults']
    row['page2_defaults'] = p['page2_defaults']
    row['claims'] = p['claims']
    ext = p.get('ext', {})
    row['author'] = ext.get(2, b'').rstrip(b'\0').decode('ascii', 'replace') or '-'
    row['version'] = ext.get(3, b'').rstrip(b'\0').decode('ascii', 'replace') or '-'
    return row


def main(paths):
    rows = [info(p) for p in paths]
    keys = list(rows[0])
    w = max(len(os.path.basename(p)) for p in paths) + 2
    print(f'{"field":16s}' + ''.join(f'{os.path.basename(p):>{max(w, 22)}s}' for p in paths))
    ok = True
    for k in keys:
        vals = [str(r[k]) for r in rows]
        flag = ''
        if k in MUST:
            same = len(set(vals)) == 1
            ok &= same
            flag = '  = must match' + ('' if same else '  <-- DIFFERS')
        print(f'{k:16s}' + ''.join(f'{v[:40]:>{max(w, 22)}s}' for v in vals) + flag)
    print('structural fields agree' if ok else 'STRUCTURAL MISMATCH')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
