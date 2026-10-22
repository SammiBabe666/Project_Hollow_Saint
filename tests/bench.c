/* Relative CPU cost on the host: 6 voices, all active, rendered for 20 s of audio with the knobs at their defaults,
 * then at the maximum, then at zero; prints the mean ns per voice-block (128 samples). Built at -O2 without
 * sanitizers. This is only a comparison against engines that ship and run on the OP-1 (same host, same harness);
 * it does not measure the Blackfin.
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

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
void host_raster_dim(void *r) { (void)r; }
void *read_fx_param_safe(int slot, int pos) { (void)slot; (void)pos; return 0; }
void *host_text_ctor(void *o, int32_t x, int32_t y, int32_t s, const uint8_t *c) { (void)x; (void)y; (void)s; (void)c; return o; }
void host_text_set(void *t, const char *s) { (void)t; (void)s; }
void host_text_draw(void *t, void *r, const int32_t *m) { (void)t; (void)r; (void)m; }

extern const op1_engine_t ENGINE;
volatile int32_t sink;

static double run(const int16_t *k4)
{
    int16_t k[8] = { k4[0], k4[1], k4[2], k4[3], 0x5300 + 32, 0, 0, 0 };
    static const int notes[6] = { 36, 43, 48, 55, 60, 67 };
    int16_t out[OP1_BLOCK];
    for (int v = 0; v < 6; v++) {
        host_voice_vel[v] = 100 << 8;
        ENGINE.note_on(OP1_VOICE(v), k);
    }
    const int blocks = 44100 * 20 / OP1_BLOCK;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int b = 0; b < blocks; b++)
        for (int v = 0; v < 6; v++) {
            int32_t inc = (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (notes[v] - 69) / 12.0) / 44100);
            for (int n = 0; n < OP1_BLOCK; n++) { host_pitch_inc[n] = inc; host_voice_env[n] = 24576; }
            ENGINE.render(OP1_VOICE(v), out, k);
            sink += out[b & 127];
        }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / (blocks * 6.0);
}

int main(void)
{
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    static const int16_t mid[4] = { 16384, 16384, 16384, 16384 }, hi[4] = { 32767, 32767, 32767, 32767 }, lo[4] = { 0 };
    double a = run(mid), b = run(hi), c = run(lo);
    double worst = a > b ? (a > c ? a : c) : (b > c ? b : c);
    printf("%.0f (mid %.0f, max %.0f, zero %.0f)\n", worst, a, b, c);
    return 0;
}
