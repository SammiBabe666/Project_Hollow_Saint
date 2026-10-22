/* Cathedral of the Hollow Saint (plugin id hollow_saint): a hurdy-gurdy for the original OP-1, kennel synth plugin.
 *
 * Milestone 2: the melody string (chanterelle) only.
 *   green (knob 1) STRINGS: soft hollow, almost flute-like (low) .. nasal hurdy-gurdy (middle) .. bright and cutting
 *                  (high). Not yet: blue DRONE, white WHEEL BUZZ, orange CATHEDRAL (they have no effect yet).
 *
 * The string: a bowed string's waveform is close to a sawtooth (Helmholtz motion); its nasal reedy colour comes from a
 * narrow pulse and the instrument body. Per sample: a polyBLEP saw, a polyBLEP pulse (two saws, one offset by the pulse
 * width), a hollow sine pair (fundamental + a little third harmonic) for the soft end, and bow noise that is loud at
 * the moment the string slips under the wheel (the first quarter of each cycle) and quieter while it sticks. Then a
 * one-pole tone low-pass that follows the pitch and a Chamberlin band-pass "nasal formant" around 0.9 .. 2.1 kHz.
 * A slow random pitch drift (a few cents per voice) keeps it organic.
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
static const int32_t T_FLUTE[5] = { 27853, 14746, 3277, 0, 0 };          /* hollow sine pair */
static const int32_t T_REED[5]  = { 3277, 14746, 19661, 16384, 14746 };  /* pulse */
static const int32_t T_SAW[5]   = { 0, 3277, 9830, 16384, 21299 };       /* saw */
static const int32_t T_PW[5]    = { 16384, 9830, 5243, 3932, 3277 };     /* pulse width, Q15 of a cycle: square .. 10 % */
static const int32_t T_LPK[5]   = { 3, 6, 12, 24, 40 };                  /* tone low-pass, multiples of the pitch */
static const int32_t T_NASAL[5] = { 0, 9830, 21299, 16384, 11469 };      /* formant gain (x2 below: 0 .. 1.3) */
static const int32_t T_FHZ[5]   = { 900, 1100, 1300, 1650, 2100 };       /* formant centre, Hz */
static const int32_t T_NOISE[5] = { 1100, 900, 750, 750, 900 };          /* bow noise */
static const int32_t T_LEVEL[5] = { 19285, 21408, 26288, 24156, 19762 }; /* loudness compensation */

/* ramped gains: each moves linearly across the block from the previous block's value */
enum { G_FLUTE, G_REED, G_SAW, G_NOISE, G_NASAL, G_OUT, NG };

typedef struct {
    uint32_t ph;                 /* melody string phase (2^32 = one cycle) */
    uint32_t rng;                /* per-voice noise */
    int32_t drift, drift_t;      /* pitch drift and its target, Q15 fraction of the increment (+-~4 cents) */
    int32_t tone;                /* tone low-pass state */
    int32_t flp, fbp;            /* nasal formant SVF states */
    int32_t dc8;                 /* DC blocker (one-pole ~14 Hz), Q8 */
    int32_t ks[4];               /* knobs, smoothed per block */
    int32_t g[NG];               /* gains reached at the end of the previous block */
    uint8_t held, fresh;
} voice_t;

static voice_t V[NV];

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

    /* knobs: ~6 ms smoothing (two blocks); a fresh note starts at the current position */
    for (int i = 0; i < 4; i++) v->ks[i] += (knob(k, i) - v->ks[i]) >> 1;
    const int32_t s = v->ks[1];

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
    t[G_FLUTE] = tab5(T_FLUTE, s);
    t[G_REED]  = tab5(T_REED, s);
    t[G_SAW]   = tab5(T_SAW, s);
    t[G_NOISE] = (tab5(T_NOISE, s) * bow) >> 15;
    t[G_NASAL] = tab5(T_NASAL, s);
    t[G_OUT]   = (((tab5(T_LEVEL, s) * 9000) >> 15) * (12000 + ((vel * 20767) >> 15))) >> 15;   /* level x velocity */
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

    uint32_t ph = v->ph, rng = v->rng;
    int32_t tone = v->tone, flp = v->flp, fbp = v->fbp, dc8 = v->dc8;
    int16_t *o = out;
    /* 8 sub-blocks of 16 samples; the gains step from the old block value to the new one (the first sub-block at
     * 1/8 of the way), as EVA does: smooth enough with knobs smoothed per block, and far cheaper than per sample */
    for (int sb = 1; sb <= 8; sb++) {
#define GAIN(i) (g0[i] + ((dg[i] * sb) >> 3))
        const int32_t gf = GAIN(G_FLUTE), gr = GAIN(G_REED), gs = GAIN(G_SAW), gn = GAIN(G_NOISE);
        const int32_t gb = GAIN(G_NASAL), go = GAIN(G_OUT);
#undef GAIN
        /* saw * gs + pulse * gr with pulse = (saw - saw2) / 2: two products instead of three */
        const int32_t ga = gs + (gr >> 1), gp = gr >> 1;
        const int32_t gbq = (fq * gb) >> 14;                               /* formant: unity at the centre, x2 */
        for (int n = 0; n < 16; n++, inc++) {
            ph += (uint32_t)*inc + doff;
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
            tone += (((raw - tone) >> 1) * ta) >> 14;
            /* nasal formant: Chamberlin SVF on half the raw signal, band output normalised to unity gain at the
             * centre (x fq), then x2 (>> 14) for the halved input */
            flp += (ff * fbp) >> 15;
            const int32_t hp = (raw >> 1) - flp - ((fq * fbp) >> 15);
            fbp += (ff * hp) >> 15;
            int32_t y = tone + ((fbp * gbq) >> 15);

            dc8 += (y * 256 - dc8) >> 9;                                   /* DC blocker */
            y -= dc8 >> 8;
            *o++ = op1_sat16((clampi(y, -65535, 65535) * go) >> 15);
        }
    }
    v->ph = ph; v->rng = rng;
    v->tone = tone; v->flp = flp; v->fbp = fbp; v->dc8 = dc8;
}

const op1_engine_t ENGINE_hollow_saint = { hs_note_on, hs_render, hs_note_off,
                                           { OP1_PRESS_NONE, OP1_PRESS_NONE, OP1_PRESS_NONE, OP1_PRESS_NONE } };
