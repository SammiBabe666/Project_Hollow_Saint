# Hollow Saint 0.7.0: hardware test checklist

0.7.0 = the 0.6.1 speed-up + **Lo-Fi Relic** + **Crank & Tremor**. It has **never run on an OP-1**. If you
want to be careful, test 0.6.1 first (`../v0.6.1/`): it is the same engine with no new features, so a
problem that shows up in both points at the install or the platform, not the new code.

Files: `hollow_saint.knl` (40,548 bytes, sha256 in `hollow_saint.knl.sha256`) and `presets/` (rewritten for 0.7.0:
copy them over the old ones). Demos made on a PC: `relic_demo.mp3`, `crank_demo.mp3`, `screen_relic.png`.

## Install

Same as before: Workshop **Your plugins**, add `hollow_saint.knl`, **Update OP-1**, confirm with **T1**. Then
**synth**, **Shift + T1**, **hollow saint**. Presets: **Shift + COM**, **T3**, copy `presets/synth/hollow saint` into
the OP-1's `synth` folder. If it won't boot: power-cycle, then remove the plugin in the Workshop.

## Everything from 0.6.1

- [ ] Loads, the screen draws, all 8 presets load. Blue and red knobs and blue's push behave as before.
- [ ] Resource monitor (clock screen, **T1**): peak load with 6 notes, all knobs at maximum, heavy FX on.

## Lo-Fi Relic (push green)

- [ ] Push green: the green meter turns into a cassette, its reels turn while you play, and grey dust specks appear.
- [ ] The sound turns older: darker, slightly wobbly pitch, a bit of grit and dust. It should be **about as loud**
      as before, not quieter or muffled.
- [ ] While it's on, turn green: the small meter next to the cassette moves (the age). Low = subtle, high = a very
      worn tape. STRINGS doesn't change while the relic is on (that's expected).
- [ ] Push green again: back to normal, and green controls STRINGS again.
- [ ] Hold a chord and switch the relic on and off several times: **no clicks or jumps**, just a quick fade.
- [ ] With CATHEDRAL high: the room should sound like it's hearing the worn tape.
- [ ] Resource monitor with the relic on, 6 notes, everything at maximum (the heaviest case).

## Crank & Tremor (white knob + white push)

Set a tempo and press play (or just play: the wheel free-runs at the tempo).
- [ ] Push white: the rate icon shows. There are now 7 rates: steady, **1 bar**, **1/2** (a slur over the line),
      1/4, 1/8, 1/8T, 1/16.
- [ ] White low (~25%) at 1 bar or 1/2: slow, breathing swells.
- [ ] White middle at 1/4 or 1/8: a steady mechanical tremolo with the buzz.
- [ ] White high at 1/16: aggressive buzzing with a strong pulse.
- [ ] Steady (first rate) or white at 0: no pulsing.
- [ ] The pulse stays in time with the metronome.

## Report back

- Resource monitor peaks (relic off and relic on).
- Anything that clicks, crackles, freezes or looks wrong, with the knob positions, preset and rate.
- How the relic and the tremor feel. Too much or too little at any setting? The depths are one table each and easy
  to retune.
