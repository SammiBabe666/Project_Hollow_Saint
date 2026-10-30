# Hollow Saint 0.6.1: hardware test checklist

0.6.1 is a speed-up only. It sounds exactly like 0.6.0 (the audio is bit-identical in every test), and the
Blackfin simulator shows it using about 38 % fewer cycles. It has still **never run on an OP-1**, so this is the first real test.

Files: `hollow_saint.knl` (33,556 bytes, sha256 in `hollow_saint.knl.sha256`) and `presets/`.

## Install

1. The OP-1 must run Hundekraft **3.0.x**.
2. In the Workshop: **Your plugins**, add `hollow_saint.knl`, connect by USB, **Update OP-1**, send, press **T1** on the
   OP-1 to confirm. It installs on the restart.
3. On the OP-1: **synth**, **Shift + T1**, pick **hollow saint**.
4. Presets: **Shift + COM**, **T3** (disk mode), copy `presets/synth/hollow saint` into the OP-1's `synth` folder, eject.

If the OP-1 won't start after adding the plugin, power-cycle it. It boots once without plugins, and then you can
remove the plugin in the Workshop.

## Checks (tick each, note anything odd)

**Loads and shows**
- [ ] It appears in the engine list as "hollow saint" and selecting it doesn't freeze or reboot.
- [ ] The screen shows the hurdy-gurdy in the arch, and the wheel turns while you play.
- [ ] All 8 presets load and sound different from each other.

**Each knob**
- [ ] Blue (DRONE): off at 0, then a drone under the notes. Push blue: the keyboard icon shows, turning picks FOLLOW
      or a key; push again to return.
- [ ] Green (STRINGS): soft and flutey, to nasal, to bright.
- [ ] White (WHEEL BUZZ): buzzing that pulses with the tempo. Push white: the stroke-rate icon shows; try each rate
      with the metronome/tape running and stopped.
- [ ] Red (CATHEDRAL): from dry to a big dark room, with a faint bell on each note near the top.

**CPU and glitches** (the main point of 0.6.1)
- [ ] Clock screen, **T1** (resource monitor): note the peak load with 6 notes held, all four knobs at maximum.
- [ ] Same, with a heavy FX (e.g. a reverb or delay) on the synth and the master.
- [ ] Fast chords and repeated notes for a minute: no crackles, dropouts or stuck notes.
- [ ] Hold a 6-note chord with a long T2 release and let go: the tail fades cleanly, no clicks.
- [ ] Record to tape while playing: no glitches in the recording.

**Report back**
- The resource monitor numbers (both cases above).
- Anything that clicks, crackles, freezes, or looks wrong on screen, with the preset or knob positions.
