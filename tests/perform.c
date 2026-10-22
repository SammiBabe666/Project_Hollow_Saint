/* Host performance: plays a script through the engine AND its screen, as the OP-1 would run them, and writes
 *   <out>.wav     the synth bus (6 voices, stock-like ADSR applied after render, stealing the oldest voice)
 *   <out>.jsonl   one line per video frame (30 fps): the screen's node transforms and hidden nodes after update(),
 *                 the knobs and the caption, for tools/render_screen.py / tools/make_tour.py
 * Script lines: "<seconds> <command> <args>"; commands:
 *   on <note> <vel> | off <note> | alloff | knob <i> <v> | ramp <i> <from> <to> <seconds> | word <5..7> <v> |
 *   page <knob> <0|1> | adsr <attack ms> <decay ms> <sustain 0..32767> <release ms> | bpm <bpm> | caption <text> |
 *   end
 * Screen calls are mocks that record (as op1r's harness/screen_harness.c does); the SVG is drawn by the renderer.
 *   usage: perform <script> <out basename>
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
void *read_fx_param_safe(int slot, int pos) { (void)slot; (void)pos; return 0; }

/* ---- screen mocks: record */
#define MAXN 96
static char node_id[MAXN][24]; static int32_t node_m[MAXN][9]; static uint8_t node_hid[MAXN]; static int nnodes;
void *host_find_node(void *scr, const char *id)
{
    (void)scr;
    for (int i = 0; i < nnodes; i++) if (!strcmp(node_id[i], id)) return (void *)(intptr_t)(i + 1);
    if (nnodes == MAXN) return 0;
    snprintf(node_id[nnodes], sizeof node_id[0], "%s", id);
    static const int32_t ident[9] = { 1024, 0, 0, 0, 1024, 0, 0, 0, 1024 };
    memcpy(node_m[nnodes], ident, sizeof ident);
    return (void *)(intptr_t)(++nnodes);
}
void host_node_set_transform(void *node, const int32_t *m9) { memcpy(node_m[(intptr_t)node - 1], m9, 36); }
void host_node_set_visible(void *node, int on) { node_hid[(intptr_t)node - 1] = !on; }
uint8_t *host_node_style_rgba(void *s, const char *id) { static uint8_t c[4]; (void)s; (void)id; return c; }
void host_screen_draw(void *s, int dt) { (void)s; (void)dt; }
void *host_raster(void) { return 0; }
void host_raster_line(void *r, const void *a, const void *b, int32_t w, const uint8_t *c) { (void)r; (void)a; (void)b; (void)w; (void)c; }
void host_raster_dim(void *r) { (void)r; }

extern const op1_engine_t ENGINE;
extern const op1_screen_t SCREEN;

/* ---- firmware stand-in (as tests/suite.c) */
#define NV OP1_VOICES
#define ENV1 (32767 << 8)
typedef struct { int active, gate, note, stage; int32_t env; uint32_t born; } fw_voice;
static fw_voice fv[NV];
static uint32_t now_block;
static int16_t *knobs = host_knobs[0];
static int32_t a_inc = ENV1 / 441, d_dec = (8192 << 8) / 4410, sus = 24576 << 8, r_dec = (24576 << 8) / 13230;
static int32_t inc_of(double midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }
static void note_on(int note, int vel)
{
    int best = -1;
    for (int i = 0; i < NV; i++) if (!fv[i].active) { best = i; break; }
    if (best < 0) { best = 0; for (int i = 1; i < NV; i++) if (fv[i].born < fv[best].born) best = i; }
    host_voice_vel[best] = (int16_t)(vel << 8);
    fv[best] = (fw_voice){ 1, 1, note, 0, fv[best].active ? fv[best].env : 0, now_block };
    ENGINE.note_on(OP1_VOICE(best), knobs);
}
static void note_off(int note)
{
    for (int i = 0; i < NV; i++)
        if (fv[i].active && fv[i].gate && fv[i].note == note) {
            fv[i].gate = 0; fv[i].stage = 3;
            if (ENGINE.note_off) ENGINE.note_off(OP1_VOICE(i));
        }
}
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
static int32_t bus[OP1_BLOCK];
static void render_block(void)
{
    for (int n = 0; n < OP1_BLOCK; n++) bus[n] = 0;
    for (int i = 0; i < NV; i++) {
        if (!fv[i].active) continue;
        for (int n = 0; n < OP1_BLOCK; n++) host_pitch_inc[n] = inc_of(fv[i].note);
        int32_t e[OP1_BLOCK];
        for (int n = 0; n < OP1_BLOCK; n++) e[n] = env_step(&fv[i]);
        for (int n = 0; n < OP1_BLOCK; n++) host_voice_env[n] = (int16_t)e[n];
        int16_t out[OP1_BLOCK];
        ENGINE.render(OP1_VOICE(i), out, knobs);
        for (int n = 0; n < OP1_BLOCK; n++) bus[n] += (out[n] * e[n]) >> 15;
    }
    op1_beat_t *b = host_beat();
    b->next -= OP1_BLOCK << 14;
    while (b->next < 0) b->next += b->len;
    now_block++;
}

/* ---- script */
typedef struct { double t; char cmd[12]; double a[4]; char text[160]; } ev_t;
static ev_t ev[4096]; static int nev;
typedef struct { int on; int k; double from, to, t0, dur; } ramp_t;
static ramp_t ramps[4];

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: perform <script> <out>\n"); return 2; }
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    FILE *sf = fopen(argv[1], "r"); if (!sf) { perror(argv[1]); return 2; }
    char line[256];
    while (fgets(line, sizeof line, sf) && nev < 4096) {
        if (line[0] == '#' || line[0] == '\n') continue;
        ev_t *e = &ev[nev];
        int off = 0;
        if (sscanf(line, "%lf %11s %n", &e->t, e->cmd, &off) < 2) continue;
        if (!strcmp(e->cmd, "caption")) { snprintf(e->text, sizeof e->text, "%s", line + off); e->text[strcspn(e->text, "\n")] = 0; }
        else sscanf(line + off, "%lf %lf %lf %lf", &e->a[0], &e->a[1], &e->a[2], &e->a[3]);
        nev++;
    }
    fclose(sf);
    double end_t = nev ? ev[nev - 1].t : 0;
    for (int i = 0; i < nev; i++) if (!strcmp(ev[i].cmd, "end")) end_t = ev[i].t;

    char path[512];
    snprintf(path, sizeof path, "%s.wav", argv[2]);
    FILE *wf = fopen(path, "wb");
    snprintf(path, sizeof path, "%s.jsonl", argv[2]);
    FILE *jf = fopen(path, "w");
    if (!wf || !jf) { perror(argv[2]); return 2; }
    const long total = (long)(end_t * 44100);
    uint32_t v32 = 0; uint16_t v16 = 0;
    fwrite("RIFF", 1, 4, wf); v32 = 36 + (uint32_t)total * 2; fwrite(&v32, 4, 1, wf); fwrite("WAVEfmt ", 1, 8, wf);
    v32 = 16; fwrite(&v32, 4, 1, wf); v16 = 1; fwrite(&v16, 2, 1, wf); fwrite(&v16, 2, 1, wf);
    v32 = 44100; fwrite(&v32, 4, 1, wf); v32 = 88200; fwrite(&v32, 4, 1, wf); v16 = 2; fwrite(&v16, 2, 1, wf); v16 = 16; fwrite(&v16, 2, 1, wf);
    fwrite("data", 1, 4, wf); v32 = (uint32_t)total * 2; fwrite(&v32, 4, 1, wf);

    knobs[4] = 0x5300 + 32;
    op1_beat_t *bc = host_beat(); bc->len = 22050 << 14; bc->next = bc->len; bc->step = 0x100000;
    static char screen_obj[64];
    SCREEN.init(screen_obj);
    char caption[160] = "";
    int ei = 0; long done = 0; double next_frame = 0; int frame = 0;
    while (done < total) {
        const double t = done / 44100.0;
        while (ei < nev && ev[ei].t <= t) {
            ev_t *e = &ev[ei++];
            if (!strcmp(e->cmd, "on")) note_on((int)e->a[0], (int)e->a[1]);
            else if (!strcmp(e->cmd, "off")) note_off((int)e->a[0]);
            else if (!strcmp(e->cmd, "alloff")) { for (int i = 0; i < NV; i++) if (fv[i].active && fv[i].gate) note_off(fv[i].note); }
            else if (!strcmp(e->cmd, "knob")) { knobs[(int)e->a[0]] = (int16_t)e->a[1]; ramps[(int)e->a[0] & 3].on = 0; }
            else if (!strcmp(e->cmd, "ramp")) ramps[(int)e->a[0] & 3] = (ramp_t){ 1, (int)e->a[0], e->a[1], e->a[2], e->t, e->a[3] };
            else if (!strcmp(e->cmd, "word")) knobs[(int)e->a[0]] = (int16_t)e->a[1];
            else if (!strcmp(e->cmd, "page")) op1_eng_page[(int)e->a[0] & 3] = (uint8_t)e->a[1];
            else if (!strcmp(e->cmd, "adsr")) {
                a_inc = ENV1 / (int32_t)(e->a[0] * 44.1 + 1); d_dec = (int32_t)((ENV1 - e->a[2] * 256) / (e->a[1] * 44.1 + 1));
                sus = (int32_t)e->a[2] << 8; r_dec = (int32_t)(sus / (e->a[3] * 44.1 + 1)) + 1;
            }
            else if (!strcmp(e->cmd, "bpm")) { bc->len = (int32_t)(2646000.0 / e->a[0] * 16384); bc->next = bc->len; }
            else if (!strcmp(e->cmd, "caption")) snprintf(caption, sizeof caption, "%s", e->text);
        }
        for (int r = 0; r < 4; r++) if (ramps[r].on) {
            double x = (t - ramps[r].t0) / ramps[r].dur; if (x >= 1) { x = 1; ramps[r].on = 0; }
            knobs[ramps[r].k] = (int16_t)(ramps[r].from + (ramps[r].to - ramps[r].from) * x);
        }
        render_block();
        for (int n = 0; n < OP1_BLOCK && done < total; n++, done++) { int16_t s = op1_sat16(bus[n]); fwrite(&s, 2, 1, wf); }
        while (done / 44100.0 >= next_frame) {                         /* a frame at 30 fps: two screen updates (60 Hz) */
            SCREEN.update(screen_obj, 16);
            SCREEN.update(screen_obj, 16);
            fprintf(jf, "{\"frame\":%d,\"t\":%.4f,\"knobs\":[%d,%d,%d,%d],\"words\":[%d,%d,%d],\"pages\":[%d,%d,%d,%d],\"caption\":\"",
                    frame, next_frame, knobs[0], knobs[1], knobs[2], knobs[3], knobs[5], knobs[6], knobs[7],
                    op1_eng_page[0], op1_eng_page[1], op1_eng_page[2], op1_eng_page[3]);
            for (const char *c = caption; *c; c++) { if (*c == '"' || *c == '\\') fputc('\\', jf); fputc(*c, jf); }
            fprintf(jf, "\",\"nodes\":{");
            for (int i = 0; i < nnodes; i++) {
                fprintf(jf, "%s\"%s\":[", i ? "," : "", node_id[i]);
                for (int j = 0; j < 9; j++) fprintf(jf, "%s%d", j ? "," : "", node_m[i][j]);
                fprintf(jf, "]");
            }
            fprintf(jf, "},\"hidden\":[");
            int first = 1;
            for (int i = 0; i < nnodes; i++) if (node_hid[i]) { fprintf(jf, "%s\"%s\"", first ? "" : ",", node_id[i]); first = 0; }
            fprintf(jf, "]}\n");
            frame++; next_frame = frame / 30.0;
        }
    }
    fclose(wf); fclose(jf);
    fprintf(stderr, "%s: %.1f s, %d frames\n", argv[2], end_t, frame);
    return 0;
}
