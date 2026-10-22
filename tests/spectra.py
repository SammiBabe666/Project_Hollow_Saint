#!/usr/bin/env python3
"""Spectrogram / average-spectrum views of rendered WAVs, for checking timbre by eye.
    python3 tests/spectra.py out.png a.wav[:label] b.wav[:label] ...   (one column per file: spectrogram + mean spectrum)
"""
import sys, wave, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt

def load(p):
    with wave.open(p) as w: return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(float) / 32768

out, items = sys.argv[1], sys.argv[2:]
fig, ax = plt.subplots(2, len(items), figsize=(4.2 * len(items), 6), squeeze=False)
for i, it in enumerate(items):
    path, _, label = it.partition(':')
    x = load(path)
    a = ax[0][i]; a.specgram(x, NFFT=2048, Fs=44100, noverlap=1536, cmap='magma', vmin=-110); a.set_ylim(0, 12000)
    a.set_title(label or path.split('/')[-1], fontsize=9)
    seg = x[int(0.6 * 44100):int(2.6 * 44100)] if len(x) > 3 * 44100 else x
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) + 1e-9
    f = np.fft.rfftfreq(len(seg), 1 / 44100)
    b = ax[1][i]; b.semilogx(f, 20 * np.log10(sp / sp.max())); b.set_xlim(40, 16000); b.set_ylim(-90, 2); b.grid(alpha=.3)
fig.tight_layout(); fig.savefig(out, dpi=80)
print(out)
