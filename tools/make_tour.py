#!/usr/bin/env python3
"""Build the tour video: a scripted performance through the real engine and screen code (tests/perform.c, host build),
its audio and screen frames, with captions, muxed with ffmpeg.

    python3 tools/make_tour.py [out.mp4]                 the full tour (default build/tour/hollow_saint_tour.mp4)
    python3 tools/make_tour.py --features [out.mp4]      0.7.0: Lo-Fi Relic, Crank & Tremor, then the 8 presets
                                                         (default build/tour/hollow_saint_0.7.0_features.mp4)
    python3 tools/make_tour.py --demo [out.mp4]          the engine and its knobs, then both of the above
                                                         (default build/tour/hollow_saint_<version>_demo.mp4)
    --stills DIR (with any mode)                         also save screen stills and a short GIF of the screen at
                                                         the moments in STILLS (the README's docs/media/)

What it is: the engine's own C code rendering audio on a PC, and its own screen code moving the SVG's nodes, drawn by
tools/render_screen.py's rasteriser. Not an OP-1 recording: no OP-1 FX, the stock envelope is approximated and the
OLED is drawn by a stand-in renderer.
Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
"""
import json, os, subprocess, sys, xml.etree.ElementTree as ET
from multiprocessing import Pool
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'plugin', 'hollow_saint'))
import render_screen as RS
import presets as P

KENNEL = os.environ.get('KENNEL', '/home/claude/ref/bundle/hundekraft-3.0.3-source/ext/kennel')
OUT_DIR = os.path.join(ROOT, 'build', 'tour')
SVG = os.path.join(ROOT, 'plugin', 'hollow_saint', 'hollow_saint.svg')
FPS = 30
TAGLINE = 'a haunted hurdy-gurdy for the original OP-1'
VW, VH = 1280, 720
SC, SS = 3, 2                                       # screen at 3x (960 x 480), drawn at 2x supersampling
SX, SY = (VW - 320 * SC) // 2, 34
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
FONTB = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'
COLS = [(0x4D, 0x9E, 0xFF), (0x00, 0xED, 0x95), (0xFF, 0xFF, 0xFF), (0xFF, 0x3A, 0x5D)]
KEYS = ['FOLLOW', 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
RATES = P.RATES                                     # steady, 1 bar, 1/2, 1/4, 1/8, 1/8T, 1/16
VERSION = json.load(open(os.path.join(ROOT, 'plugin', 'hollow_saint', 'plugin.json')))['version']
# --stills: (file, the caption the moment follows, seconds after that caption appears); .gif: (..., seconds long)
STILLS = [('screen_default.png', 'HOLLOW SAINT  the default sound', 3.0),
          ('screen_drone_key.png', 'push BLUE', 2.6),
          ('screen_stroke_rate.png', 'push WHITE  the stroke rate', 5.0),
          ('screen_cathedral.png', 'RED  CATHEDRAL', 12.2),
          ('screen_relic.png', 'turn GREEN while it is on', 5.0),
          ('screen_crank.png', 'WHITE middle, 1/8', 3.0),
          ('screen.gif', 'WHITE middle, 1/8', 0.6, 4.0)]


def ms(v):                                           # OP-1 ADSR word -> ms: an approximation for the host render
    return max(1.0, (v / 16320) ** 2 * 6000)


# ------------------------------------------------------------------------------------------------ the performance
class Script:
    def __init__(self):
        self.lines = []

    def at(self, t, *cmd):
        self.lines.append(f'{t:.3f} ' + ' '.join(str(c) for c in cmd))

    def preset(self, t, p):
        knobs, words = P.knob_words(p)
        a, d, s, r = p[4]
        self.at(t, 'adsr', f'{ms(a):.0f}', f'{ms(d):.0f}', s, f'{ms(r):.0f}')
        for i, v in enumerate(knobs):
            self.at(t, 'knob', i, v)
        self.at(t, 'word', 5, words[0]); self.at(t, 'word', 6, words[1]); self.at(t, 'word', 7, words[2])

    def note(self, t, n, dur, vel=110):
        self.at(t, 'on', n, vel); self.at(t + dur, 'off', n)


def title(S, hold=9.2, phrase=7):
    """the opening: a held D and a slow phrase under the title card; returns the end time"""
    S.note(0.4, 50, hold, 110)
    t = 1.6
    for n, d in ((62, .9), (65, .45), (64, .45), (62, .9), (60, .45), (62, 1.35), (57, .9))[:phrase]:
        S.note(t, n, d - 0.04, 105); t += d
    return t


TUNE = [(62, .5), (64, .25), (65, .25), (67, .5), (69, .5), (70, .25), (69, .25), (67, .5), (65, .5), (64, .5), (62, 1.0)]
SLOW = [(62, 1.0), (65, 1.0), (64, 1.0), (60, 1.0)]


def engine_section(S, t):
    """the default sound, then each knob and its push; returns the end time"""
    tune, slow = TUNE, SLOW
    S.at(t, 'caption', 'HOLLOW SAINT  the default sound: melody string, drone, wheel buzz, cathedral')
    t = play(S, t + 0.2, tune)
    t = play(S, t, tune)
    # DRONE
    S.at(t + 0.2, 'caption', 'BLUE  DRONE: off, subtle and dark, rich and resonant, dense with a sub-octave')
    S.at(t + 0.2, 'knob', 0, 0)
    S.at(t + 0.6, 'ramp', 0, 0, 32767, 11.0)
    t = play(S, t + 0.6, slow * 3, base=50)
    S.at(t + 0.1, 'caption', 'push BLUE  the drone\'s key: FOLLOW the lowest note, or a fixed key (here D)')
    S.at(t + 0.1, 'knob', 0, 22000); S.at(t + 0.1, 'page', 0, 1); S.at(t + 0.1, 'word', 5, 1260)
    S.at(t + 1.6, 'word', 5, 8823)
    t = play(S, t + 0.3, [(69, .5), (67, .5), (65, .5), (64, .5), (65, .5), (62, .5), (60, .5), (62, 1.0)], hold_bass=False)
    S.at(t, 'page', 0, 0)
    # STRINGS
    S.at(t + 0.2, 'caption', 'GREEN  STRINGS: soft hollow flute, nasal hurdy-gurdy, bright and cutting')
    S.at(t + 0.2, 'word', 5, 1260); S.at(t + 0.2, 'knob', 1, 0)
    S.at(t + 0.6, 'ramp', 1, 0, 32767, 12.0)
    t = play(S, t + 0.6, tune + tune + [(62, 2.0)])
    # WHEEL BUZZ
    S.at(t + 0.2, 'caption', 'WHITE  WHEEL BUZZ: smooth, then the dog lifts on each wheel stroke, in tempo')
    S.at(t + 0.2, 'knob', 1, 16384); S.at(t + 0.2, 'knob', 2, 0)
    S.at(t + 0.6, 'ramp', 2, 0, 32767, 13.0)
    t = play(S, t + 0.6, slow * 3 + [(62, 2.0)])
    S.at(t + 0.1, 'caption', 'push WHITE  the stroke rate: steady, 1 bar, 1/2, 1/4, 1/8, 1/8 triplet, 1/16')
    S.at(t + 0.1, 'knob', 2, 21000); S.at(t + 0.1, 'page', 2, 1)
    S.note(t + 0.1, 50, 9.8, 110); S.note(t + 0.1, 62, 9.8, 105)
    for j, r in enumerate(RATES):
        S.at(t + 0.1 + j * 1.4, 'word', 7, P.rate_word(r))
    t += 10.0
    S.at(t, 'page', 2, 0); S.at(t, 'word', 7, P.rate_word('1/8'))
    # CATHEDRAL
    S.at(t + 0.2, 'caption', 'RED  CATHEDRAL: dry and close, hollow and old-world, dark and expansive, a distant bell')
    S.at(t + 0.2, 'knob', 2, 14000); S.at(t + 0.2, 'knob', 3, 0)
    S.at(t + 0.2, 'adsr', 60, 400, 32767, 2600)
    S.at(t + 0.6, 'ramp', 3, 0, 32767, 13.0)
    tt = t + 0.6
    for n in (62, 69, 65, 72, 70, 67, 69, 62, 74, 69, 65, 62, 57):
        S.note(tt, n, 0.35, 112); tt += 1.0
    return tt + 2.2


def start():
    S = Script()
    S.at(0, 'bpm', 96)
    S.preset(0, P.PRESETS[0])
    S.at(0, 'word', 6, 8192)
    S.at(0, 'caption', '')
    return S


def finish(S, t):
    S.at(t, 'alloff')
    S.at(t, 'caption', 'END')
    S.at(t + 3.5, 'end')
    S.lines.sort(key=lambda l: float(l.split()[0]))
    return S.lines


def performance():
    """the original tour: the engine, then the presets"""
    S = start()
    title(S)
    t = engine_section(S, 10.0)
    return finish(S, presets_section(S, t))


def demo():
    """the full demo: the engine, the 0.7.0 features, then the presets"""
    S = start()
    title(S)
    t = engine_section(S, 10.0)
    t = features_section(S, t, intro='LO-FI RELIC  back to the default sound, clean')
    return finish(S, presets_section(S, t))


def presets_section(S, t):
    """the 8 presets, each with its own phrase; returns the end time"""
    phr = {
        'hollow saint':   [(62, .5), (64, .25), (65, .25), (67, .5), (65, .5), (64, .5), (62, 1.0)],
        'midnight vigil': [(62, 1.5), (65, 1.0), (64, 1.5), (62, 2.0)],
        'howling dog':    [(62, .25), (65, .25), (67, .25), (69, .25), (70, .5), (69, .25), (67, .25), (65, .5), (62, 1.0), (62, .5)],
        'ash procession': [(55, 1.0), (58, 1.0), (60, 1.0), (62, 1.0), (60, 1.0), (55, 1.0)],
        'bone flute':     [(62, .75), (64, .25), (65, 1.0), (67, .5), (69, 1.0), (65, 1.5)],
        'crypt drone':    [(60, 2.0), (63, 1.5), (62, 2.5)],
        'plague dance':   [(62, .25), (62, .25), (69, .25), (69, .25), (70, .25), (69, .25), (67, .5), (65, .25), (64, .25), (62, .5), (61, .25), (62, .75)],
        'drowned bell':   [(69, .5), (0, .5), (65, .5), (0, .5), (62, .5), (0, 1.0), (57, .5), (0, 1.5)],
    }
    bass = {'ash procession': 43, 'crypt drone': 48}
    for p in P.PRESETS:
        name = p[0]
        S.at(t, 'alloff')
        S.preset(t + 0.05, p)
        kn, w = P.knob_words(p)
        S.at(t + 0.05, 'caption', f'PRESET  {name}')
        o = 12 * p[7]
        tt = t + 0.3
        dur = sum(d for _, d in phr[name])
        S.note(tt, bass.get(name, 50) + o, dur + 0.2, 108)
        for n, d in phr[name]:
            if n:
                S.note(tt, n + o, d - 0.05, 106)
            tt += d
        t = tt + 1.6
    return t


def play(S, t0, tune, base=50, vel=108, hold_bass=True, legato=0.04):
    t = t0
    if hold_bass:
        S.note(t0, base, sum(d for _, d in tune) - 0.02, vel)
    for n, d in tune:
        S.note(t, n, d - legato, vel); t += d
    return t


def features():
    """0.7.0: the Lo-Fi Relic, Crank & Tremor, both together, then the presets"""
    S = start()
    title(S, hold=7.4, phrase=6)
    t = features_section(S, 8.4)
    return finish(S, presets_section(S, t))


def features_section(S, t, intro='the instrument as it is: no relic'):
    """the Lo-Fi Relic, Crank & Tremor, then both together; returns the end time"""
    hs = P.PRESETS[0]
    tune, slow = TUNE, SLOW
    # ---- LO-FI RELIC
    S.at(t, 'alloff')
    S.preset(t, hs)
    S.at(t, 'word', 6, 8192)
    S.at(t, 'caption', intro)
    t = play(S, t + 0.2, tune)
    S.at(t + 0.1, 'caption', 'push GREEN  LO-FI RELIC: an old tape in a forgotten room (age 25 %)')
    S.at(t + 0.1, 'page', 1, 1)
    t = play(S, t + 0.3, tune)
    S.at(t + 0.1, 'caption', 'turn GREEN while it is on: the relic ages (wow, grit, dust, a darker tape head)')
    t0 = t + 0.3
    t = play(S, t0, slow * 3)
    steps = int((t - t0) / 0.25)
    for j in range(steps + 1):                       # the age from 25 % to 100 %, in quarter-second steps
        S.at(t0 + j * 0.25, 'word', 6, int(8192 + (32767 - 8192) * j / steps))
    S.at(t + 0.1, 'caption', 'with CATHEDRAL up: the room hears the worn tape')
    S.at(t + 0.1, 'word', 6, 22000)
    S.at(t + 0.1, 'adsr', 60, 400, 32767, 2400)
    S.at(t + 0.2, 'ramp', 3, 12000, 30000, 3.0)
    tt = t + 0.4
    for n in (62, 69, 65, 72, 70, 67, 69, 62):
        S.note(tt, n, 0.4, 112); tt += 1.0
    t = tt + 1.2
    S.at(t, 'caption', 'push GREEN again: the clean instrument, the same room')
    S.at(t, 'page', 1, 0)
    tt = t + 0.2
    for n in (62, 69, 65, 72, 70, 67):
        S.note(tt, n, 0.4, 112); tt += 1.0
    t = tt + 1.6
    # ---- CRANK & TREMOR
    S.preset(t, hs)
    S.at(t, 'word', 6, 8192)
    S.at(t, 'knob', 0, 20000)
    S.at(t, 'page', 2, 1)
    parts = [(8192, '1 bar', 'CRANK & TREMOR  WHITE low, 1 bar: a slow, breathing swell on the wheel'),
             (8192, '1/2', 'WHITE low, 1/2: breathing twice a bar'),
             (13000, '1/4', 'WHITE toward the middle, 1/4: a steady mechanical tremolo'),
             (19000, '1/8', 'WHITE middle, 1/8: the dog starts to buzz on each stroke; the string\'s tone pulses'),
             (29491, '1/16', 'WHITE high, 1/16: faster, aggressive buzzing')]
    for w, r, cap in parts:
        S.at(t, 'caption', cap)
        S.at(t, 'knob', 2, w); S.at(t, 'word', 7, P.rate_word(r))
        S.note(t + 0.1, 43, 5.8, 110); S.note(t + 0.1, 50, 5.8, 104)
        tt = t + 0.6
        for n, d in ((62, 1.25), (65, 1.25), (64, 1.25), (62, 1.25)):
            S.note(tt, n, d - 0.05, 100); tt += d
        t += 6.2
    S.at(t, 'page', 2, 0)
    S.at(t, 'caption', 'push WHITE: steady stops the pulse')
    S.at(t, 'word', 7, P.rate_word('steady')); S.at(t, 'knob', 2, 14000)
    t = play(S, t + 0.2, slow) + 0.6
    # ---- both
    S.at(t, 'caption', 'BOTH  the relic and the wheel at 1/8, under a full cathedral')
    S.at(t, 'page', 1, 1); S.at(t, 'word', 6, 20000)
    S.at(t, 'knob', 2, 18000); S.at(t, 'word', 7, P.rate_word('1/8')); S.at(t, 'knob', 3, 26000)
    t = play(S, t + 0.2, tune + tune)
    S.at(t + 0.4, 'page', 1, 0)
    return t + 2.2


# ------------------------------------------------------------------------------------------------ frames
_static = None
_groups = None


def _prep():
    global _static, _groups
    root = ET.parse(SVG).getroot()
    W, H = 320 * SC * SS, 160 * SC * SS
    img = Image.new('RGB', (W, H), (0, 0, 0))
    dr = ImageDraw.Draw(img)
    _groups = []
    for el in root:
        if el.tag.split('}')[-1] == 'g':
            _groups.append(el)
        else:
            _draw(dr, el, (1, 0, 0, 1, 0, 0), {}, set())
    _static = img


def _draw(dr, el, m, nodes, hidden):
    import math
    tag = el.tag.split('}')[-1]
    if el.get('id') in hidden:
        return
    if tag == 'g':
        if el.get('id') in nodes:
            m = RS.mat_mul(RS.from_q10(nodes[el.get('id')]), m)
        for ch in el:
            _draw(dr, ch, m, nodes, hidden)
        return
    S = SC * SS
    stroke, fill = RS.parse_color(el.get('stroke')), RS.parse_color(el.get('fill', 'none'))
    w = float(el.get('stroke-width', '1'))
    sw = max(1, round(w * S * (abs(m[0] * m[3] - m[1] * m[2]) ** 0.5)))
    if tag == 'line':
        sub = [([(float(el.get('x1')), float(el.get('y1'))), (float(el.get('x2')), float(el.get('y2')))], False)]
    elif tag == 'path':
        sub = RS.path_subpaths(el.get('d'))
    elif tag == 'circle':
        cx, cy, r = float(el.get('cx')), float(el.get('cy')), float(el.get('r'))
        sub = [([(cx + r * math.cos(2 * math.pi * k / 40), cy + r * math.sin(2 * math.pi * k / 40)) for k in range(41)], True)]
    else:
        sub = []
    for pts, closed in sub:
        Pp = [(x * S, y * S) for x, y in (RS.apply(m, p) for p in pts)]
        if fill and closed and len(Pp) > 2:
            dr.polygon(Pp, fill=fill)
        if stroke:
            dr.line(Pp, fill=stroke, width=sw, joint='curve')
            r = sw / 2
            for x, y in (Pp[0], Pp[-1]):
                dr.ellipse([x - r, y - r, x + r, y + r], fill=stroke)


def _frame(args):
    i, fr, title_alpha, out = args
    if _static is None:
        _prep()
    img = _static.copy()
    dr = ImageDraw.Draw(img)
    hidden = set(fr['hidden'])
    for g in _groups:
        _draw(dr, g, (1, 0, 0, 1, 0, 0), fr['nodes'], hidden)
    scr = img.resize((320 * SC, 160 * SC), Image.LANCZOS)
    V = Image.new('RGB', (VW, VH), (11, 11, 12))
    vd = ImageDraw.Draw(V)
    vd.rounded_rectangle([SX - 18, SY - 14, SX + 320 * SC + 18, SY + 160 * SC + 14], radius=18, fill=(24, 24, 26), outline=(44, 44, 48), width=2)
    V.paste(scr, (SX, SY))
    f_cap, f_k, f_s = ImageFont.truetype(FONT, 22), ImageFont.truetype(FONTB, 19), ImageFont.truetype(FONT, 14)
    cap = fr['caption']
    if cap and cap != 'END':
        vd.text((VW // 2, 556), cap, font=f_cap, fill=(235, 235, 235), anchor='mm')
    k, w, pg = fr['knobs'], fr['words'], fr['pages']
    labels = [f'DRONE {k[0] * 100 // 32767:3d}%', f'STRINGS {k[1] * 100 // 32767:3d}%',
              f'WHEEL BUZZ {k[2] * 100 // 32767:3d}%', f'CATHEDRAL {k[3] * 100 // 32767:3d}%']
    labels[0] += f'  {KEYS[(max(0, w[0]) * 13) >> 15]}'
    labels[2] += f'  {RATES[(max(0, w[2]) * len(RATES)) >> 15]}'
    if pg[1]:
        age = w[1] if w[1] > 0 else 8192
        labels[1] = f'RELIC age {age * 100 // 32767:3d}%'
    xs = [170, 470, 790, 1110]
    for j in range(4):
        vd.text((xs[j], 604), labels[j], font=f_k, fill=COLS[j], anchor='mm')
    vd.text((VW // 2, 690), 'host render of the hollow_saint engine and screen code (not recorded on an OP-1; OP-1 FX not '
            'simulated)', font=f_s, fill=(110, 110, 115), anchor='mm')
    if title_alpha > 0 or cap == 'END':
        a = 1.0 if cap == 'END' else title_alpha
        ov = Image.new('RGB', (VW, VH), (0, 0, 0))
        V = Image.blend(V, ov, 0.72 * a)
        vd = ImageDraw.Draw(V)
        c = tuple(int(255 * a) for _ in range(3))
        r = tuple(int(v * a) for v in COLS[3])
        g = tuple(int(150 * a) for _ in range(3))
        vd.text((VW // 2, 300), 'CATHEDRAL OF THE HOLLOW SAINT', font=ImageFont.truetype(FONTB, 46), fill=c, anchor='mm')
        vd.text((VW // 2, 360), TAGLINE, font=ImageFont.truetype(FONT, 24), fill=r, anchor='mm')
        sub = ('kennel synth plugin  hollow_saint  for the Hundekraft OP-1 Workshop 3.0.3' if cap != 'END'
               else 'drone  strings  wheel buzz  cathedral  /  lo-fi relic  crank & tremor  /  8 presets  /  '
                    'GPL-3.0-or-later')
        vd.text((VW // 2, 410), sub, font=ImageFont.truetype(FONT, 18), fill=g, anchor='mm')
    V.save(out)
    return i


def main():
    global TAGLINE
    args = sys.argv[1:]
    stills = None
    if '--stills' in args:
        j = args.index('--stills')
        stills = args[j + 1]
        del args[j:j + 2]
    mode = next((a for a in args if a in ('--features', '--demo')), '')
    args = [a for a in args if a not in ('--features', '--demo')]
    perf, name = {'--features': (features, 'hollow_saint_0.7.0_features.mp4'),
                  '--demo': (demo, f'hollow_saint_{VERSION}_demo.mp4')}.get(mode, (performance, 'hollow_saint_tour.mp4'))
    if mode == '--features':
        TAGLINE = '0.7.0: lo-fi relic  /  crank & tremor  /  the presets'
    elif mode == '--demo':
        TAGLINE = f'{VERSION}: the engine  /  lo-fi relic  /  crank & tremor  /  the presets'
    mp4 = args[0] if args else os.path.join(OUT_DIR, name)
    os.makedirs(OUT_DIR, exist_ok=True)
    fdir = os.path.join(OUT_DIR, 'frames')
    os.makedirs(fdir, exist_ok=True)
    for f in os.listdir(fdir):
        os.remove(os.path.join(fdir, f))
    script = os.path.join(OUT_DIR, 'tour.txt')
    open(script, 'w').write('\n'.join(perf()) + '\n')
    exe = os.path.join(OUT_DIR, 'perform')
    sdk = os.path.join(KENNEL, 'synth_sdk')
    subprocess.run(['cc', '-O2', '-DOP1_HOST', '-DENGINE=ENGINE_hollow_saint', '-DSCREEN=SCREEN_hollow_saint',
                    '-I', os.path.join(ROOT, 'tests', 'host_sdk'), '-I', sdk, '-I', os.path.join(ROOT, 'plugin', 'hollow_saint'),
                    os.path.join(ROOT, 'tests', 'perform.c'), os.path.join(ROOT, 'plugin', 'hollow_saint', 'hollow_saint.c'),
                    '-lm', '-o', exe], check=True)
    base = os.path.join(OUT_DIR, 'tour')
    subprocess.run([exe, script, base], check=True)
    frames = [json.loads(l) for l in open(base + '.jsonl')]
    jobs = []
    for i, fr in enumerate(frames):
        t = fr['t']
        ta = 1.0 if t < 8.0 else max(0.0, 1.0 - (t - 8.0) / 1.5)
        jobs.append((i, fr, ta, os.path.join(fdir, f'f{i:05d}.png')))
    with Pool(os.cpu_count() or 2) as pool:
        for n, _ in enumerate(pool.imap_unordered(_frame, jobs, chunksize=16)):
            if n % 600 == 0:
                print(f'frames {n}/{len(jobs)}', flush=True)
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(fdir, 'f%05d.png'),
                    '-i', base + '.wav', '-c:v', 'libx264', '-preset', 'medium', '-crf', '20', '-pix_fmt', 'yuv420p',
                    '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', mp4], check=True)
    if stills:
        save_stills(frames, fdir, stills)
    print(mp4)


def save_stills(frames, fdir, out):
    """the screen (with its bezel) cropped from the frames at the moments in STILLS"""
    os.makedirs(out, exist_ok=True)
    box = (SX - 18, SY - 14, SX + 320 * SC + 18, SY + 160 * SC + 14)
    for name, cap, after, *dur in STILLS:
        t0 = next(fr['t'] for fr in frames if fr['caption'].startswith(cap)) + after
        i0 = next(i for i, fr in enumerate(frames) if fr['t'] >= t0)
        dst = os.path.join(out, name)
        if not dur:
            Image.open(os.path.join(fdir, f'f{i0:05d}.png')).crop(box).save(dst, optimize=True)
        else:                                        # every other frame (15 fps), half size, one shared palette
            x0, y0, x1, y1 = box
            subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-start_number', str(i0),
                            '-i', os.path.join(fdir, 'f%05d.png'), '-frames:v', str(int(dur[0] * FPS)), '-vf',
                            f'crop={x1 - x0}:{y1 - y0}:{x0}:{y0},fps=15,scale={(x1 - x0) // 2}:-1:flags=lanczos,'
                            'split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse=dither=none', dst], check=True)
        print(dst)


if __name__ == '__main__':
    main()
