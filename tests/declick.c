/* Click hunt for the voice modes and the encoders: the firmware stand-in of tests/suite.c, with the OP-1's three voice
 * modes and encoder-like knob movement, and a sample-accurate click detector.
 *
 *   - poly:    a free voice, else the oldest is stolen (its envelope restarts from where it was)
 *   - unison:  every note plays on all 6 voices, detuned; a new note while others sound re-triggers the sounding voices
 *              (note_on on a voice that is still audible: the case a steal is too)
 *   - mono:    one voice; a new note re-triggers it (legato: the envelope keeps going)
 *   - encoders: the OS hands the engine a new knob value only when the encoder moves, in steps of a few hundred to a
 *              few thousand (fast turns accelerate), at its display rate (every ~6 blocks here), not a smooth ramp
 *
 * Clicks: d2 = the second difference of the bus (a crude high-pass). A sample is a click when |d2| is more than
 * 3x the largest |d2| in the 2048 samples around it (32 samples either side left out) and above a floor: a periodic
 * waveform edge recurs and never stands out, a discontinuity does. Each click is counted once (512 samples apart).
 *   usage: declick [scenario] [-w outdir]   (prints one line per scenario: clicks, worst stand-out ratio, where)
 *   env: DECLICK_KNOBS=a,b,c,d (starting knobs; a soft sound such as 0,0,0,12000 makes clicks easiest to see),
 *        DECLICK_FLOOR (the smallest |d2| that counts, default 400), DECLICK_SMOOTH=1 (see turn)
 *   build: cc -O2 -DOP1_HOST -I tests/host_sdk -I tests/host_shim tests/declick.c plugin/hollow_saint/hollow_saint.c -lm
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3] = { 0, 0, 16384 };
int16_t host_knobs[8][8]; int host_slot; const uint8_t *host_sample_buffer[8];
int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
void *host_find_node(void *s, const char *id) { (void)s; (void)id; return 0; }
void host_node_set_transform(void *n, const int32_t *m) { (void)n; (void)m; }
void host_node_set_visible(void *n, int on) { (void)n; (void)on; }
uint8_t *host_node_style_rgba(void *s, const char *id) { static uint8_t c[4]; (void)s; (void)id; return c; }
void host_screen_draw(void *s, int dt) { (void)s; (void)dt; }
void *host_raster(void) { return 0; }
void host_raster_line(void *r, const void *a, const void *b, int32_t w, const uint8_t *c) { (void)r; (void)a; (void)b; (void)w; (void)c; }
void *read_fx_param_safe(int slot, int pos) { (void)slot; (void)pos; return 0; }

extern const op1_engine_t ENGINE;

#define NV OP1_VOICES
#define ENV1 (32767 << 8)
enum { POLY, UNISON, MONO };
typedef struct { int active, gate, note, stage; int32_t env; uint32_t born; double det; } fw_voice;
static fw_voice fv[NV];
static uint32_t now_block;
static int mode;
static int16_t knobs[8];
static int32_t a_inc = ENV1 / 441, d_dec = (8192 << 8) / 4410, sus = 24576 << 8, r_dec = (24576 << 8) / 13230;

static int32_t inc_of(double midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }
static void stale_pitch(int note) { for (int n = 0; n < OP1_BLOCK; n++) host_pitch_inc[n] = inc_of(note > 60 ? 30 : 100); }
static void trigger(int i, int note, int vel, double det)
{
    stale_pitch(note);
    host_voice_vel[i] = (int16_t)(vel << 8);
    fv[i] = (fw_voice){ 1, 1, note, 0, fv[i].active ? fv[i].env : 0, now_block, det };
    ENGINE.note_on(OP1_VOICE(i), knobs);
}
static void note_on(int note, int vel)
{
    if (mode == UNISON) {                                   /* all six, spread over +-12 cents */
        for (int i = 0; i < NV; i++) trigger(i, note, vel, (i - 2.5) * 4.8);
        return;
    }
    if (mode == MONO) { trigger(0, note, vel, 0); return; }
    int best = -1;
    for (int i = 0; i < NV; i++) if (!fv[i].active) { best = i; break; }
    if (best < 0) { best = 0; for (int i = 1; i < NV; i++) if (fv[i].born < fv[best].born) best = i; }
    trigger(best, note, vel, 0);
}
static void note_off(int note)
{
    for (int i = 0; i < NV; i++)
        if (fv[i].active && fv[i].gate && fv[i].note == note) { fv[i].gate = 0; fv[i].stage = 3; ENGINE.note_off(OP1_VOICE(i)); }
}
static void all_off(void) { for (int i = 0; i < NV; i++) if (fv[i].active && fv[i].gate) note_off(fv[i].note); }
static int32_t env_step(fw_voice *v)
{
    switch (v->stage) {
    case 0: v->env += a_inc; if (v->env >= ENV1) { v->env = ENV1; v->stage = 1; } break;
    case 1: v->env -= d_dec; if (v->env <= sus) { v->env = sus; v->stage = 2; } break;
    case 2: break;
    case 3: v->env -= r_dec; if (v->env <= 0) { v->env = 0; v->active = 0; } break;
    }
    return v->env >> 8;
}

#define MAXS (44100 * 30)
static int32_t rec[MAXS];
static long nrec;
static void render_block(void)
{
    int32_t bus[OP1_BLOCK] = { 0 };
    op1_beat_t *b = host_beat();
    if (b->len <= 0) { b->len = 22050 << 14; b->next = b->len; b->step = 0x100000; b->follows_tape = 0; }
    for (int i = 0; i < NV; i++) {
        if (!fv[i].active) continue;
        for (int n = 0; n < OP1_BLOCK; n++) host_pitch_inc[n] = inc_of(fv[i].note + fv[i].det / 100.0);
        int32_t e[OP1_BLOCK];
        for (int n = 0; n < OP1_BLOCK; n++) e[n] = env_step(&fv[i]);
        int16_t out[OP1_BLOCK];
        ENGINE.render(OP1_VOICE(i), out, knobs);
        for (int n = 0; n < OP1_BLOCK; n++) bus[n] += (out[n] * e[n]) >> 15;
    }
    b->next -= OP1_BLOCK << 14;
    while (b->next < 0) b->next += b->len;
    now_block++;
    for (int n = 0; n < OP1_BLOCK && nrec < MAXS; n++) rec[nrec++] = bus[n];
}
static void run_blocks(int nb) { while (nb-- > 0) render_block(); }
static int blocks_of(double s) { return (int)(s * 44100 / OP1_BLOCK + 0.5); }
static void hold(double s) { run_blocks(blocks_of(s)); }

/* an encoder turn: the knob moves from a to b in steps of `step`, one step every 6 blocks (~17 ms, the display rate).
 * DECLICK_SMOOTH=1: the same path, the knob moving a sixth of a step every block instead (a smooth reference, to
 * hear or diff against the stepped render) */
static int smooth_turns;
static void turn(int k, int a, int b, int step)
{
    const int dir = b > a ? 1 : -1;
    for (int v = a;; v += dir * step) {
        if ((dir > 0 && v > b) || (dir < 0 && v < b)) v = b;
        if (smooth_turns) {
            const int from = knobs[k];
            for (int j = 1; j <= 6; j++) { knobs[k] = (int16_t)(from + (v - from) * j / 6); run_blocks(1); }
        } else {
            knobs[k] = (int16_t)v;
            run_blocks(6);
        }
        if (v == b) break;
    }
}

/* ---- scenarios */
static const int16_t DEF[4] = { 16384, 16384, 12000, 12000 };
static int16_t start[4];                 /* the scenario's starting knobs (DEF, or DECLICK_KNOBS=a,b,c,d) */
static void mono_encoder(int k)
{
    mode = MONO; note_on(48, 110); hold(0.5);
    turn(k, knobs[k], 32767, 1500); turn(k, 32767, 0, 2500); turn(k, 0, 20000, 600); hold(0.3);
    all_off(); hold(0.5);
}
static void sc_mono_red(void) { mono_encoder(3); }
static void sc_mono_blue(void) { mono_encoder(0); }
static void sc_mono_green(void) { mono_encoder(1); }
static void sc_mono_white(void) { mono_encoder(2); }
static void sc_mono_red_jump(void)     /* the fastest turns: big steps, back and forth over the whole range */
{
    mode = MONO; note_on(43, 120); hold(0.4);
    for (int r = 0; r < 4; r++) { turn(3, knobs[3], 32767, 6000); turn(3, 32767, 0, 6000); }
    hold(0.3); all_off(); hold(0.5);
}
static void sc_mono_legato(void)       /* a mono melody, legato and repeated notes: the voice re-triggered while it sounds */
{
    mode = MONO;
    static const int mel[] = { 55, 57, 59, 60, 59, 57, 55, 55, 55, 62, 50, 0 };
    for (int i = 0; mel[i]; i++) { note_on(mel[i], 90 + 30 * (i & 1)); hold(0.22); }
    all_off(); hold(0.8);
}
static void sc_poly_heavy(void)        /* fast overlapping chords: more notes than voices, stealing all the time */
{
    mode = POLY;
    knobs[2] = 24000; knobs[3] = 26000;
    static const int ch[4][4] = { { 36, 48, 55, 63 }, { 41, 53, 60, 65 }, { 43, 50, 59, 67 }, { 38, 53, 57, 62 } };
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) {
            for (int i = 0; i < 4; i++) { note_on(ch[c][i], 100 + 9 * i); hold(0.03); }
            hold(0.25);
            for (int i = 0; i < 4; i++) note_off(ch[c][i]);
        }
    hold(1.0);
}
static void sc_poly_arp(void)          /* a fast arpeggio with a long release: every note steals a still-ringing voice */
{
    mode = POLY;
    r_dec = (24576 << 8) / 44100;
    knobs[3] = 20000;
    static const int arp[] = { 48, 55, 60, 63, 67, 72, 67, 63, 60, 55 };
    for (int r = 0; r < 4; r++) for (int i = 0; i < 10; i++) { note_on(arp[i] - 5 * (r & 1), 110); hold(0.07); note_off(arp[i] - 5 * (r & 1)); }
    hold(1.2);
    r_dec = (24576 << 8) / 13230;
}
static void sc_poly_red(void)          /* a held chord while CATHEDRAL turns */
{
    mode = POLY;
    note_on(43, 110); note_on(50, 110); note_on(55, 110); note_on(62, 110); hold(0.5);
    turn(3, knobs[3], 32767, 1500); turn(3, 32767, 0, 2500); hold(0.3);
    all_off(); hold(0.5);
}
static void sc_unison(void)            /* a legato unison melody: each note re-triggers all six sounding voices */
{
    mode = UNISON;
    static const int mel[] = { 50, 53, 55, 57, 55, 53, 50, 50, 57, 45, 0 };
    for (int i = 0; mel[i]; i++) { note_on(mel[i], 110); hold(0.25); }
    all_off(); hold(0.8);
}
static void sc_unison_heavy(void)      /* unison at heavy settings, staccato and legato */
{
    mode = UNISON;
    knobs[0] = 28000; knobs[2] = 26000; knobs[3] = 28000;
    static const int mel[] = { 38, 41, 43, 45, 43, 41, 38, 50, 0 };
    for (int i = 0; mel[i]; i++) { note_on(mel[i], 120); hold(0.18); if (i & 1) { note_off(mel[i]); hold(0.04); } }
    all_off(); hold(1.0);
}

typedef struct { const char *name; void (*fn)(void); } scen_t;
static const scen_t scen[] = {
    { "mono_red", sc_mono_red }, { "mono_red_jump", sc_mono_red_jump }, { "mono_blue", sc_mono_blue },
    { "mono_green", sc_mono_green }, { "mono_white", sc_mono_white }, { "mono_legato", sc_mono_legato },
    { "poly_heavy", sc_poly_heavy }, { "poly_arp", sc_poly_arp }, { "poly_red", sc_poly_red },
    { "unison", sc_unison }, { "unison_heavy", sc_unison_heavy },
};

static double floor_ = 400.0;           /* DECLICK_FLOOR: the smallest |d2| that counts */
static double d2(long i) { return i < 2 ? 0 : (double)rec[i] - 2.0 * rec[i - 1] + rec[i - 2]; }
static int clicks(double *worst, long *where, long *first)
{
    static double a[MAXS];
    for (long i = 0; i < nrec; i++) a[i] = fabs(d2(i));
    /* the largest |d2| per 32-sample cell, so the surroundings' maximum is quick */
    static double cell[MAXS / 32 + 1];
    const long nc = nrec / 32;
    for (long c = 0; c < nc; c++) { double m = 0; for (int j = 0; j < 32; j++) if (a[c * 32 + j] > m) m = a[c * 32 + j]; cell[c] = m; }
    int n = 0; long last = -100000; *worst = 0; *where = -1; *first = -1;
    for (long c = 2; c < nc - 2; c++) {
        double pk = 0; long pi = 0;
        for (int j = 0; j < 32; j++) if (a[c * 32 + j] > pk) { pk = a[c * 32 + j]; pi = c * 32 + j; }
        double around = 0;
        for (long g = c - 32; g <= c + 32; g++) if (g >= 0 && g < nc && (g < c - 1 || g > c + 1) && cell[g] > around) around = cell[g];
        const double r = pk / (around + 50.0);
        if (pk > floor_ && r > *worst) { *worst = r; *where = pi; }
        if (pk > floor_ && r > 3.0 && pi - last > 512) { if (!n) *first = pi; n++; last = pi; }
    }
    return n;
}
static void write_wav(const char *dir, const char *name)
{
    char path[512]; snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return; }
    uint32_t bytes = (uint32_t)(nrec * 2), v; uint16_t s;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 88200; fwrite(&v, 4, 1, f); s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (long i = 0; i < nrec; i++) { int16_t x = op1_sat16(rec[i]); fwrite(&x, 2, 1, f); }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *only = 0, *wdir = 0;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "-w") && i + 1 < argc) wdir = argv[++i]; else only = argv[i]; }
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    for (int k = 0; k < 4; k++) start[k] = DEF[k];
    smooth_turns = getenv("DECLICK_SMOOTH") != 0;
    if (getenv("DECLICK_FLOOR")) floor_ = atof(getenv("DECLICK_FLOOR"));
    const char *ov = getenv("DECLICK_KNOBS");
    if (ov) { int a[4]; if (sscanf(ov, "%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3]) == 4) for (int k = 0; k < 4; k++) start[k] = (int16_t)a[k]; }
    int total = 0;
    printf("%-14s %6s %7s %s\n", "scenario", "clicks", "worst", "at");
    for (size_t s = 0; s < sizeof scen / sizeof scen[0]; s++) {
        if (only && strcmp(only, scen[s].name)) continue;
        memset(fv, 0, sizeof fv); nrec = 0; memset(knobs, 0, sizeof knobs);
        for (int k = 0; k < 4; k++) knobs[k] = start[k];
        host_beat()->len = 0;
        scen[s].fn();
        double worst; long where, first;
        const int c = clicks(&worst, &where, &first);
        total += c;
        printf("%-14s %6d %7.2f %.3f s%s", scen[s].name, c, worst, where / 44100.0, c ? "" : "\n");
        if (c) printf("  (first %.3f s)\n", first / 44100.0);
        if (wdir) write_wav(wdir, scen[s].name);
    }
    printf("total %d\n", total);
    return 0;
}
