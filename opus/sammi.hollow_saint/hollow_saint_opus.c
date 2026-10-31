/* sammi.hollow_saint: Cathedral of the Hollow Saint on Opus 2, the same engine as the kennel .knl.
 *
 * The engine itself is plugin/hollow_saint/hollow_saint.c, included here unchanged (this folder's op1_plugin.h gives
 * it the kennel API on Opus 2). What this file adds is the Opus side:
 *   - an opus_engine in the OS's engine list, named "hollow saint", with OPUS_ENGINE_OS_VOICES (its voices are iter's
 *     objects, as on kennel) and the type "hollow saint", so the kennel presets and sounds load as this engine;
 *   - the encoder pushes on T1 (blue: the drone key page, green: the Lo-Fi Relic, white: the stroke rate page), which
 *     kennel's dispatcher did: a push toggles op1_eng_page[k]; while a page is open the encoder turns its page word
 *     (5, 6 or 7) with opus_engine_knob_set, so it is kept with the sound;
 *   - the screen: hollow_saint.svg is carried inside the plugin (hs_svg.h), written to the plugin's folder with
 *     opus_art_text and shown as the engine's own OS screen (svg, screen_init, screen_draw: core 2.6);
 *   - memset and memcpy, in case the compiler calls them (the core exports neither; kennel builds with
 *     -fno-tree-loop-distribute-patterns instead, and the Opus kit's flags are fixed). A "#pragma GCC optimize" for
 *     that flag would apply to the whole engine and cost it ~10 % in the simulator, so only these two get it, so
 *     that GCC cannot turn their own loops into calls to themselves.
 *
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include "../../plugin/hollow_saint/hollow_saint.c"
#include "hs_sine.h"
#include "hs_svg.h"

#define HS_ID       "sammi.hollow_saint"
#define HS_SVG_NAME "hollow_saint.svg"
#define HS_SVG_PATH "/yaffs2/mod/" HS_ID "/" HS_SVG_NAME

volatile uint8_t op1_eng_page[4];
const unsigned *hs_opus_pitch;
static struct opl_self *me;

#ifndef OPUS_HOST                                      /* the PC has its C library's */
#define NO_LIBCALL __attribute__((optimize("no-tree-loop-distribute-patterns")))
NO_LIBCALL void *memset(void *d, int c, unsigned n);
NO_LIBCALL void *memcpy(void *d, const void *s, unsigned n);
void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--) *p++ = *q++;
    return d;
}
#endif

/* ---------------------------------------------------------------- the voices: kennel's callbacks */

static void o_init(void *voice, int index)
{
    (void)voice;
    if (index >= 0 && index < NV) V[index].held = 0;   /* the OS built the voice again: no key holds it */
}

static void o_note_on(void *voice, const short *knobs) { ENGINE_hollow_saint.note_on(voice, knobs); }

static void o_release(void *voice, const short *knobs)
{
    (void)knobs;
    ENGINE_hollow_saint.note_off(voice);
}

static void o_render(void *voice, short *out, const short *par, const unsigned *pitch)
{
    hs_opus_pitch = pitch;
    ENGINE_hollow_saint.render(voice, out, par);
}

/* ---------------------------------------------------------------- page 2: the pushes and their turns */

#define K_BLUE_PUSH 0x3B
static int16_t sc_knobs[8];

const int16_t *hs_opus_knobs(void)
{
    short k[8];
    opus_engine_knobs(k);
    for (int i = 0; i < 8; i++) sc_knobs[i] = k[i];
    return sc_knobs;
}

void hs_opus_knobs_live(int16_t out[4])
{
    short k[8];
    opus_engine_live(k);
    for (int i = 0; i < 4; i++) out[i] = k[i];
}

/* a zone's middle as a page word: zone z of n */
static int zone_word(int z, int n) { return (z * 32768 + 16384) / n; }

static int o_turn(int encoder, int detents, int page)
{
    if (page != 0 || encoder < 0 || encoder > 2 || !op1_eng_page[encoder]
        || ENGINE_hollow_saint.press[encoder] != OP1_PRESS_PAGE)
        return 0;                                      /* the OS's: knobs 0-3 */
    if (!detents) return 1;
    short k[8];
    opus_engine_knobs(k);
    const int word = OP1_ENG_PAGE_WORD(encoder);
    int v;
    if (encoder == 0) {                                /* the drone key: FOLLOW, C .. B, a key a detent */
        v = clampi(drone_key(k) + detents, 0, 12);
        v = zone_word(v, 13);
    } else if (encoder == 2) {                         /* the stroke rate, a rate a detent */
        v = clampi(stroke_zone(k) + detents, 0, NZONES - 1);
        v = zone_word(v, NZONES);
    } else {                                           /* the relic's age: 3 % a detent (unset starts at 8192) */
        v = clampi((k[word] > 0 ? k[word] : 8192) + detents * 983, 1, 32767);
    }
    opus_engine_knob_set(word, v);
    return 1;
}

static int o_key(int key, int pressed, int page)
{
    if (page != 0 || key < K_BLUE_PUSH || key > K_BLUE_PUSH + 3) return 0;
    const int k = key - K_BLUE_PUSH;
    if (ENGINE_hollow_saint.press[k] != OP1_PRESS_PAGE) return 0;
    if (pressed) op1_eng_page[k] ^= 1;
    return 1;                                          /* press and release: ours */
}

/* ---------------------------------------------------------------- the screen */

static void o_screen_init(void *scr) { SCREEN_hollow_saint.init(scr); }
static void o_screen_draw(void *scr, int dt) { SCREEN_hollow_saint.update(scr, dt); }

/* without the screen (its file could not be written): the four knobs as bars, the page words as text */
static void o_page(void)
{
    static const char *const NAME[4] = { "drone", "strings", "wheel buzz", "cathedral" };
    short k[8];
    opus_engine_live(k);
    opus_page_text(me, 0, 10, 10, 13, OPUS_COLOR_TEXT, "hollow saint");
    for (int i = 0; i < 4; i++) {
        const int x = 10 + 78 * i;
        opus_page_text(me, 1 + i, x, 60, 11, OPUS_COLOR_E(i), NAME[i]);
        opus_page_bar(x, 80, x + 66, 84, (unsigned)(k[i] < 0 ? 0 : k[i]), 32767, OPUS_COLOR_E(i));
    }
    static const char *const RATE[NZONES] = { "steady", "1 bar", "1/2", "1/4", "1/8", "1/8t", "1/16" };
    static const char *const KEY[13] = { "follow", "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    opus_page_text(me, 5, 10, 100, 9, op1_eng_page[0] ? OPUS_COLOR_E1 : OPUS_COLOR_DIM, KEY[drone_key(k)]);
    opus_page_text(me, 6, 88, 100, 9, op1_eng_page[1] ? OPUS_COLOR_E2 : OPUS_COLOR_DIM,
                   op1_eng_page[1] ? "relic on" : "relic off");
    opus_page_text(me, 7, 166, 100, 9, op1_eng_page[2] ? OPUS_COLOR_E3 : OPUS_COLOR_DIM, RATE[stroke_zone(k)]);
}

/* ---------------------------------------------------------------- the engine */

static struct opus_engine HOLLOW_SAINT = {
    .size = sizeof(struct opus_engine),
    .name = "hollow saint",
    /* a new sound: kennel's defaults; page words: FOLLOW, relic age 8192, wheel steady (as 0.7.7) */
    .knobs = { 16384, 16384, 12000, 12000, 0, 0, 8192, 0 },
    .voice = 0,                                        /* OPUS_ENGINE_OS_VOICES: iter's objects, state in V[] */
    .init = o_init,
    .note_on = o_note_on,
    .release = o_release,
    .render = o_render,
    .page = o_page,
    .turn = o_turn,
    .key = o_key,
    .flags = OPUS_ENGINE_OS_VOICES,
    .type = "hollow saint",                            /* the kennel sounds' and presets' "type" */
    .svg = HS_SVG_PATH,
    .screen_init = o_screen_init,
    .screen_draw = o_screen_draw,
};

int opl_init(struct opl_self *self)
{
    me = self;
    if (opus_art_text(self, HS_SVG_NAME, HS_SVG, sizeof HS_SVG - 1) < 0) {
        HOLLOW_SAINT.svg = 0;                          /* no file: the plain page instead of the screen */
        HOLLOW_SAINT.screen_init = 0;
        HOLLOW_SAINT.screen_draw = 0;
        opus_log(self, "no room for the screen's svg: plain page");
    }
    return opus_engine_add(self, &HOLLOW_SAINT) < 0 ? -1 : 0;
}
