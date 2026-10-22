#!/usr/bin/env python3
"""Host checks for the hollow_saint engine (no OP-1, no Blackfin toolchain needed).

    python3 tests/host.py phrase   [--knobs a,b,c,d]   # the SDK's standard phrase render (op1r harness/audio_harness.c)
    python3 tests/host.py suite                        # polyphony, key range, knob sweeps, stealing, extremes (tests/suite.c)
    python3 tests/host.py bench                        # relative CPU cost vs. shipped kennel engines (-O2, no sanitizers)

Compiles against the kennel 3.0.3 SDK header (synth_sdk/op1_plugin.h, -DOP1_HOST) with the op1r host mocks in
tests/host_sdk/harness/op1_host.h. phrase and suite build with -fsanitize=address,undefined and
-fno-sanitize-recover=undefined, as the SDK's harness/audio.py does: any out-of-bounds access or signed overflow
aborts the run.
Set KENNEL to the kennel folder of the unpacked hundekraft-3.0.3-source bundle (default: the path used here).
"""
import argparse, json, os, subprocess, sys, wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PLUGIN = os.path.join(ROOT, 'plugin', 'hollow_saint')
KENNEL = os.environ.get('KENNEL', '/home/claude/ref/bundle/hundekraft-3.0.3-source/ext/kennel')
SDK = os.path.join(KENNEL, 'synth_sdk')
OUT = os.path.join(ROOT, 'build', 'host')
SAN = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined']
WARN = ['-Wall', '-Wextra', '-Wno-unused-parameter', '-Werror=implicit-function-declaration']


def meta():
    return json.load(open(os.path.join(PLUGIN, 'plugin.json')))


def cc(srcs, exe, flags, defs):
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    cmd = ['cc', *flags, *WARN, '-DOP1_HOST', *defs, '-I', os.path.join(HERE, 'host_sdk'), '-I', SDK, *srcs, '-lm', '-o', exe]
    subprocess.run(cmd, check=True)


def write_wav(path, raw):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(raw)


def phrase(knobs):
    m = meta()
    exe = os.path.join(OUT, 'phrase')
    cc([os.path.join(HERE, 'host_sdk', 'harness', 'audio_harness.c'), *(os.path.join(PLUGIN, s) for s in m['sources'])],
       exe, SAN, [f'-DENGINE={m["symbol"]}'])
    r = subprocess.run([exe, *map(str, knobs or m['defaults'])], capture_output=True)
    sys.stderr.write(r.stderr.decode())
    out = os.path.join(OUT, 'phrase.wav')
    write_wav(out, r.stdout)
    print(f'phrase: knobs {knobs or m["defaults"]} -> {out} ({len(r.stdout) / 88200:.1f} s), exit {r.returncode}')
    return r.returncode


def suite(only=None):
    m = meta()
    exe = os.path.join(OUT, 'suite')
    cc([os.path.join(HERE, 'suite.c'), *(os.path.join(PLUGIN, s) for s in m['sources'])], exe, SAN,
       [f'-DENGINE={m["symbol"]}'])
    args = [exe, os.path.join(OUT, 'wav')] + ([only] if only else [])
    r = subprocess.run(args)
    return r.returncode


def bench():
    """ns per voice-block for this engine and for shipped kennel synths (same host, same harness)."""
    m = meta()
    rows = [('hollow_saint', [os.path.join(PLUGIN, s) for s in m['sources']], m['symbol'])]
    for name in ('eva', 'mbira', 'oxbow', 'supersaw', 'taffy', 'wavetable'):
        d = os.path.join(SDK, 'plugins', name)
        if not os.path.isfile(os.path.join(d, 'plugin.json')):
            continue
        pm = json.load(open(os.path.join(d, 'plugin.json')))
        rows.append((name, [os.path.join(d, s) for s in pm['sources']], pm['symbol']))
    res = []
    for name, srcs, sym in rows:
        exe = os.path.join(OUT, 'bench_' + name)
        try:
            cc([os.path.join(HERE, 'bench.c'), *srcs], exe, ['-O2'], [f'-DENGINE={sym}', '-I', os.path.dirname(srcs[0])])
        except subprocess.CalledProcessError:
            print(f'{name}: does not build on the host harness, skipped'); continue
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            print(f'{name}: bench failed: {r.stderr.strip()[:200]}'); continue
        res.append((name, float(r.stdout.split()[0])))
    base = dict(res).get('eva')
    for name, ns in res:
        rel = f'  {ns / base:5.2f} x eva' if base else ''
        print(f'{name:14s} {ns:8.0f} ns per voice-block{rel}')
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('what', choices=['phrase', 'suite', 'bench'])
    ap.add_argument('--knobs'); ap.add_argument('--only')
    a = ap.parse_args()
    k = [int(x) for x in a.knobs.split(',')] if a.knobs else None
    sys.exit({'phrase': lambda: phrase(k), 'suite': lambda: suite(a.only), 'bench': bench}[a.what]())
