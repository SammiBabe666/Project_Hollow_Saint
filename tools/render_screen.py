#!/usr/bin/env python3
"""Rasterise an OP-1 screen SVG (the native subset) with node transforms / visibility from the screen harness.

    python3 tools/render_screen.py screen.svg out.png [frame.json] [--scale 2]

Draws only what R246's renderer supports and the plugin uses: <g> (id; transforms come from the harness), <line>,
<polyline>, <polygon>, <circle>, <path> (M L H V C Z, absolute and relative), each element with its own stroke /
stroke-width / fill. frame.json: {"nodes": {"id": [9 x Q10, row vectors as op1_node_set_transform], ...},
"hidden": ["id", ...]}. A preview, not the OP-1: check the real screen on hardware or in OP1-Emulator.
Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
"""
import json, re, sys, xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

SS = 4                                            # supersampling


def parse_color(c):
    if not c or c == 'none':
        return None
    c = c.strip()
    if c.startswith('#') and len(c) == 7:
        return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))
    raise ValueError(f'colour {c!r}: only #rrggbb')


NUM = re.compile(r'-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?')


def path_subpaths(d):
    """-> [(points, closed)], cubic curves flattened"""
    out, cur, pos, start, closed = [], [], (0.0, 0.0), (0.0, 0.0), False
    for cmd, args in re.findall(r'([MLHVCZmlhvcz])([^MLHVCZmlhvcz]*)', d):
        n = [float(v) for v in NUM.findall(args)]
        rel = cmd.islower()
        C = cmd.upper()
        if C == 'Z':
            if cur:
                cur.append(start); out.append((cur, True)); cur = []
            pos = start
            continue
        if C == 'M':
            if len(cur) > 1:
                out.append((cur, False))
            x, y = n[0] + (pos[0] if rel else 0), n[1] + (pos[1] if rel else 0)
            pos = start = (x, y); cur = [pos]
            n = n[2:]
            C = 'L'
        if C == 'L':
            for i in range(0, len(n), 2):
                x, y = n[i] + (pos[0] if rel else 0), n[i + 1] + (pos[1] if rel else 0)
                pos = (x, y); cur.append(pos)
        elif C == 'H':
            for v in n:
                pos = (v + (pos[0] if rel else 0), pos[1]); cur.append(pos)
        elif C == 'V':
            for v in n:
                pos = (pos[0], v + (pos[1] if rel else 0)); cur.append(pos)
        elif C == 'C':
            for i in range(0, len(n), 6):
                p0 = pos
                p1, p2, p3 = [(n[i + j] + (pos[0] if rel else 0), n[i + j + 1] + (pos[1] if rel else 0)) for j in (0, 2, 4)]
                for k in range(1, 17):
                    t = k / 16
                    a, b, c, e = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
                    cur.append((a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1]))
                pos = p3
    if len(cur) > 1:
        out.append((cur, False))
    return out


def mat_mul(m, n):
    """row-vector affine matrices as (a, b, c, d, tx, ty): x' = x a + y c + tx, y' = x b + y d + ty; m then n"""
    a, b, c, d, e, f = m
    A, B, C, D, E, F = n
    return (a * A + b * C, a * B + b * D, c * A + d * C, c * B + d * D, e * A + f * C + E, e * B + f * D + F)


def from_q10(m9):
    return tuple(v / 1024 for v in (m9[0], m9[1], m9[3], m9[4], m9[6], m9[7]))


def apply(m, p):
    return (p[0] * m[0] + p[1] * m[2] + m[4], p[0] * m[1] + p[1] * m[3] + m[5])


def render(svg_path, out_png, frame=None, scale=2):
    frame = frame or {}
    nodes, hidden = frame.get('nodes', {}), set(frame.get('hidden', []))
    root = ET.parse(svg_path).getroot()
    W, H = 320, 160
    S = scale * SS
    img = Image.new('RGB', (W * S, H * S), (0, 0, 0))
    dr = ImageDraw.Draw(img)

    def draw(el, m):
        tag = el.tag.split('}')[-1]
        if el.get('id') in hidden:
            return
        if tag in ('svg', 'g'):
            if el.get('id') in nodes:
                m = mat_mul(from_q10(nodes[el.get('id')]), m)
            for ch in el:
                draw(ch, m)
            return
        stroke, fill = parse_color(el.get('stroke')), parse_color(el.get('fill', 'none'))
        w = float(el.get('stroke-width', '1'))
        sw = max(1, round(w * S * (abs(m[0] * m[3] - m[1] * m[2]) ** 0.5)))
        sub = []
        if tag == 'line':
            sub = [([(float(el.get('x1')), float(el.get('y1'))), (float(el.get('x2')), float(el.get('y2')))], False)]
        elif tag in ('polyline', 'polygon'):
            n = [float(v) for v in NUM.findall(el.get('points'))]
            pts = list(zip(n[0::2], n[1::2]))
            sub = [(pts + ([pts[0]] if tag == 'polygon' else []), tag == 'polygon')]
        elif tag == 'path':
            sub = path_subpaths(el.get('d'))
        elif tag == 'circle':
            import math
            cx, cy, r = float(el.get('cx')), float(el.get('cy')), float(el.get('r'))
            pts = [(cx + r * math.cos(2 * math.pi * k / 48), cy + r * math.sin(2 * math.pi * k / 48)) for k in range(49)]
            sub = [(pts, True)]
        for pts, closed in sub:
            P = [(x * S, y * S) for x, y in (apply(m, p) for p in pts)]
            if fill and closed and len(P) > 2:
                dr.polygon(P, fill=fill)
            if stroke:
                dr.line(P, fill=stroke, width=sw, joint='curve')
                if el.get('stroke-linecap') == 'round':
                    r = sw / 2
                    for x, y in (P[0], P[-1]):
                        dr.ellipse([x - r, y - r, x + r, y + r], fill=stroke)

    draw(root, (1, 0, 0, 1, 0, 0))
    img = img.resize((W * scale, H * scale), Image.LANCZOS)
    img.save(out_png)
    return img


if __name__ == '__main__':
    a = sys.argv[1:]
    sc = 2
    if '--scale' in a:
        i = a.index('--scale'); sc = int(a[i + 1]); del a[i:i + 2]
    fr = json.load(open(a[2])) if len(a) > 2 else None
    render(a[0], a[1], fr, sc)
    print(a[1])
