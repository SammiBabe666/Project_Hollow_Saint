/* Host performance of the Opus 2 build: tests/perform.c's script player, but through the Opus engine struct
 * (opus/sammi.hollow_saint/hollow_saint_opus.c, which includes the engine unchanged) against mocks of the core's
 * opus_* and fw_* names. It writes the same <out>.wav and <out>.jsonl as perform.c, for tools/make_tour.py --opus.
 *
 * The differences from perform.c are the Opus side only: the voices are driven through opus_engine's note_on /
 * release / render (render takes the pitch steps as its argument), the knobs are the core's current sound
 * (opus_engine_knobs / opus_engine_live), the screen runs through screen_init / screen_draw with fw_svg_node_find and
 * fw_node_set_matrix recorded, and "page <k> <0|1>" presses the encoder through the engine's key handler (0x3B + k),
 * as a push on T1 does. The envelope after render is the same stand-in as perform.c's (not the OS's own).
 *
 *   cc -DOP1_HOST -DOPUS_HOST -I opus/sammi.hollow_saint -I $OPUS_KIT/sdk tests/opus_host/opus_perform.c -lm
 *   usage: opus_perform <script> <out basename>
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../opus/sammi.hollow_saint/hollow_saint_opus.c"

uint8_t host_voice_mem[6 * 0x24];
int16_t host_voice_vel[6];

/* ---- the core, mocked */
static const struct opus_engine *added;
static short sound[8];                                 /* the current sound's knobs */
int opus_engine_add(struct opl_self *self, const struct opus_engine *e) { (void)self; added = e; return 0x1234; }
int opus_art_text(struct opl_self *self, const char *name, const char *svg, unsigned len)
{ (void)self; (void)name; (void)svg; (void)len; return 0; }
int opus_engine_knobs(short par[8]) { memcpy(par, sound, sizeof sound); return 1; }
int opus_engine_live(short par[8]) { memcpy(par, sound, sizeof sound); return 1; }
int opus_engine_knob_set(int k, int value)
{ if (k == 4 || k < 0 || k > 7) return 0; sound[k] = (short)(value < 0 ? 0 : value > 32767 ? 32767 : value); return 1; }
void opus_log(struct opl_self *self, const char *text) { (void)self; fprintf(stderr, "log: %s\n", text); }
void opus_page_text(struct opl_self *self, int slot, int x, int y, int size, int colour, const char *text)
{ (void)self; (void)slot; (void)x; (void)y; (void)size; (void)colour; (void)text; }
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned value, unsigned max, int colour)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)value; (void)max; (void)colour; }

/* ---- the screen's nodes, recorded: each handle is a node-sized block, its visible byte at 0x50 (op1fw.h F0104) */
#define MAXN 96
static char node_id[MAXN][24]; static int32_t node_m[MAXN][9]; static uint8_t node_mem[MAXN][0x60]; static int nnodes;
static int node_index(void *n) { return (int)(((uint8_t (*)[0x60])n) - node_mem); }
void *fw_svg_node_find(void *node, const char *id, int warn)
{
    (void)node; (void)warn;
    for (int i = 0; i < nnodes; i++) if (!strcmp(node_id[i], id)) return node_mem[i];
    if (nnodes == MAXN) return 0;
    snprintf(node_id[nnodes], sizeof node_id[0], "%s", id);
    static const int32_t ident[9] = { 1024, 0, 0, 0, 1024, 0, 0, 0, 1024 };
    memcpy(node_m[nnodes], ident, sizeof ident);
    node_mem[nnodes][0x50] = 1;
    return node_mem[nnodes++];
}
void fw_node_set_matrix(void *node, const int *m9) { memcpy(node_m[node_index(node)], m9, 36); }
void fw_screen_draw(void *screen) { (void)screen; }

static struct opl_self self = { sizeof(struct opl_self), "sammi.hollow_saint", "/yaffs2/mod/sammi.hollow_saint", 0 };

/* ---- firmware stand-in (as tests/perform.c) */
#define ENV1 (32767 << 8)
typedef struct { int active, gate, note, stage; int32_t env; uint32_t born; } fw_voice;
static fw_voice fv[NV];
static uint32_t now_block;
static int32_t a_inc = ENV1 / 441, d_dec = (8192 << 8) / 4410, sus = 24576 << 8, r_dec = (24576 << 8) / 13230;
static unsigned inc_of(double midi) { return (unsigned)llrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }
static void note_on(int note, int vel)
{
    int best = -1;
    for (int i = 0; i < NV; i++) if (!fv[i].active) { best = i; break; }
    if (best < 0) { best = 0; for (int i = 1; i < NV; i++) if (fv[i].born < fv[best].born) best = i; }
    host_voice_vel[best] = (int16_t)(vel << 8);
    fv[best] = (fw_voice){ 1, 1, note, 0, fv[best].active ? fv[best].env : 0, now_block };
    added->note_on(OP1_VOICE(best), sound);
}
static void note_off(int note)
{
    for (int i = 0; i < NV; i++)
        if (fv[i].active && fv[i].gate && fv[i].note == note) {
            fv[i].gate = 0; fv[i].stage = 3;
            added->release(OP1_VOICE(i), sound);
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
    static unsigned pitch[OP1_BLOCK];
    for (int n = 0; n < OP1_BLOCK; n++) bus[n] = 0;
    for (int i = 0; i < NV; i++) {
        if (!fv[i].active) continue;
        for (int n = 0; n < OP1_BLOCK; n++) pitch[n] = inc_of(fv[i].note);
        int32_t e[OP1_BLOCK];
        for (int n = 0; n < OP1_BLOCK; n++) e[n] = env_step(&fv[i]);
        short out[OP1_BLOCK];
        added->render(OP1_VOICE(i), out, sound, pitch);
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

static void push(int k)                                /* an encoder push on T1, through the engine's key handler */
{
    added->key(K_BLUE_PUSH + k, 1, 0);
    added->key(K_BLUE_PUSH + k, 0, 0);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: opus_perform <script> <out>\n"); return 2; }
    if (opl_init(&self) != 0 || !added || !added->screen_init) { fprintf(stderr, "opl_init failed\n"); return 1; }
    memcpy(sound, added->knobs, sizeof sound);
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

    sound[4] = 0x5300 + 32;
    op1_beat_t *bc = host_beat(); bc->len = 22050 << 14; bc->next = bc->len; bc->step = 0x100000;
    static void *doc = 0;
    static void *screen_obj[16] = { &doc };            /* [scr + 0]: the document */
    added->screen_init(screen_obj);
    char caption[160] = "";
    int ei = 0; long done = 0; double next_frame = 0; int frame = 0;
    while (done < total) {
        const double t = done / 44100.0;
        while (ei < nev && ev[ei].t <= t) {
            ev_t *e = &ev[ei++];
            if (!strcmp(e->cmd, "on")) note_on((int)e->a[0], (int)e->a[1]);
            else if (!strcmp(e->cmd, "off")) note_off((int)e->a[0]);
            else if (!strcmp(e->cmd, "alloff")) { for (int i = 0; i < NV; i++) if (fv[i].active && fv[i].gate) note_off(fv[i].note); }
            else if (!strcmp(e->cmd, "knob")) { sound[(int)e->a[0]] = (short)e->a[1]; ramps[(int)e->a[0] & 3].on = 0; }
            else if (!strcmp(e->cmd, "ramp")) ramps[(int)e->a[0] & 3] = (ramp_t){ 1, (int)e->a[0], e->a[1], e->a[2], e->t, e->a[3] };
            else if (!strcmp(e->cmd, "word")) opus_engine_knob_set((int)e->a[0], (int)e->a[1]);
            else if (!strcmp(e->cmd, "page")) { int k = (int)e->a[0] & 3; if (k < 3 && op1_eng_page[k] != (uint8_t)e->a[1]) push(k); }
            else if (!strcmp(e->cmd, "adsr")) {
                a_inc = ENV1 / (int32_t)(e->a[0] * 44.1 + 1); d_dec = (int32_t)((ENV1 - e->a[2] * 256) / (e->a[1] * 44.1 + 1));
                sus = (int32_t)e->a[2] << 8; r_dec = (int32_t)(sus / (e->a[3] * 44.1 + 1)) + 1;
            }
            else if (!strcmp(e->cmd, "bpm")) { bc->len = (int32_t)(2646000.0 / e->a[0] * 16384); bc->next = bc->len; }
            else if (!strcmp(e->cmd, "caption")) snprintf(caption, sizeof caption, "%s", e->text);
        }
        for (int r = 0; r < 4; r++) if (ramps[r].on) {
            double x = (t - ramps[r].t0) / ramps[r].dur; if (x >= 1) { x = 1; ramps[r].on = 0; }
            sound[ramps[r].k] = (short)(ramps[r].from + (ramps[r].to - ramps[r].from) * x);
        }
        render_block();
        for (int n = 0; n < OP1_BLOCK && done < total; n++, done++) { int16_t s = sat16(bus[n]); fwrite(&s, 2, 1, wf); }
        while (done / 44100.0 >= next_frame) {                         /* a frame at 30 fps: two screen draws (60 Hz) */
            added->screen_draw(screen_obj, 16);
            added->screen_draw(screen_obj, 16);
            fprintf(jf, "{\"frame\":%d,\"t\":%.4f,\"knobs\":[%d,%d,%d,%d],\"words\":[%d,%d,%d],\"pages\":[%d,%d,%d,%d],\"caption\":\"",
                    frame, next_frame, sound[0], sound[1], sound[2], sound[3], sound[5], sound[6], sound[7],
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
            for (int i = 0; i < nnodes; i++) if (!node_mem[i][0x50]) { fprintf(jf, "%s\"%s\"", first ? "" : ",", node_id[i]); first = 0; }
            fprintf(jf, "]}\n");
            frame++; next_frame = frame / 30.0;
        }
    }
    fclose(wf); fclose(jf);
    fprintf(stderr, "%s: %.1f s, %d frames\n", argv[2], end_t, frame);
    return 0;
}
