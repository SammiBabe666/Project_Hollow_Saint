Hollow Saint 0.7.7: the stable 0.7 release

A haunted hurdy-gurdy for the **original OP-1**: a synth engine plugin for the [Hundekraft OP-1 Workshop](https://hundekraft.github.io/op1-workshop/index.html) **3.0.x**. Bowed melody strings, a drone that can hold a key like real bourdons, a buzzing bridge that kicks in on wheel strokes locked to the OP-1's tempo, and a cathedral that darkens, rings and tolls a distant bell. Pure synthesis, no samples.

## What's new in 0.7.7

- The most stable build of the 0.7 line, recommended over 0.7.0.
- On a fresh load the white push starts at **steady** (no pulse) instead of 1/8, so the wheel only pulses once you choose a rate. Presets still set their own rate.
- Updated screen artwork.

## The 0.7 features (since 0.6)

- **Lo-Fi Relic (push green).** The sound goes through a worn old tape before it reaches the cathedral: wow and flutter, warm saturation, a lower sample rate and fewer bits, dust, a darker tape head, a little hiss and a sagging transport. While it's on, turning green sets how old the tape is. It stays about as loud as the clean sound, and switching it on or off fades over ~35 ms, so it doesn't click.
- **Crank & Tremor (white).** The wheel strokes now also pulse the level and the string's tone. WHEEL BUZZ low gives slow, breathing swells, the middle gives a steady mechanical tremolo, and high gives an aggressive rattle with a deep pulse.
- **Two new stroke rates:** steady, **1 bar**, **1/2**, 1/4, 1/8, 1/8 triplet, 1/16.
- **Screen:** a worn cassette with turning reels and an age meter appears while the relic is on, with dust in the cathedral. There are icons for all 7 rates.
- **Presets** updated: midnight vigil breathes once a bar, crypt drone and drowned bell swell every half bar, and each preset has its own relic age.
- **Faster (from 0.6.1):** about 38 % fewer processor cycles than 0.6.0, with exactly the same sound. With the relic off and the wheel on steady, 0.7.0 sounds bit-for-bit the same as 0.6.1.

## The encoders

| Knob | Turn | Push |
|---|---|---|
| 🔵 **Blue: DRONE** | off, subtle and dark, rich and resonant, dense with a sub-octave | the drone's key: FOLLOW (your lowest note) or a fixed key C to B |
| 🟢 **Green: STRINGS** | soft hollow flute, nasal hurdy-gurdy, bright and cutting | **Lo-Fi Relic** on/off; while on, green sets the tape's age |
| ⚪ **White: WHEEL BUZZ** | smooth, breathing swells, mechanical tremolo with buzz, aggressive rattle | the stroke rate, synced to the OP-1's tempo |
| 🔴 **Red: CATHEDRAL** | dry and close, hollow and old-world, a dark huge space with a distant bell | none |

## Install

1. Your OP-1 needs the Hundekraft firmware **3.0.x**.
2. In the Workshop, open **Your plugins**, add `hollow_saint.knl`, connect by USB, click **Update OP-1**, then press **T1** on the OP-1 to confirm.
3. On the OP-1, press **synth**, then **Shift + T1**, and pick **hollow saint**.
4. **Presets:** unzip `hollow_saint-presets.zip`. On the OP-1, press **Shift + COM**, then **T3** (disk mode). Copy the `hollow saint` folder from `synth` into the OP-1's `synth` folder, then eject.

If the OP-1 won't start after you add the plugin, power-cycle it. It boots once without plugins, and you can then remove the plugin in the Workshop.

## Files

- `hollow_saint.knl`: the plugin (45,452 bytes)
- `hollow_saint-presets.zip`: the 8 presets (hollow saint, midnight vigil, howling dog, ash procession, bone flute, crypt drone, plague dance, drowned bell)
- `hollow_saint_0.7.0_demo.mp4` (optional): a 4-minute demo of the engine, the 0.7 features and the presets, rendered on a PC from the 0.7.0 code.
- `SHA256SUMS.txt`: checksums

Licence: GPL-3.0-or-later, with the OP-1 firmware permission (see `LICENSE` and `NOTICE`).
