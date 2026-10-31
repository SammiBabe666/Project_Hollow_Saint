/* The Opus 2 build's compiled Blackfin code in the instruction-set simulator: the plugin as the Opus kit compiles it
 * (opus/sammi.hollow_saint, -mcpu=bf524-any and the kit's flags), the core's names stubbed, run bare-metal with the
 * voices, velocity and beat clock at the firmware's addresses. It plays sim_main.c's workload through the Opus engine
 * struct and writes the raw audio (mode dump) or the cycles per voice block (-DSIM_CYCLES), so its audio can be
 * compared with the kennel build's, byte for byte (tests/bfin_sim/run_opus.sh).
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "hollow_saint_opus.c"
#include "tables.h"

int sim_syscall(int nr, const int *args);
uint32_t sim_cycles(void);
static void out_bytes(const void *p, int n) { int a[3] = { 1, (int)p, n }; sim_syscall(5, a); }
#ifdef SIM_CYCLES
static void out_text(const char *s) { int n = 0; while (s[n]) n++; out_bytes(s, n); }
static void out_num(uint32_t v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); out_text(b + i); }
#endif

/* the core, stubbed */
static const struct opus_engine *added;
int opus_engine_add(struct opl_self *s, const struct opus_engine *e) { (void)s; added = e; return 0x1234; }
int opus_art_text(struct opl_self *s, const char *n, const char *svg, unsigned len) { (void)s; (void)n; (void)svg; (void)len; return 0; }
int opus_engine_knobs(short par[8]) { for (int i = 0; i < 8; i++) par[i] = 0; return 1; }
int opus_engine_live(short par[8]) { for (int i = 0; i < 8; i++) par[i] = 0; return 1; }
int opus_engine_knob_set(int k, int value) { (void)k; (void)value; return 1; }
void opus_log(struct opl_self *s, const char *t) { (void)s; (void)t; }
void opus_page_text(struct opl_self *s, int sl, int x, int y, int z, int c, const char *t)
{ (void)s; (void)sl; (void)x; (void)y; (void)z; (void)c; (void)t; }
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned v, unsigned m, int c)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)v; (void)m; (void)c; }
void *fw_svg_node_find(void *node, const char *id, int warn) { (void)node; (void)id; (void)warn; return 0; }
void fw_node_set_matrix(void *node, const int *m9) { (void)node; (void)m9; }
void fw_screen_draw(void *screen) { (void)screen; }

int main(void);
int main(void)
{
    static struct opl_self self = { sizeof(struct opl_self), HS_ID, "/yaffs2/mod/" HS_ID, 0 };
    volatile uint8_t *m = (volatile uint8_t *)0x01AADABC;            /* the beat clock: none (len 0) */
    for (int i = 0; i < 0x24; i++) m[i] = 0;
    *(volatile uint32_t *)0xFF802970 = 0;
    *(volatile uint32_t *)0xFF802974 = 0;
    if (opl_init(&self) != 0 || !added) return 1;
    static const int16_t settings[4][4] = { { 16384, 16384, 16384, 16384 }, { 32767, 32767, 32767, 32767 }, { 0, 0, 0, 0 },
                                            { 32767, 32767, 32767, 32767 } };
    static const int notes[6] = { 36, 43, 48, 55, 60, 67 };
    static unsigned pitch[128];
    static short out[128];
    volatile int16_t *vel = (volatile int16_t *)0xFF801064;
#ifdef SIM_CYCLES
    uint32_t total[4];
#endif
    for (int s = 0; s < 4; s++) {
        short k[8] = { settings[s][0], settings[s][1], settings[s][2], settings[s][3], 0x5300 + 32, 0, 0, 16384 };
        if (s == 3) { k[6] = 32767; k[7] = 32767; }
        if ((s == 3) != (op1_eng_page[1] != 0)) { added->key(0x3C, 1, 0); added->key(0x3C, 0, 0); }
        for (int v = 0; v < 6; v++) {
            *(volatile uint32_t *)OP1_VOICE(v) = 0xC0DEF00Du;
            vel[v] = (int16_t)(100 << 8);
            added->note_on(OP1_VOICE(v), k);
        }
#ifdef SIM_CYCLES
        uint32_t t0 = sim_cycles();
#endif
        for (int b = 0; b < 60; b++)
            for (int v = 0; v < 6; v++) {
                for (int n = 0; n < 128; n++) pitch[n] = (unsigned)SIM_INC[notes[v]];
                added->render(OP1_VOICE(v), out, k, pitch);
#ifndef SIM_CYCLES
                out_bytes(out, (int)sizeof out);
#endif
            }
#ifdef SIM_CYCLES
        total[s] = sim_cycles() - t0;
#endif
        for (int v = 0; v < 6; v++) added->release(OP1_VOICE(v), k);
    }
#ifdef SIM_CYCLES
    static const char *const name[4] = { "mid", "max", "zero", "max+relic" };
    for (int s = 0; s < 4; s++) {
        out_text(name[s]); out_text(" "); out_num(total[s] / (60 * 6)); out_text(" cycles per voice-block\n");
    }
#endif
    return 0;
}
