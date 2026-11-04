/* A stand-in for the kennel SDK's op1_plugin.h on the PC, with just what hollow_saint.c and tests/suite.c use, so the
 * host suite builds without the hundekraft source bundle:
 *   cc -DOP1_HOST -I tests/host_sdk -I tests/host_shim tests/suite.c plugin/hollow_saint/hollow_saint.c -lm
 * The real header (KENNEL=.../synth_sdk) is still what tests/host.py and the release builds use.
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#ifndef OP1_PLUGIN_H
#define OP1_PLUGIN_H
#include <stdint.h>
#include "harness/op1_host.h"

#define OP1_BLOCK        128
#define OP1_VOICES       6
#define OP1_SAMPLE_RATE  44100
#define OP1_VOICE_SIZE   0x24u
#define OP1_VOICE(i)     ((void *)((uintptr_t)OP1_VOICE_BASE + (uintptr_t)(i) * OP1_VOICE_SIZE))

static inline int op1_voice_index(const void *self)
{ return (int)(((uintptr_t)self - (uintptr_t)OP1_VOICE_BASE) / OP1_VOICE_SIZE); }
static inline int32_t op1_voice_vel_gain(const void *self)
{
    int32_t v = OP1_VOICE_VEL[op1_voice_index(self)] >> 8;
    return v > 0 && v <= 127 ? v * 258 : 32767;
}
static inline int16_t op1_sat16(int32_t x) { return (int16_t)(x < -32768 ? -32768 : x > 32767 ? 32767 : x); }

typedef struct {
    void (*note_on)(void *self, const int16_t *params);
    void (*render)(void *self, int16_t *out, const int16_t *params);
    void (*note_off)(void *self);
    uint8_t press[4];
} op1_engine_t;
#define OP1_ENG_PAGE_WORD(k) ((k) < 3 ? 5 + (k) : 7)
enum { OP1_PRESS_NONE = 0, OP1_PRESS_PAGE = 1, OP1_PRESS_LATCH = 2 };
extern volatile uint8_t op1_eng_page[4];
extern volatile uint8_t op1_fx_page[3][4];

typedef struct { int32_t len, next, step, tape_pos; uint8_t follows_tape; } op1_beat_t;
static inline op1_beat_t *host_beat(void) { static op1_beat_t b; return &b; }
static inline void op1_beat_clock(op1_beat_t *b) { *b = *host_beat(); }

typedef struct { void (*init)(void *scr); void (*update)(void *scr, int dt); } op1_screen_t;
static inline void op1_node_rotate(void *node, int32_t cx, int32_t cy, int32_t s15, int32_t c15)
{
    int32_t c = c15 >> 5, s = s15 >> 5;
    int32_t m[9] = { c, s, 0, -s, c, 0, cx - ((cx * c - cy * s) >> 10), cy - ((cx * s + cy * c) >> 10), 1024 };
    op1_node_set_transform(node, m);
}
#endif
