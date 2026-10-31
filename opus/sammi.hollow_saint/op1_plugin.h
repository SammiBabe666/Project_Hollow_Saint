/* The kennel engine API that plugin/hollow_saint/hollow_saint.c is written against, provided on Opus 2.
 *
 * hollow_saint.c includes "op1_plugin.h". In the kennel build that is the SDK's own header; in the Opus build
 * (opus/sammi.hollow_saint) this file is found first, so the same engine source compiles unchanged for both. It maps:
 *   - the voice object, its index and velocity: the engine is registered with OPUS_ENGINE_OS_VOICES, so its voices
 *     are iter's own objects at 0xFFB00000 + 0x24 i, as on kennel (Opus 2's opus_plugin.h: "op1r's and kennel's find
 *     their voice and its velocity from the object's address");
 *   - OP1_PITCH_INC: the render's own pitch argument (hollow_saint_opus.c keeps it for the call);
 *   - OP1_SINE_TABLE: hs_sine.h, round(32767 sin(2 pi i / 8192)), the same values as the firmware's table, computed
 *     by tools/gen_opus_headers.py rather than read from the firmware;
 *   - op1_eng_page: the encoder pushes on the engine's T1 page, kept by hollow_saint_opus.c's key handler;
 *   - the screen calls: Opus 2's fw_* names (op1fw.h) for the same firmware functions;
 *   - the beat clock: read where kennel reads it (deep: see below).
 * Deep reads (firmware #246 addresses, as documented by kennel's SDK, no opus_* service gives them): the voice's
 * velocity (0xFF801064), the metronome's beat clock (0x01AADABC) and the tape's speed and position (0xFF802970,
 * 0xFF802974). They are read, never written.
 *
 * -DOPUS_HOST: the host build (tests/opus_host), with the firmware's memory replaced by arrays.
 *
 * Parts follow kennel's op1_plugin.h (Hundekraft, pcppcp; GPL-3.0-or-later with the OP-1 firmware permission).
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#ifndef OP1_PLUGIN_H
#define OP1_PLUGIN_H
#include <stdint.h>
#include "opus_plugin.h"
#include "op1fw.h"

#define OP1_BLOCK        128
#define OP1_VOICES       6
#define OP1_SAMPLE_RATE  44100
#define OP1_VOICE_SIZE   0x24u

#ifdef OPUS_HOST
extern uint8_t host_voice_mem[6 * 0x24];
extern int16_t host_voice_vel[6];
#define OP1_VOICE_BASE   ((uintptr_t)host_voice_mem)
#define OP1_VOICE_VEL    ((const volatile int16_t *)host_voice_vel)
#else
#define OP1_VOICE_BASE   0xFFB00000u
#define OP1_VOICE_VEL    ((const volatile int16_t *)0xFF801064)   /* velocity << 8, by VoiceCtl::note_on_synth */
#endif
#define OP1_VOICE(i)     ((void *)((uintptr_t)OP1_VOICE_BASE + (uintptr_t)(i) * OP1_VOICE_SIZE))

static inline int op1_voice_index(const void *self)
{ return (int)(((uintptr_t)self - (uintptr_t)OP1_VOICE_BASE) / OP1_VOICE_SIZE); }
static inline int32_t op1_voice_vel_gain(const void *self)
{
    int32_t v = OP1_VOICE_VEL[op1_voice_index(self)] >> 8;
    return v > 0 && v <= 127 ? v * 258 : 32767;
}

/* the voice being rendered: its 128 phase steps (hollow_saint_opus.c sets it before each render) */
extern const unsigned *hs_opus_pitch;
#define OP1_PITCH_INC    ((const int32_t *)hs_opus_pitch)
extern const int16_t hs_sine[8192];
#define OP1_SINE_TABLE   hs_sine

typedef struct {
    void (*note_on)(void *self, const int16_t *params);
    void (*render)(void *self, int16_t *out, const int16_t *params);
    void (*note_off)(void *self);
    uint8_t press[4];
} op1_engine_t;
#define OP1_ENG_PAGE_WORD(k) ((k) < 3 ? 5 + (k) : 7)
enum { OP1_PRESS_NONE = 0, OP1_PRESS_PAGE = 1, OP1_PRESS_LATCH = 2 };
extern volatile uint8_t op1_eng_page[4];

/* the beat clock, as kennel's op1_beat_clock (read once per render block) */
typedef struct {
    int32_t len;            /* beat length, 1/16384 samples (<= 0: no clock yet) */
    int32_t next;           /* time to the next beat from the first sample of this block, 1/16384 samples */
    int32_t step;           /* clock advance per 64-sample block: 0x100000 = real time, else the tape speed */
    int32_t tape_pos;       /* tape position (samples) */
    uint8_t follows_tape;   /* 1: the clock moves with the tape */
} op1_beat_t;
#ifdef OPUS_HOST
static inline op1_beat_t *host_beat(void) { static op1_beat_t b; return &b; }
static inline void op1_beat_clock(op1_beat_t *b) { *b = *host_beat(); }
#else
static inline void op1_beat_clock(op1_beat_t *b)
{
    const volatile uint8_t *m = (const volatile uint8_t *)0x01AADABC;
    b->len = *(const volatile int32_t *)(m + 0x1C);
    b->next = *(const volatile int32_t *)(m + 0x20);
    b->follows_tape = m[0x14];
    uint32_t sp = *(const volatile uint32_t *)0xFF802970;
    b->step = b->follows_tape ? (int32_t)((sp >> 21) << 14) : 0x100000;
    b->tape_pos = *(const volatile int32_t *)0xFF802974 >> 6;
}
#endif

/* ---- the screen (display thread): the engine's own OS screen, through Opus 2's screen_init / screen_draw ---- */
typedef struct {
    void (*init)(void *scr);
    void (*update)(void *scr, int dt);
} op1_screen_t;
/* an element by id from the screen's document ([scr + 0]); warn 0: a missing id returns 0 and never asserts */
#define op1_find_node(scr, id)          fw_svg_node_find(*(void **)(scr), (id), 0)
#define op1_node_set_transform(n, m9)   fw_node_set_matrix((n), (const int *)(m9))
#define op1_node_set_visible(n, on)     (((volatile uint8_t *)(n))[0x50] = (uint8_t)((on) != 0))   /* op1fw.h F0104 */
#define op1_screen_draw(scr, dt)        ((void)(dt), fw_screen_draw(scr))
static inline void op1_node_rotate(void *node, int32_t cx, int32_t cy, int32_t s15, int32_t c15)
{
    int32_t c = c15 >> 5, s = s15 >> 5;
    int32_t m[9] = { c, s, 0, -s, c, 0,
                     cx - ((cx * c - cy * s) >> 10), cy - ((cx * s + cy * c) >> 10), 1024 };
    op1_node_set_transform(node, m);
}
/* the current sound's knobs for the screen: Opus 2 has one current sound, so the slot is not needed */
#define OP1_SCREEN_SLOT 0
const int16_t *hs_opus_knobs(void);
void hs_opus_knobs_live(int16_t out[4]);
#define op1_synth_knobs(slot)            ((void)(slot), hs_opus_knobs())
#define op1_synth_knobs_live(slot, out)  ((void)(slot), hs_opus_knobs_live(out))

#endif
