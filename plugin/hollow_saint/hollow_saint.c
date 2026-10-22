/* Cathedral of the Hollow Saint (plugin id hollow_saint): a hurdy-gurdy for the original OP-1, kennel synth plugin.
 *
 * Milestone 4: melody string + drone + wheel buzz.
 *   blue  (knob 0) DRONE: off (0) .. subtle and dark .. rich and resonant (middle) .. dense, with a sub-octave (high).
 *                  Push for page 2: the drone's tuning, FOLLOW (the lowest held note) or a fixed key C .. B (word 5).
 *   green (knob 1) STRINGS: soft hollow, almost flute-like (low) .. nasal hurdy-gurdy (middle) .. bright and cutting
 *                  (high).
 *   white (knob 2) WHEEL BUZZ: smooth (0) .. rhythmic buzzing on the wheel strokes (middle) .. a continuous, gritty
 *                  rattle with accents (high). Push for page 2: the stroke rate, steady / 1/4 / 1/8 / 1/8T / 1/16 of
 *                  the OP-1's tempo (word 7).
 *   Not yet: orange CATHEDRAL (no effect yet).
 *
 * The string: a bowed string's waveform is close to a sawtooth (Helmholtz motion); its nasal reedy colour comes from a
 * narrow pulse and the instrument body. Per sample: a polyBLEP saw, a polyBLEP pulse (two saws, one offset by the pulse
 * width), a hollow sine pair (fundamental + a little third harmonic) for the soft end, and bow noise that is loud at
 * the moment the string slips under the wheel (the first quarter of each cycle) and quieter while it sticks. Then a
 * one-pole tone low-pass that follows the pitch and a Chamberlin band-pass "nasal formant" around 0.9 .. 2.1 kHz.
 * A slow random pitch drift (a few cents per voice) keeps it organic.
 *
 * The drone (bourdons): two bowed drone strings, root and fifth (+2 / -3 cents: they beat slowly), polyBLEP saws
 * through a resonant two-pole low-pass, plus a sine an octave below the root at high settings. The OP-1 gives an engine
 * only its voices, each multiplied by its own envelope after render, so a drone cannot run on its own: it sounds while
 * a note sounds. Within that, ONE voice owns the drone at a time: the held voice with the lowest pitch (none held: the
 * last owner keeps it through its release). A chord gets one drone, not six, and only that voice pays for it. When
 * ownership moves, the new owner copies the old one's oscillator and filter state (rewound to the start of the block
 * when the old owner has already rendered it) and the two cross-fade over ~40 ms: with a fixed key the hand-over is
 * seamless (same pitch, same phase); in FOLLOW it glides to the new bass note's drone.
 *   FOLLOW: root an octave below the owner's note, fifth above that root (the note itself is the root's octave).
 *   Fixed key: root in octave 2 (C2 .. B2), fifth above; it ignores the played notes, octave and transpose.
 *
 * The wheel buzz (trompette and chien): on the instrument a buzzing bridge (the "dog") on the trompette string lifts and
 * slaps the soundboard once per string cycle, but only while the wheel pushes the string hard enough; players push in
 * rhythm with short wrist strokes (coups de poignet). Here: the trompette, an octave above the drone root (FOLLOW: the
 * owner's own note), runs with the drone owner; once per trompette cycle a slap (its size jittered per cycle: the
 * rattle) excites a bridge resonance (1.7 .. 3.2 kHz) with a little noise inside the slap only, so the buzz stays pitched
 * and harmonic at any setting. Its size follows the wheel: a stroke envelope locked to the OP-1's beat clock (read once
 * per block; when the clock stands still, e.g. tape stopped, a free-running phase at the same tempo takes over) plus a
 * steady part, through a pressure threshold, so each stroke kicks the buzz in. The strokes also swell the string a
 * little, and at high settings each melody-string cycle gets a random roughness (grit).
 *
 * Fixed point: Q15 throughout (32768 = 1.0); signals stay within +-65535 before each multiply by a Q15 gain so every
 * product fits in 31 bits. No division or 64-bit arithmetic per sample (one division per block for the polyBLEP).
 * Every knob-driven gain moves linearly across the block from the previous block's value (no zipper), filter
 * coefficients step once per block from knobs smoothed over ~6 ms.
 * Per-voice state lives in this file's static array (indexed by op1_voice_index), not in the 32-byte voice object;
 * the firmware-owned first word of the voice object is never touched.
 *
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and
 * NOTICE of the kennel SDK). The polyBLEP saw follows the formulation in Hundekraft's EVA (eva.c, same licence).
 */
#include "op1_plugin.h"

#define NV  OP1_VOICES
#define BLK OP1_BLOCK                    /* 128 samples per render call */

static inline int32_t clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int32_t knob(const int16_t *k, int i) { return k[i] < 0 ? 0 : k[i]; }
static inline uint32_t lcg(uint32_t *s) { return *s = *s * 1664525u + 1013904223u; }
static inline int32_t sine(uint32_t ph) { return OP1_SINE_TABLE[ph >> 19]; }   /* 8192-entry table, Q15 */

/* a 5-point table over a knob (0..32767): t[0] at 0 %, t[4] at 100 %; entries within +-65535 */
static inline int32_t tab5(const int32_t *t, int32_t k)
{
    const int i = k >> 13;
    const int32_t f = (k & 8191) << 1;                                    /* Q14 */
    return t[i] + (((t[i + 1] - t[i]) * f) >> 14);
}

/* polyBLEP sawtooth, +-32768: t = the phase as Q15, dt = the phase step as Q15, inv8 = (2^30 / dt) >> 8 */
static inline int32_t blep_saw(uint32_t ph, int32_t dt, int32_t inv8)
{
    const int32_t t = (int32_t)(ph >> 17);
    int32_t s = (int32_t)(ph >> 16) - 32768;
    if (t < dt) {
        const int32_t x = (t * inv8) >> 7;
        s -= x + x - ((x * x) >> 15) - 32768;
    } else if (t > 32767 - dt) {
        const int32_t x = ((t - 32768) * inv8) >> 7;
        s -= ((x * x) >> 15) + x + x + 32768;
    }
    return s;
}

/* one-pole low-pass coefficient (Q15) for a cutoff in Hz (clamped 40 .. 8000): 1 - e^-x, x = 2 pi fc / fs, as a
 * four-term series (within 2 % up to 8 kHz). Per block only. */
static int32_t onepole_coef(int32_t hz)
{
    hz = clampi(hz, 40, 8000);
    const int32_t x = (hz * 4781) >> 10;                                  /* 2 pi / 44100 in Q15 = 4.6687 */
    const int32_t x2 = (x * x) >> 15, x3 = (x2 * x) >> 15, x4 = (x3 * x) >> 15;
    return clampi(x - (x2 >> 1) + ((x3 * 5461) >> 15) - ((x4 * 1365) >> 15), 1, 32767);
}
/* Chamberlin SVF frequency coefficient 2 sin(pi fc / fs), Q15, for fc up to ~2.5 kHz (small-angle: within 1 %) */
static inline int32_t svf_coef(int32_t hz) { return (clampi(hz, 20, 2500) * 4781) >> 10; }

/* ---------------------------------------------------------------- STRINGS: the knob's map (5 points, 0..100 %) */
static const int32_t T_FLUTE[5] = { 27853, 14746, 0, 0, 0 };          /* hollow sine pair */
static const int32_t T_REED[5]  = { 3277, 14746, 19661, 16384, 14746 };  /* pulse */
static const int32_t T_SAW[5]   = { 0, 3277, 9830, 16384, 21299 };       /* saw */
static const int32_t T_PW[5]    = { 16384, 9830, 5243, 3932, 3277 };     /* pulse width, Q15 of a cycle: square .. 10 % */
static const int32_t T_LPK[5]   = { 3, 6, 12, 24, 40 };                  /* tone low-pass, multiples of the pitch */
static const int32_t T_NASAL[5] = { 0, 9830, 21299, 16384, 11469 };      /* formant gain (x2 below: 0 .. 1.3) */
static const int32_t T_FHZ[5]   = { 900, 1100, 1300, 1650, 2100 };       /* formant centre, Hz */
static const int32_t T_NOISE[5] = { 1100, 900, 750, 750, 900 };          /* bow noise */
static const int32_t T_LEVEL[5] = { 19285, 21408, 30027, 24156, 19762 }; /* loudness compensation */

/* ---------------------------------------------------------------- DRONE: the knob's map */
static const int32_t T_DLEVEL[5] = { 0, 9000, 16000, 21000, 24500 };    /* drone level against the string */
static const int32_t T_DHZ[5]    = { 260, 420, 780, 1350, 2200 };       /* low-pass cutoff: dark .. dense */
static const int32_t T_DQ[5]     = { 30000, 24000, 17000, 19000, 22000 };   /* damping: resonant in the middle */
static const int32_t T_DFIFTH[5] = { 16384, 18000, 20300, 22300, 23600 };   /* the fifth against the root */
static const int32_t T_DSUB[5]   = { 0, 0, 0, 6000, 14000 };              /* sub-octave sine */
/* fixed drone keys: phase increments of C2 .. B2 (2^32 = one cycle per sample at 44.1 kHz) */
static const uint32_t KEY_INC[12] = { 6370030u, 6748811u, 7150117u, 7575285u, 8025735u, 8502970u,
                                      9008582u, 9544261u, 10111792u, 10713070u, 11350103u, 12025015u };
#define DRONE_FADE 2341                  /* drone gain step per block: 0 .. 1 in 14 blocks (~40 ms) */
#define ROOT_DET   38                    /* +2 cents, Q15 fraction of the increment */
#define FIFTH_DET  (-57)                 /* -3 cents */
/* page-2 word 5 (blue pushed): 13 zones, 0 = FOLLOW, 1 .. 12 = C .. B */
static inline int drone_key(const int16_t *k) { return (int)((knob(k, 5) * 13) >> 15); }

/* ---------------------------------------------------------------- WHEEL BUZZ: the knob's map */
static const int32_t T_BLEVEL[5]  = { 0, 18000, 27000, 31000, 32767 };  /* buzz level against the string */
static const int32_t T_BBASE[5]   = { 0, 0, 4915, 14746, 29491 };        /* steady wheel pressure: 0, 0, .15, .45, .9 */
static const int32_t T_BSTROKE[5] = { 0, 16384, 27853, 24576, 16384 };   /* stroke depth: .5, .85, .75, .5 */
static const int32_t T_BJIT[5]    = { 3277, 6554, 11469, 16384, 22938 }; /* per-cycle slap jitter: the rattle */
static const int32_t T_BNOISE[5]  = { 2000, 4000, 7000, 10000, 12500 };  /* noise inside the slap */
static const int32_t T_BHZ[5]     = { 1700, 2000, 2400, 2800, 3200 };    /* bridge resonance */
static const int32_t T_BQ[5]      = { 8192, 6554, 5461, 4681, 4096 };    /* its damping: Q 4 .. 8 */
static const int32_t T_BTONE[5]   = { 0, 5000, 8000, 9500, 10500 };      /* the trompette string's own tone */
static const int32_t T_GRIT[5]    = { 0, 0, 0, 4500, 9000 };          /* melody per-cycle roughness */
static const int32_t T_SWELL[5]   = { 0, 2500, 4500, 4000, 3000 };       /* the strokes swelling the string */
#define BUZZ_THRESH 9830                 /* wheel pressure (Q15) where the dog starts to lift: 0.3 */
/* page-2 word 7 (white pushed): 5 zones, steady / 1/4 / 1/8 / 1/8T / 1/16; strokes per beat */
static const uint8_t STROKES[5] = { 0, 1, 2, 3, 4 };
static inline int stroke_zone(const int16_t *k) { return (int)((knob(k, 7) * 5) >> 15); }

/* stroke envelope from the stroke phase (Q32): a 7 % rise, then a squared fall over the rest */
static inline int32_t stroke_env(uint32_t sp)
{
    const int32_t p = (int32_t)(sp >> 16);                                 /* 0 .. 65535 */
    if (p < 4588) return clampi((p * 7314) >> 10, 0, 32767);
    const int32_t r = ((65536 - p) * 17617) >> 15;                         /* 0 .. 32767 */
    return (r * r) >> 15;
}

/* shared wheel: the stroke phase when the beat clock is not moving (advanced once per block) */
static uint32_t wheel_free;
static uint8_t wheel_new_block;

/* ramped gains: each moves linearly across the block from the previous block's value */
enum { G_FLUTE, G_REED, G_SAW, G_NOISE, G_NASAL, G_OUT, G_DRONE, G_BUZZ, NG };

typedef struct {
    uint32_t ph;                 /* melody string phase (2^32 = one cycle) */
    uint32_t rng;                /* per-voice noise */
    int32_t drift, drift_t;      /* pitch drift and its target, Q15 fraction of the increment (+-~4 cents) */
    int32_t tone;                /* tone low-pass state */
    int32_t flp, fbp;            /* nasal formant SVF states */
    int32_t dc8;                 /* DC blocker (one-pole ~14 Hz), Q8 */
    int32_t ks[4];               /* knobs, smoothed per block */
    int32_t g[NG];               /* gains reached at the end of the previous block */
    /* drone (only the owner, and a voice fading its drone out, run it) */
    uint32_t dph[3];             /* root, fifth, sub-octave phases */
    uint32_t dinc[3];            /* their increments in the last block rendered (to rewind a copied state) */
    int32_t dlp, dbp;            /* drone low-pass (SVF) */
    int32_t dgain;               /* ownership fade, Q15 */
    /* trompette and dog (with the drone owner) */
    uint32_t tph, tinc;          /* trompette phase and its last increment */
    int32_t slap, rattle;        /* this cycle's slap and its noise envelope */
    int32_t rlp, rbp;            /* bridge resonance (SVF) */
    int32_t tlp;                 /* trompette tone low-pass */
    int32_t amp;                 /* smoothed dog amplitude (Q15) */
    int32_t mjit;                /* melody grit: this cycle's gain */
    uint8_t held, fresh;
} voice_t;

static voice_t V[NV];
static int32_t buf[BLK];         /* the voice being built (render is never re-entered: one audio interrupt) */

/* shared: block counting (voices render in ascending order, so an index not above the last one starts a new block),
 * each voice's last pitch and the drone owner */
static uint32_t blk;
static int last_vi = NV;
static uint32_t v_blk[NV], v_inc[NV];
static int owner = -1;

static void take_drone(int to)
{
    const int from = owner;
    owner = to;
    if (from < 0 || from == to) return;
    voice_t *a = &V[from], *b = &V[to];
    const int ahead = v_blk[from] == blk;                  /* the old owner has rendered this block already */
    for (int j = 0; j < 3; j++) b->dph[j] = a->dph[j] - (ahead ? a->dinc[j] * BLK : 0u);
    b->dlp = a->dlp; b->dbp = a->dbp;
    b->tph = a->tph - (ahead ? a->tinc * BLK : 0u);
    b->slap = a->slap; b->rattle = a->rattle; b->rlp = a->rlp; b->rbp = a->rbp; b->tlp = a->tlp; b->amp = a->amp;
}

static void hs_note_on(void *self, const int16_t *k)
{
    const int vi = op1_voice_index(self);
    if (vi < 0 || vi >= NV) return;
    voice_t *v = &V[vi];
    /* OP1_PITCH_INC is not this voice's pitch yet (the firmware writes it before render): nothing here reads it */
    v->rng = 0x9E3779B9u ^ ((uint32_t)vi * 0x85EBCA6Bu) ^ (v->rng << 7);   /* varies from note to note */
    v->ph = lcg(&v->rng) & 0x3FFFFFFFu;     /* random start in the first quarter: no two voices line up */
    v->drift = v->drift_t = ((int32_t)(lcg(&v->rng) >> 24) - 128) >> 1;   /* +-64 = +-3.4 cents from the start */
    v->tone = v->flp = v->fbp = 0;
    v->dc8 = 0;
    for (int i = 0; i < 4; i++) v->ks[i] = knob(k, i);
    v_inc[vi] = 0;                          /* its pitch is unknown until it renders */
    if (owner < 0) {                        /* the first note ever: a fresh drone */
        v->dph[0] = 0; v->dph[1] = 0x55555555u; v->dph[2] = 0;
        v->dlp = v->dbp = 0;
        v->tph = 0; v->slap = v->rattle = v->rlp = v->rbp = v->tlp = v->amp = 0;
    }
    v->mjit = 32767;
    /* the owner keeps its drone state when it is re-triggered (a stolen or repeated note): no restart */
    v->held = 1;
    v->fresh = 1;
}

static void hs_note_off(void *self)
{
    const int vi = op1_voice_index(self);
    if (vi < 0 || vi >= NV) return;
    V[vi].held = 0;
}

static void hs_render(void *self, int16_t *out, const int16_t *k)
{
    const int vi = op1_voice_index(self);
    if (vi < 0 || vi >= NV) { for (int n = 0; n < BLK; n++) out[n] = 0; return; }
    voice_t *v = &V[vi];
    const int32_t *inc = OP1_PITCH_INC;

    /* shared, once per block: the block count; then this voice's pitch and the drone's owner */
    if (vi <= last_vi) { blk++; wheel_new_block = 1; }
    last_vi = vi;
    v_inc[vi] = (uint32_t)inc[0];
    {
        int best = -1;
        uint32_t lo = 0xFFFFFFFFu;
        for (int i = 0; i < NV; i++)
            if (V[i].held && v_inc[i] && v_inc[i] < lo) { lo = v_inc[i]; best = i; }
        if (best < 0 && owner < 0) best = vi;
        if (best == vi && owner != vi) take_drone(vi);     /* the new owner takes over when it renders */
    }

    /* knobs: ~6 ms smoothing (two blocks); a fresh note starts at the current position */
    for (int i = 0; i < 4; i++) v->ks[i] += (knob(k, i) - v->ks[i]) >> 1;
    const int32_t d = v->ks[0], s = v->ks[1], bz = v->ks[2];

    /* the wheel: stroke phase at the block start and its step per sample, from the beat clock (once per block) */
    uint32_t sp = 0, sinc = 0;
    const int zone = stroke_zone(k);
    if (STROKES[zone]) {
        op1_beat_t bc;
        op1_beat_clock(&bc);
        const int32_t len_s = bc.len > 0 ? clampi(bc.len >> 14, 4410, 176400) : 22050;  /* samples per beat */
        sinc = (0xFFFFFFFFu / (uint32_t)len_s) * STROKES[zone];
        if (bc.len > 0 && bc.step > 0) {
            const int32_t next_s = clampi(bc.next >> 14, 0, len_s);
            const uint32_t bp15 = 32768u - (((uint32_t)next_s << 15) / (uint32_t)len_s);   /* beat phase */
            sp = (bp15 * STROKES[zone]) << 17;
            if (bc.step != 0x100000) sinc = (sinc >> 10) * (uint32_t)(clampi(bc.step, 0, 0x400000) >> 10);
            wheel_free = sp;                                               /* the free phase continues from here */
        } else {
            if (wheel_new_block) wheel_free += sinc * BLK;
            sp = wheel_free;
        }
    }
    wheel_new_block = 0;

    /* pitch: drift wanders to a new random target every ~0.15 s on average (~0.2 s glide) */
    if ((lcg(&v->rng) >> 24) < 5) v->drift_t = ((int32_t)(lcg(&v->rng) >> 24) - 128) * 3 >> 2;   /* +-96 */
    v->drift += (v->drift_t - v->drift) >> 6;
    const int32_t drift = v->drift;
    /* the drift as a constant phase offset per sample for this block (the pitch's own per-sample changes, LFO or
     * bend, still come through inc[n]; the drift is a few cents of the block's first increment) */
    const uint32_t doff = (uint32_t)((int32_t)((uint32_t)inc[0] >> 15) * drift);
    int32_t dt = (int32_t)((uint32_t)inc[0] >> 17);                       /* phase step, Q15 */
    dt += (dt * drift) >> 15;
    dt = clampi(dt, 1, 16384);
    const int32_t inv8 = (int32_t)((1u << 30) / (uint32_t)dt) >> 8;

    /* bow pressure from velocity: brighter and noisier when played harder */
    const int32_t vel = op1_voice_vel_gain(self);                          /* Q15 */
    const int32_t bow = 18000 + ((vel * 14767) >> 15);                     /* 0.55 .. 1.0 */

    /* block targets */
    int32_t t[NG];
    /* the string's loudness compensation is folded into its source gains (the filters are linear), so it does not
     * move the drone */
    const int32_t lvl = tab5(T_LEVEL, s);
    t[G_FLUTE] = (tab5(T_FLUTE, s) * lvl) >> 15;
    t[G_REED]  = (tab5(T_REED, s) * lvl) >> 15;
    t[G_SAW]   = (tab5(T_SAW, s) * lvl) >> 15;
    t[G_NOISE] = (((tab5(T_NOISE, s) * bow) >> 15) * lvl) >> 15;
    t[G_NASAL] = tab5(T_NASAL, s);
#ifdef HS_TEST_SOLO_DRONE                    /* host tests only: the string muted, to measure the drone alone */
    t[G_FLUTE] = t[G_REED] = t[G_SAW] = t[G_NOISE] = t[G_NASAL] = 0;
#endif
    t[G_OUT]   = (9000 * (12000 + ((vel * 20767) >> 15))) >> 15;         /* level x velocity */
    /* drone: ownership fade, then its level */
    v->dgain = clampi(v->dgain + (vi == owner ? DRONE_FADE : -DRONE_FADE), 0, 32767);
    t[G_DRONE] = (tab5(T_DLEVEL, d) * v->dgain) >> 15;
    t[G_BUZZ]  = bz < 1000 ? 0 : (tab5(T_BLEVEL, bz) * v->dgain) >> 15;   /* the bottom 3 %: off */
#ifdef HS_TEST_SOLO_DRONE
    t[G_DRONE] = 0;                              /* (solo builds measure one part at a time: the buzz here) */
#endif
    const uint32_t pw = (uint32_t)tab5(T_PW, s) << 17;
    const int32_t f0x = (dt * 1378) >> 10;                                 /* the pitch in Hz (dt * 44100 / 32768) */
    const int32_t ta = onepole_coef((((tab5(T_LPK, s) * f0x) >> 0) * bow) >> 15);
    const int32_t ff = svf_coef(tab5(T_FHZ, s));
    const int32_t fq = 10923;                                              /* formant damping: Q = 3 */

    if (v->fresh) { for (int i = 0; i < NG; i++) v->g[i] = t[i]; v->fresh = 0; }
    int32_t g0[NG], dg[NG];
    for (int i = 0; i < NG; i++) { g0[i] = v->g[i]; dg[i] = t[i] - v->g[i]; v->g[i] = t[i]; }
    /* the sine pair only while it is heard (STRINGS below ~55 %, or fading out) */
    const int flute = g0[G_FLUTE] | t[G_FLUTE];
    const int32_t grit = tab5(T_GRIT, bz);
    int32_t mjit = v->mjit;
    const int rough = grit || mjit != 32767;                               /* grit on, or its last cycle to finish */

    uint32_t ph = v->ph, rng = v->rng;
    int32_t tone = v->tone, flp = v->flp, fbp = v->fbp;
    int32_t *o = buf;
    const int32_t *ip = inc;                                               /* inc itself stays at sample 0 */
    /* 8 sub-blocks of 16 samples; the gains step from the old block value to the new one (the first sub-block at
     * 1/8 of the way), as EVA does: smooth enough with knobs smoothed per block, and far cheaper than per sample */
    for (int sb = 1; sb <= 8; sb++) {
#define GAIN(i) (g0[i] + ((dg[i] * sb) >> 3))
        const int32_t gf = GAIN(G_FLUTE), gr = GAIN(G_REED), gs = GAIN(G_SAW), gn = GAIN(G_NOISE);
        const int32_t gb = GAIN(G_NASAL);
#undef GAIN
        /* saw * gs + pulse * gr with pulse = (saw - saw2) / 2: two products instead of three */
        const int32_t ga = gs + (gr >> 1), gp = gr >> 1;
        const int32_t gbq = (fq * gb) >> 14;                               /* formant: unity at the centre, x2 */
        for (int n = 0; n < 16; n++, ip++) {
            const uint32_t pold = ph;
            ph += (uint32_t)*ip + doff;
            if (rough && ph < pold) mjit = 32767 - (int32_t)(((lcg(&rng) >> 17) * (uint32_t)grit) >> 15);   /* new cycle */
            const int32_t saw = blep_saw(ph, dt, inv8);
            const int32_t saw2 = blep_saw(ph - pw, dt, inv8);              /* the pulse's second edge */
            int32_t nz = (int32_t)(lcg(&rng) >> 17) - 16384;               /* +-16384 */
            if (ph >= 0x40000000u) nz >>= 2;                               /* the slip: noisy; the stick: quiet */
            int32_t raw = ((saw * ga) >> 15) - ((saw2 * gp) >> 15) + ((nz * gn) >> 15);
            if (flute) {
                /* negative: a rising saw's fundamental is -sin, so the sine pair adds to the saw and pulse instead
                 * of cancelling them where they cross-fade */
                const int32_t fl = -sine(ph) - ((sine(ph * 3u) * 6554) >> 15);   /* +-39322 */
                raw += (fl * gf) >> 15;
            }
            if (rough) raw = (raw * mjit) >> 15;                           /* grit */
            tone += (((raw - tone) >> 1) * ta) >> 14;
            /* nasal formant: Chamberlin SVF on half the raw signal, band output normalised to unity gain at the
             * centre (x fq), then x2 (>> 14) for the halved input */
            flp += (ff * fbp) >> 15;
            const int32_t hp = (raw >> 1) - flp - ((fq * fbp) >> 15);
            fbp += (ff * hp) >> 15;
            *o++ = tone + ((fbp * gbq) >> 15);
        }
    }
    v->ph = ph; v->rng = rng;
    v->tone = tone; v->flp = flp; v->fbp = fbp; v->mjit = mjit;

    /* ---- drone: the owner, and a former owner while it fades out */
    uint32_t ri[BLK];                                                      /* drone root increment per sample */
    const int run_drone = (g0[G_DRONE] | t[G_DRONE]) != 0, run_buzz = (g0[G_BUZZ] | t[G_BUZZ]) != 0;
    if (run_drone || run_buzz) {
        const int key = drone_key(k);
        if (key) for (int n = 0; n < BLK; n++) ri[n] = KEY_INC[key - 1];
        else for (int n = 0; n < BLK; n++) ri[n] = (uint32_t)inc[n] >> 1;
    }
    if (run_drone) {
        const uint32_t r0 = ri[0], f0 = r0 + (r0 >> 1), s0 = r0 >> 1;
        const uint32_t roff = (uint32_t)((int32_t)(r0 >> 15) * ROOT_DET), foff = (uint32_t)((int32_t)(f0 >> 15) * FIFTH_DET);
        const int32_t rdt = clampi((int32_t)((r0 + roff) >> 17), 1, 16384), fdt = clampi((int32_t)((f0 + foff) >> 17), 1, 16384);
        const int32_t rinv = (int32_t)((1u << 30) / (uint32_t)rdt) >> 8, finv = (int32_t)((1u << 30) / (uint32_t)fdt) >> 8;
        const int32_t fifth = tab5(T_DFIFTH, d), sub = tab5(T_DSUB, d);
        const int32_t fd = svf_coef(tab5(T_DHZ, d)), qd = tab5(T_DQ, d);
        uint32_t p0 = v->dph[0], p1 = v->dph[1], p2 = v->dph[2];
        int32_t dlp = v->dlp, dbp = v->dbp;
        int32_t *b = buf;
        for (int sb = 1; sb <= 8; sb++) {
            const int32_t gd = g0[G_DRONE] + ((dg[G_DRONE] * sb) >> 3);
            for (int n = 0; n < 16; n++, b++) {
                const uint32_t r = ri[(sb - 1) * 16 + n];
                p0 += r + roff;
                p1 += r + (r >> 1) + foff;
                p2 += r >> 1;
                const int32_t x = (blep_saw(p0, rdt, rinv) >> 1) + ((blep_saw(p1, fdt, finv) * fifth) >> 16);
                /* resonant two-pole low-pass: Chamberlin SVF on half the signal, low-pass output x2 */
                dlp += (fd * dbp) >> 15;
                const int32_t hp = (x >> 1) - dlp - ((qd * dbp) >> 15);
                dbp += (fd * hp) >> 15;
                const int32_t y = dlp * 2 + ((sine(p2) * sub) >> 15);
                *b += (clampi(y, -65535, 65535) * gd) >> 15;
            }
        }
        v->dph[0] = p0; v->dph[1] = p1; v->dph[2] = p2;
        v->dinc[0] = r0 + roff; v->dinc[1] = f0 + foff; v->dinc[2] = s0;
        v->dlp = dlp; v->dbp = dbp;
    } else {
        /* not running: its phases did not move this block, so a copy from it needs no rewind */
        v->dinc[0] = v->dinc[1] = v->dinc[2] = 0;
    }

    /* ---- wheel buzz: the trompette and its dog (the owner, and a former owner while it fades out) */
    int32_t senv[8];                                                       /* stroke envelope per sub-block */
    for (int sb = 0; sb < 8; sb++) senv[sb] = sinc ? stroke_env(sp + sinc * (uint32_t)(sb * 16 + 8)) : 32767;
    if (run_buzz) {
        const int32_t base = tab5(T_BBASE, bz), depth = tab5(T_BSTROKE, bz), jit = tab5(T_BJIT, bz);
        const int32_t bnz = tab5(T_BNOISE, bz), btone = tab5(T_BTONE, bz);
        const int32_t fr = svf_coef(tab5(T_BHZ, bz)), rq = tab5(T_BQ, bz);
        const int32_t tlc = 7900;                                          /* trompette tone low-pass, ~1.9 kHz */
        /* an octave above the root, folded up to at least C3: a trompette never sits lower, and below that the
         * slaps stop fusing into a buzz and come apart as ticks */
        int tsh = 1;
        while (tsh < 6 && (ri[0] << tsh) < 12740060u) tsh++;
        uint32_t tph = v->tph, rng = v->rng;
        int32_t slap = v->slap, rattle = v->rattle, rlp = v->rlp, rbp = v->rbp, tlp = v->tlp, amp = v->amp;
        int32_t *b = buf;
        for (int sb = 0; sb < 8; sb++) {
            /* wheel pressure through the dog's threshold: 0 below it, full 0.4 above it */
            const int32_t drive = base + ((depth * senv[sb]) >> 15);
            amp += (clampi(((drive - BUZZ_THRESH) * 5) >> 1, 0, 32767) - amp) >> 1;
            const int32_t gz = g0[G_BUZZ] + ((dg[G_BUZZ] * (sb + 1)) >> 3);
            const int32_t tg = (btone * (19661 + ((amp * 13107) >> 15))) >> 15;   /* string tone 0.6 .. 1 with pressure */
            for (int n = 0; n < 16; n++, b++) {
                const uint32_t told = tph;
                tph += ri[sb * 16 + n] << tsh;
                if (tph < told) {                                          /* a new cycle: the dog slaps */
                    const int32_t u = (int32_t)(lcg(&rng) >> 17);
                    slap = (amp * (32767 - ((u * jit) >> 15))) >> 15;
                    rattle = (slap * bnz) >> 15;
                }
                const int32_t nz = (int32_t)(lcg(&rng) >> 17) - 16384;
                const int32_t exc = slap + ((nz * rattle) >> 14);
                slap -= slap >> 3;                                         /* ~0.2 ms */
                rattle -= rattle >> 5;                                     /* ~0.7 ms */
                /* bridge resonance: SVF band-pass on a quarter of the excitation, normalised (x rq) and x4 */
                rlp += (fr * rbp) >> 15;
                const int32_t hp = (exc >> 2) - rlp - ((rq * rbp) >> 15);
                rbp += (fr * hp) >> 15;
                const int32_t ring = (rbp * rq) >> 13;
                /* the trompette's own tone: a plain saw, darkened */
                tlp += ((((int32_t)(tph >> 16) - 32768) - tlp) * tlc) >> 15;
                const int32_t y = ring + (exc >> 1) + ((tlp * tg) >> 15);
                *b += (clampi(y, -65535, 65535) * gz) >> 15;
            }
        }
        v->tph = tph; v->tinc = ri[BLK - 1] << tsh; v->rng = rng;
        v->slap = slap; v->rattle = rattle; v->rlp = rlp; v->rbp = rbp; v->tlp = tlp; v->amp = amp;
    } else {
        v->tinc = 0;
    }

    /* ---- output: DC blocker, level x velocity, the strokes swelling the string */
    const int32_t swell = tab5(T_SWELL, bz);
    int32_t dc8 = v->dc8;
    for (int sb = 1; sb <= 8; sb++) {
        int32_t go = g0[G_OUT] + ((dg[G_OUT] * sb) >> 3);
        go += (go * ((swell * senv[sb - 1]) >> 15)) >> 15;
        for (int n = 0; n < 16; n++) {
            const int i = (sb - 1) * 16 + n;
            int32_t y = buf[i];
            dc8 += (y * 256 - dc8) >> 9;
            y -= dc8 >> 8;
            out[i] = op1_sat16((clampi(y, -65535, 65535) * go) >> 15);
        }
    }
    v->dc8 = dc8;
    v_blk[vi] = blk;
}

const op1_engine_t ENGINE_hollow_saint = { hs_note_on, hs_render, hs_note_off,
                                           { OP1_PRESS_PAGE, OP1_PRESS_NONE, OP1_PRESS_PAGE, OP1_PRESS_NONE } };
