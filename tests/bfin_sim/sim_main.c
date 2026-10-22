/* One workload, two builds:
 *   Blackfin, in the instruction-set simulator (sim/bfin): the engine compiled as for the .knl (packer flags), at the
 *   firmware's own fixed addresses (pitch, envelope, velocity, voice objects, sine table), run bare-metal;
 *   host (-DOP1_HOST): the same engine source against the SDK host mocks.
 * Both write the same raw int16 audio (6 voices x 3 knob settings, every voice block) to stdout, so the two outputs
 * must be identical byte for byte. The Blackfin build also prints cycle counts (mode "cycles", argv-free: built with
 * -DSIM_CYCLES) per voice block for each setting.
 * No floating point, no libc: the tables are precomputed (tables.h).
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include "tables.h"

extern const op1_engine_t ENGINE;
#define BLOCKS 60                        /* per voice and setting (~0.17 s) */

#ifdef OP1_HOST
#include <stdio.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3];
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
void host_raster_dim(void *r) { (void)r; }
void *read_fx_param_safe(int slot, int pos) { (void)slot; (void)pos; return 0; }
static void out_bytes(const void *p, int n) { fwrite(p, 1, (size_t)n, stdout); }
static void beat_reset(void) { op1_beat_t *b = host_beat(); b->len = 0; b->next = 0; b->step = 0; b->follows_tape = 0; b->tape_pos = 0; }
#else
volatile uint8_t op1_eng_page[4];
int sim_syscall(int nr, const int *args);
uint32_t sim_cycles(void);
static void out_bytes(const void *p, int n) { int a[3] = { 1, (int)p, n }; sim_syscall(5, a); }
static void beat_reset(void)            /* the metronome state op1_beat_clock reads: no clock (len 0) */
{
    volatile uint8_t *m = (volatile uint8_t *)0x01AADABC;
    for (int i = 0; i < 0x24; i++) m[i] = 0;
    *(volatile uint32_t *)0xFF802970 = 0;
    *(volatile uint32_t *)0xFF802974 = 0;
}
static void out_text(const char *s) { int n = 0; while (s[n]) n++; out_bytes(s, n); }
static void out_num(uint32_t v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); out_text(b + i); }
#endif

int main(void)
{
    int16_t *sine = (int16_t *)OP1_SINE_TABLE;
    for (int i = 0; i < 8192; i++) sine[i] = SIM_SINE[i];
    beat_reset();
    static const int16_t settings[3][4] = { { 16384, 16384, 16384, 16384 }, { 32767, 32767, 32767, 32767 }, { 0, 0, 0, 0 } };
    static const int notes[6] = { 36, 43, 48, 55, 60, 67 };
    int32_t *pitch = (int32_t *)OP1_PITCH_INC;
    int16_t *env = (int16_t *)OP1_VOICE_ENV;
    int16_t *vel = (int16_t *)OP1_VOICE_VEL;
    static int16_t out[OP1_BLOCK];
#ifdef SIM_CYCLES
    uint32_t total[3];
#endif
    for (int s = 0; s < 3; s++) {
        int16_t k[8] = { settings[s][0], settings[s][1], settings[s][2], settings[s][3], 0x5300 + 32, 0, 0, 16384 };
        for (int v = 0; v < 6; v++) {
            *(uint32_t *)OP1_VOICE(v) = 0xC0DEF00Du;                /* the firmware's word */
            vel[v] = (int16_t)(100 << 8);
            ENGINE.note_on(OP1_VOICE(v), k);
        }
#ifdef SIM_CYCLES
        uint32_t t0 = sim_cycles();
#endif
        for (int b = 0; b < BLOCKS; b++)
            for (int v = 0; v < 6; v++) {
                for (int n = 0; n < OP1_BLOCK; n++) { pitch[n] = SIM_INC[notes[v]]; env[n] = 24576; }
                ENGINE.render(OP1_VOICE(v), out, k);
#ifndef SIM_CYCLES
                out_bytes(out, (int)sizeof out);
#endif
            }
#ifdef SIM_CYCLES
        total[s] = sim_cycles() - t0;
#endif
        for (int v = 0; v < 6; v++) if (ENGINE.note_off) ENGINE.note_off(OP1_VOICE(v));
    }
#ifdef SIM_CYCLES
    static const char *const name[3] = { "mid", "max", "zero" };
    for (int s = 0; s < 3; s++) {
        out_text(name[s]); out_text(" "); out_num(total[s] / (BLOCKS * 6)); out_text(" cycles per voice-block\n");
    }
#endif
    return 0;
}
