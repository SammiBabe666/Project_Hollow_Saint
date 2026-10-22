/* Host test suite for a kennel synth engine: simulates what the OP-1 firmware does around the engine callbacks.
 *
 *   - 6 voice objects at OP1_VOICE(i); the first word is "firmware-owned" and checked after every call
 *   - voice allocation with stealing of the oldest voice; note_on with a STALE OP1_PITCH_INC (the firmware writes the
 *     pitch for the voice being rendered, not at note_on), note_off without knobs
 *   - per voice, per sample: OP1_PITCH_INC and OP1_VOICE_ENV filled just before render; voices rendered in ascending
 *     order; the stock-like ADSR applied to the output AFTER render and the voices summed (int32, as the synth bus)
 *   - the beat clock (op1_beat_clock) advanced per block at 120 BPM
 *
 * Scenarios write build/host/suite/<name>.wav (the summed bus, saturated to int16 for listening) and print metrics:
 *   peak / RMS of the bus (int32, before saturation: > 32767 means the bus would clip), DC (mean), clicks (second-
 *   difference spikes far above their running level) and, for sustained tests, the RMS drift between the start and the
 *   end of the hold (runaway resonance / decay).
 * Exit status 1 when a hard check fails: vtable word written, NaN-like garbage, bus peak over the limit, DC over the
 * limit, runaway (RMS growth) or a silent scenario.
 *   usage: suite <out dir> [scenario]
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
#define VTABLE_MAGIC 0xC0DEF00Du
typedef struct { int active, gate, note, stage; int32_t env; uint32_t born; } fw_voice;
static fw_voice fv[NV];
static uint32_t now_block;
static int16_t knobs[8] = { 0, 0, 0, 0, 0x5300 + 32, 0, 0, 0 };
/* stock-like ADSR (the OP-1's T2 page), kept in Q23 so slow stages still move: attack, decay, sustain, release */
#define ENV1 (32767 << 8)
static int32_t a_inc = ENV1 / 441, d_dec = (8192 << 8) / 4410, sus = 24576 << 8, r_dec = (24576 << 8) / 13230;   /* 10 ms, 100 ms, 0.75, 300 ms */
static double bend_cents;                       /* pitch offset for all voices (vibrato / bend tests) */

static int32_t inc_of(double midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }
static void *vobj(int i) { return OP1_VOICE(i); }
static uint32_t *vtab(int i) { return (uint32_t *)vobj(i); }

static int fails;
static void fail(const char *what, const char *scen) { printf("  FAIL %s: %s\n", scen, what); fails++; }

static void note_on(int note, int vel)
{
    int best = -1;
    for (int i = 0; i < NV; i++) if (!fv[i].active) { best = i; break; }
    if (best < 0) {                                         /* steal the oldest */
        best = 0;
        for (int i = 1; i < NV; i++) if (fv[i].born < fv[best].born) best = i;
    }
    /* stale pitch: whatever the last rendered voice left there (here: a far-away note) */
    for (int n = 0; n < OP1_BLOCK; n++) host_pitch_inc[n] = inc_of(note > 60 ? 30 : 100);
    host_voice_vel[best] = (int16_t)(vel << 8);
    fv[best] = (fw_voice){ 1, 1, note, 0, fv[best].active ? fv[best].env : 0, now_block };   /* stolen: env restarts from where it was */
    ENGINE.note_on(vobj(best), knobs);
}
static void note_off(int note)
{
    for (int i = 0; i < NV; i++)
        if (fv[i].active && fv[i].gate && fv[i].note == note) {
            fv[i].gate = 0; fv[i].stage = 3;
            if (ENGINE.note_off) ENGINE.note_off(vobj(i));
        }
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

/* ---- one bus block: all active voices, ascending ---- */
static int32_t bus[OP1_BLOCK];
static void render_block(void)
{
    for (int n = 0; n < OP1_BLOCK; n++) bus[n] = 0;
    op1_beat_t *b = host_beat();
    if (b->len <= 0) { b->len = 22050 << 14; b->next = b->len; b->step = 0x100000; b->follows_tape = 0; }
    for (int i = 0; i < NV; i++) {
        if (!fv[i].active) continue;
        const double m = fv[i].note + bend_cents / 100.0;
        for (int n = 0; n < OP1_BLOCK; n++) host_pitch_inc[n] = inc_of(m);
        int32_t e[OP1_BLOCK];
        fw_voice save = fv[i];
        for (int n = 0; n < OP1_BLOCK; n++) e[n] = env_step(&fv[i]);
        const int ended = !fv[i].active;
        fv[i] = save;
        for (int n = 0; n < OP1_BLOCK; n++) host_voice_env[n] = (int16_t)e[n];
        int16_t out[OP1_BLOCK];
        for (int n = 0; n < OP1_BLOCK; n++) out[n] = (int16_t)0x5A5A;       /* the engine must write all 128 */
        ENGINE.render(vobj(i), out, knobs);
        for (int n = 0; n < OP1_BLOCK; n++) bus[n] += (out[n] * e[n]) >> 15;
        for (int n = 0; n < OP1_BLOCK; n++) env_step(&fv[i]);
        if (ended) fv[i].active = 0;
        if (*vtab(i) != VTABLE_MAGIC) { fail("engine wrote the firmware-owned vtable word", "render"); *vtab(i) = VTABLE_MAGIC; }
    }
    b->next -= OP1_BLOCK << 14;
    while (b->next < 0) b->next += b->len;
    now_block++;
}

/* ---- recording + metrics ---- */
#define MAXS (44100 * 40)
static int32_t rec[MAXS];
static long nrec;
static void run_blocks(int nb)
{
    for (int k = 0; k < nb; k++) {
        render_block();
        for (int n = 0; n < OP1_BLOCK && nrec < MAXS; n++) rec[nrec++] = bus[n];
    }
}
static int blocks_of(double sec) { return (int)(sec * 44100 / OP1_BLOCK + 0.5); }
static void hold(double sec) { run_blocks(blocks_of(sec)); }

typedef struct { const char *name; void (*fn)(void); int32_t peak_limit; int sustain_check; double hold_from, hold_to; } scen_t;

static const char *outdir;
static void write_wav(const char *name)
{
    char path[512]; snprintf(path, sizeof path, "%s/%s.wav", outdir, name);
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(2); }
    uint32_t bytes = (uint32_t)(nrec * 2), v;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); uint16_t s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 88200; fwrite(&v, 4, 1, f); s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (long i = 0; i < nrec; i++) { int16_t x = op1_sat16(rec[i]); fwrite(&x, 2, 1, f); }
    fclose(f);
}
static double rms_range(long a, long b)
{
    double s = 0; if (b > nrec) b = nrec; if (a < 0) a = 0; if (b <= a) return 0;
    for (long i = a; i < b; i++) s += (double)rec[i] * rec[i];
    return sqrt(s / (double)(b - a));
}
/* clicks: broadband transients that stand out from their surroundings. The second difference (a crude high-pass,
 * mostly > 5 kHz) is summed in 512-sample frames (11.6 ms); a frame is a click when its energy is > 8x the median of
 * the 32 frames around it (~0.37 s) and above an absolute floor. Periodic waveform edges are in every frame and do not
 * stand out; a discontinuity, a zipper step or a spike does. Note onsets are reported too when they are harsh: the
 * scenario's timing tells which is which. */
#define CF 512
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static int count_clicks(long *first)
{
    static double fe[MAXS / CF + 1];
    long nf = nrec / CF; int c = 0; *first = -1;
    for (long f = 0; f < nf; f++) {
        double s = 0;
        for (long i = f * CF + (f ? 0 : 2); i < (f + 1) * CF; i++) {
            double d2 = (double)rec[i] - 2.0 * rec[i - 1] + rec[i - 2];
            s += d2 * d2;
        }
        fe[f] = s / CF;
    }
    for (long f = 0; f < nf; f++) {
        double w[33]; int m = 0;
        for (long g = f - 16; g <= f + 16; g++) if (g >= 0 && g < nf && g != f) w[m++] = fe[g];
        if (m < 8) continue;
        qsort(w, (size_t)m, sizeof w[0], cmp_d);
        double med = w[m / 2];
        if (fe[f] > 8 * med && fe[f] > 300.0 * 300.0) { if (c == 0) *first = f * CF; c++; }
    }
    return c;
}
static void reset_all(void)
{
    for (int i = 0; i < NV; i++) { fv[i] = (fw_voice){ 0 }; *vtab(i) = VTABLE_MAGIC; }
    nrec = 0; bend_cents = 0;
    op1_beat_t *b = host_beat(); b->len = 0;
}

/* ---- scenarios ---- */
static void sc_phrase(void)       /* legato melody over a held low note (the "hold your own drone" technique) */
{
    static const int mel[] = { 62, 64, 65, 67, 69, 67, 65, 64, 62, 60, 62, 0 };
    note_on(38, 110); hold(0.3);
    int prev = 0;
    for (int i = 0; mel[i]; i++) {
        note_on(mel[i], i & 1 ? 80 : 120);
        if (prev) { hold(0.02); note_off(prev); }
        hold(i % 4 == 3 ? 0.6 : 0.3);
        prev = mel[i];
    }
    note_off(prev); note_off(38); hold(1.0);
}
static void sc_chord6(void) { int c[] = { 36, 43, 48, 51, 55, 60 }; for (int i = 0; i < 6; i++) note_on(c[i], 127); hold(3.0); all_off(); hold(1.0); }
static void sc_single(void) { note_on(48, 127); hold(4.0); all_off(); hold(0.8); }
static void sc_tail(void)         /* a short note with a long release: the cathedral's space and the bell */
{
    r_dec = (24576 << 8) / (44100 * 3);                    /* 3 s release */
    note_on(55, 127); hold(0.6); all_off(); hold(3.4);
    r_dec = (24576 << 8) / 13230;
}
static void sc_range(void)
{
    for (int m = 21; m <= 108; m += 3) { note_on(m, 127); hold(0.35); note_off(m); hold(0.05); }
    hold(0.4);
}
static void sweep(int k)
{
    note_on(48, 127); note_on(55, 127); hold(0.5);
    const int nb = blocks_of(5.0);
    for (int b = 0; b < nb; b++) { knobs[k] = (int16_t)(32767L * b / (nb - 1)); run_blocks(1); }
    hold(0.5); all_off(); hold(0.6);
}
static void sc_sweep0(void) { sweep(0); }
static void sc_sweep1(void) { sweep(1); }
static void sc_sweep2(void) { sweep(2); }
static void sc_sweep3(void) { sweep(3); }
static void sc_steal(void)        /* fast notes, long release: stealing every note; repeated same-note retriggers */
{
    r_dec = (24576 << 8) / 44100;                          /* 1 s release */
    static const int seq[] = { 48, 55, 60, 63, 67, 72, 48, 50, 53, 58, 62, 65, 48, 48, 48, 48, 72, 36, 0 };
    for (int i = 0; seq[i]; i++) { note_on(seq[i], 100 + (i & 1) * 27); hold(0.09); note_off(seq[i]); hold(0.03); }
    hold(1.2);
    r_dec = (24576 << 8) / 13230;
}
static void sc_extreme(void)      /* the 16 knob corners, a 4-note chord each, sustained */
{
    for (int c = 0; c < 16; c++) {
        for (int k = 0; k < 4; k++) knobs[k] = (c >> k) & 1 ? 32767 : 0;
        note_on(36, 127); note_on(48, 127); note_on(55, 127); note_on(63, 127);
        hold(1.2); all_off(); hold(0.35);
    }
}
static void sc_vibrato(void)      /* per-sample pitch movement (LFO / bend): the engine must follow OP1_PITCH_INC */
{
    note_on(57, 120);
    for (int b = 0; b < blocks_of(3.0); b++) { bend_cents = 60 * sin(b * 0.06); run_blocks(1); }
    bend_cents = 0; all_off(); hold(0.6);
}
static void sc_handover(void)     /* drone ownership moving: legato up, a lower note, staccato, a chord, its bass off */
{
    note_on(62, 110); hold(0.5);
    note_on(65, 110); hold(0.1); note_off(62); hold(0.5);      /* higher note joins; owner moves at the release */
    note_on(60, 110); hold(0.1); note_off(65); hold(0.5);      /* lower note: takes the drone at once */
    note_off(60); hold(0.15);                                   /* staccato gap: the released owner keeps it */
    note_on(67, 110); hold(0.5);
    note_on(55, 110); note_on(59, 110); note_on(62, 110); hold(0.6);   /* chord: the bass (55) owns it */
    note_off(55); hold(0.6);                                    /* bass off: hand-over to 59 */
    all_off(); hold(1.0);
}
static void sc_long(void)         /* 20 s hold at maximum everything: runaway / drift */
{
    for (int k = 0; k < 4; k++) knobs[k] = 32767;
    note_on(36, 127); note_on(43, 127); note_on(48, 127); hold(20.0); all_off(); hold(1.0);
}

static const int16_t DEF[4] = { 16384, 16384, 12000, 12000 };
/* per-scenario: bus peak limit (int32 before the bus saturates), sustained-section check */
static scen_t scen[] = {
    { "phrase",  sc_phrase,  32767, 0, 0, 0 },
    { "single",  sc_single,  32767, 1, 1.0, 4.0 },
    { "chord6",  sc_chord6,  40000, 1, 1.0, 3.0 },
    { "range",   sc_range,   32767, 0, 0, 0 },
    { "sweep0",  sc_sweep0,  32767, 0, 0, 0 },
    { "sweep1",  sc_sweep1,  32767, 0, 0, 0 },
    { "sweep2",  sc_sweep2,  32767, 0, 0, 0 },
    { "sweep3",  sc_sweep3,  32767, 0, 0, 0 },
    { "steal",   sc_steal,   40000, 0, 0, 0 },
    { "extreme", sc_extreme, 40000, 0, 0, 0 },
    { "vibrato", sc_vibrato, 32767, 0, 0, 0 },
    { "long",    sc_long,    40000, 1, 2.0, 20.0 },
    { "handover", sc_handover, 32767, 0, 0, 0 },
    { "tail",    sc_tail,    32767, 0, 0, 0 },
};

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : ".";
    mkdir(outdir, 0755);
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    printf("%-8s %8s %7s %8s %7s %7s %s\n", "scenario", "peak", "dBFS", "rms", "dc", "clicks", "notes");
    for (size_t s = 0; s < sizeof scen / sizeof scen[0]; s++) {
        if (argc > 2 && strcmp(argv[2], scen[s].name)) continue;
        reset_all();
        for (int k = 0; k < 4; k++) knobs[k] = DEF[k];
        const char *ov = getenv("KNOBS");                 /* KNOBS=a,b,c,d: other starting knobs */
        if (ov) {                                         /* 4 knobs, optionally page-2 words 5..7 */
            int a[7] = { 0 }; int m = sscanf(ov, "%d,%d,%d,%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);
            for (int k = 0; k < m && k < 4; k++) knobs[k] = (int16_t)a[k];
            for (int k = 4; k < m; k++) knobs[k + 1] = (int16_t)a[k];
        }
        scen[s].fn();
        write_wav(scen[s].name);
        int32_t peak = 0; double sum = 0;
        for (long i = 0; i < nrec; i++) { int32_t a = abs(rec[i]); if (a > peak) peak = a; sum += rec[i]; }
        double rms = rms_range(0, nrec), dc = sum / (double)(nrec ? nrec : 1);
        long first; int clicks = count_clicks(&first);
        char note[160] = "";
        if (scen[s].sustain_check) {
            long a = (long)(scen[s].hold_from * 44100), b = (long)(scen[s].hold_to * 44100);
            double r0 = rms_range(a, a + 22050), r1 = rms_range(b - 22050, b);
            snprintf(note, sizeof note, "hold rms %.0f -> %.0f (x%.2f)", r0, r1, r1 / (r0 + 1e-9));
            if (r1 > 2.0 * r0 + 100) fail("runaway: RMS grew during the hold", scen[s].name);
            if (r1 < 0.25 * r0) fail("sustain collapsed during the hold", scen[s].name);
        }
        if (clicks) snprintf(note + strlen(note), sizeof note - strlen(note), "%sfirst click %.3f s", *note ? ", " : "", first / 44100.0);
        printf("%-8s %8d %7.1f %8.0f %7.1f %7d %s\n", scen[s].name, peak, 20 * log10(peak / 32768.0 + 1e-12), rms, dc, clicks, note);
        if (peak > scen[s].peak_limit) fail("bus peak over the limit", scen[s].name);
        if (fabs(dc) > 150) fail("DC offset", scen[s].name);
        if (peak < 200) fail("silent", scen[s].name);
    }
    printf(fails ? "%d check(s) FAILED\n" : "all checks passed\n", fails);
    return fails != 0;
}
