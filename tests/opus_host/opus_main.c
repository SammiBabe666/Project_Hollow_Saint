/* The Opus 2 build of hollow_saint, on the PC: the plugin (opus/sammi.hollow_saint/hollow_saint_opus.c, which
 * includes the engine) against mocks of the core's opus_* and fw_* names.
 *
 *   ./opus_host audio     the workload of tests/bfin_sim/sim_main.c (6 voices x 4 settings x 60 blocks) through the
 *                         Opus engine struct, raw int16 to stdout: must be byte-identical to sim_main.c's host build
 *                         (the kennel API), so the glue changes nothing in the sound. Setting 4's Lo-Fi Relic is
 *                         switched on with a green push through the engine's key handler, as on the OP-1.
 *   ./opus_host checks    opl_init's registration, the pushes and the page-2 turns (prints each check, exit 1 on a
 *                         failure)
 *
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include <stdio.h>
#include <string.h>
#include "../../opus/sammi.hollow_saint/hollow_saint_opus.c"
#include "../bfin_sim/tables.h"

uint8_t host_voice_mem[6 * 0x24];
int16_t host_voice_vel[6];

/* ---- the core, mocked */
static const struct opus_engine *added;
static int art_written, art_fail;
static short sound[8];                                 /* the current sound's knobs */
int opus_engine_add(struct opl_self *self, const struct opus_engine *e) { (void)self; added = e; return 0x1234; }
int opus_art_text(struct opl_self *self, const char *name, const char *svg, unsigned len)
{ (void)self; (void)name; (void)svg; art_written = (int)len; return art_fail ? -1 : 0; }
int opus_engine_knobs(short par[8]) { memcpy(par, sound, sizeof sound); return 1; }
int opus_engine_live(short par[8]) { memcpy(par, sound, sizeof sound); return 1; }
int opus_engine_knob_set(int k, int value)
{ if (k == 4 || k < 0 || k > 7) return 0; sound[k] = (short)(value < 0 ? 0 : value > 32767 ? 32767 : value); return 1; }
void opus_log(struct opl_self *self, const char *text) { (void)self; fprintf(stderr, "log: %s\n", text); }
void opus_page_text(struct opl_self *self, int slot, int x, int y, int size, int colour, const char *text)
{ (void)self; (void)slot; (void)x; (void)y; (void)size; (void)colour; (void)text; }
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned value, unsigned max, int colour)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)value; (void)max; (void)colour; }
void *fw_svg_node_find(void *node, const char *id, int warn) { (void)node; (void)id; (void)warn; return 0; }
void fw_node_set_matrix(void *node, const int *m9) { (void)node; (void)m9; }
void fw_screen_draw(void *screen) { (void)screen; }

static struct opl_self self = { sizeof(struct opl_self), "sammi.hollow_saint", "/yaffs2/mod/sammi.hollow_saint", 0 };

static int audio(void)
{
    if (opl_init(&self) != 0 || !added) return 1;
    static const int16_t settings[4][4] = { { 16384, 16384, 16384, 16384 }, { 32767, 32767, 32767, 32767 }, { 0, 0, 0, 0 },
                                            { 32767, 32767, 32767, 32767 } };
    static const int notes[6] = { 36, 43, 48, 55, 60, 67 };
    static unsigned pitch[128];
    static short out[128];
    for (int s = 0; s < 4; s++) {
        short k[8] = { settings[s][0], settings[s][1], settings[s][2], settings[s][3], 0x5300 + 32, 0, 0, 16384 };
        if (s == 3) { k[6] = 32767; k[7] = 32767; }
        if ((s == 3) != (op1_eng_page[1] != 0)) {      /* green push: the relic on (setting 4 only) */
            added->key(0x3C, 1, 0);
            added->key(0x3C, 0, 0);
        }
        for (int v = 0; v < 6; v++) {
            *(uint32_t *)OP1_VOICE(v) = 0xC0DEF00Du;
            host_voice_vel[v] = (int16_t)(100 << 8);
            added->note_on(OP1_VOICE(v), k);
        }
        for (int b = 0; b < 60; b++)
            for (int v = 0; v < 6; v++) {
                for (int n = 0; n < 128; n++) pitch[n] = (unsigned)SIM_INC[notes[v]];
                added->render(OP1_VOICE(v), out, k, pitch);
                fwrite(out, 1, sizeof out, stdout);
            }
        for (int v = 0; v < 6; v++) added->release(OP1_VOICE(v), k);
    }
    return 0;
}

static int bad;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); bad |= !ok; }

static int checks(void)
{
    art_fail = 0;
    check(opl_init(&self) == 0 && added, "opl_init registers the engine");
    check(added->size == sizeof(struct opus_engine) && !strcmp(added->name, "hollow saint"), "its name");
    check(added->flags == OPUS_ENGINE_OS_VOICES && added->voice == 0, "iter's voices, as on kennel");
    check(added->type && !strcmp(added->type, "hollow saint"), "the kennel presets' type");
    check(added->svg && !strcmp(added->svg, "/yaffs2/mod/sammi.hollow_saint/hollow_saint.svg"),
          "the screen's svg in the plugin's folder");
    check(art_written == (int)sizeof HS_SVG - 1 && art_written > 10000, "the svg written whole");
    check(added->knobs[7] == 0 && stroke_zone(added->knobs) == 0, "a new sound's wheel: steady");
    check(added->knobs[0] == 16384 && added->knobs[3] == 12000, "a new sound's knobs: kennel's defaults");

    memcpy(sound, added->knobs, sizeof sound);
    check(added->turn(0, 1, 0) == 0, "blue turn, page 2 closed: the OS's");
    check(added->key(0x3B, 1, 0) == 1 && added->key(0x3B, 0, 0) == 1 && op1_eng_page[0], "blue push opens page 2");
    added->turn(0, 1, 0);
    check(drone_key(sound) == 1, "a detent: FOLLOW -> C");
    added->turn(0, 20, 0);
    check(drone_key(sound) == 12, "the drone key stops at B");
    added->turn(0, -3, 0);
    check(drone_key(sound) == 9, "back three: G#");
    check(added->turn(0, 0, 0) == 1 && drone_key(sound) == 9, "less than a detent: taken, unchanged");
    check(added->turn(0, 1, 1) == 0, "on T2: the OS's");
    added->key(0x3B, 1, 0); added->key(0x3B, 0, 0);
    check(!op1_eng_page[0] && added->turn(0, 1, 0) == 0, "blue push closes it");

    added->key(0x3D, 1, 0);
    for (int z = 1; z < NZONES; z++) { added->turn(2, 1, 0); if (stroke_zone(sound) != z) check(0, "white: a rate a detent"); }
    check(stroke_zone(sound) == NZONES - 1, "white page 2 walks steady .. 1/16");
    added->turn(2, -100, 0);
    check(stroke_zone(sound) == 0, "and back to steady");
    added->key(0x3D, 1, 0);

    added->key(0x3C, 1, 0);
    check(op1_eng_page[1], "green push: relic on");
    sound[6] = 0;
    added->turn(1, 1, 0);
    check(sound[6] == 8192 + 983, "the age starts at 8192 (unset)");
    added->turn(1, 100, 0);
    check(sound[6] == 32767, "and stops at the top");
    added->turn(1, -100, 0);
    check(sound[6] == 1, "and at 1 (0 would read as unset)");
    added->key(0x3C, 1, 0);
    check(!op1_eng_page[1], "green push: relic off");
    check(added->key(0x3E, 1, 0) == 0, "orange push: the OS's");

    art_fail = 1;
    check(opl_init(&self) == 0 && !added->svg && !added->screen_draw && added->page, "no room for the svg: plain page");
    return bad;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "audio")) return audio();
    if (argc > 1 && !strcmp(argv[1], "checks")) return checks();
    fprintf(stderr, "usage: opus_host audio|checks\n");
    return 2;
}
