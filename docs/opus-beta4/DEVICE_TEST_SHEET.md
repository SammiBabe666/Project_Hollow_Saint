# Device test sheet (fill in on the OP-1)

For the first run on 4.0 (Beta 4), or on beta 3.2 to get today's baseline. Fill in the blanks and send the sheet back.
Then each `cpu.txt` gets its measured figure, and each manifest gets a `checked device` line, but only for the plugins
that passed here. The full procedure behind each column is in [BUILD_TEST_CHECKLIST.md](BUILD_TEST_CHECKLIST.md),
section C.

```
Opus version (site / mods list): ________   Core: ____   Date: __________   Firmware: #246
```

Before you start: developer mode on (hold synth + drum + tape + mixer, T1). Then disk mode (shift + COM, T3), copy the
`.opmod` files into **mods**, eject, restart. If the OP-1 ever won't start, hold **Stop** while switching it on to
start with no plugins.

## 1. Each plugin alone

For engines: six-note chords at the heaviest settings for about a minute, then the mods list (T2), orange push on its
row, and read **audio: worst N**. For effects: the effect on the synth with chords going through it at its heaviest
settings, then the same reading.

| Plugin | Row says "loaded"? | Sound OK? (no clicks, no silence) | Screen OK? | Controls OK? | audio: worst N | Heaviest setting played |
|---|---|---|---|---|---|---|
| hollow saint (engine) | | | | | | CATHEDRAL __ %, relic on, wheel 1/16 |
| bloodmoon (engine) | | | | | | |
| erdtree (engine) | | | | | | |
| bronzechoir (engine) | | | | | | |
| dark sister (effect) | | | | | | |
| hkreverb (effect) | | | | | | |
| hkstutter (effect) | | | | | | metronome on: does it lock to the beat? __ |
| rooms (effect) | | | | | | |

Specific checks for the firmware reads:

- Hollow Saint: does the stroke rate (white push, then 1/4) follow the metronome? __ Does it follow tape speed? __ Soft
  vs hard notes: does the level change about as much as on a stock engine, or a lot more? __
- Erdtree: does the brass brighten as a note swells (it follows the envelope)? __
- Stutter: does it lock to the beat at 90 and 140 BPM? __

## 2. Load-together (memory and CPU next to each other)

All four effects installed with one engine. Put dark sister on the synth, rooms on the drum and hkreverb on the master.
hkstutter is installed but not in use (or swapped in for one of the others). The engine plays chords: run it once with
hollow saint, then once with your heaviest other engine.

| Check | Result |
|---|---|
| Every row in the mods list says "loaded" | |
| Each plugin's memory share (mods list, orange push on its row: the details) | sister ___ rooms ___ reverb ___ stutter ___ engine ___ |
| Any "no fast memory" / pool-full line in the boot log, or a plugin refused | |
| Chords for 2 minutes: dropouts or clicks? | |
| Any plugin "switched off: N of M cycles"? Repeat with developer mode **off** | |
| A crash? (the next start says "skipped: crashed"; the crash log is in the crash folder) | |

## 3. Afterwards

Send the sheet back. Then, for each plugin that passed:

- `cpu.txt`: `cycles N` plus a line saying when it was measured and what was played.
- `manifest.txt`: `checked   device YYYY-MM-DD <what was run, up to 60 characters>`.
- A rebuild, so the `.opmod` carries both.

A plugin that failed any check gets neither line. It gets a note of what went wrong instead.
