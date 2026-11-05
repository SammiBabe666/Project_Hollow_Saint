/* Memory-line probe for tests/mem_cost/run.sh: a six-voice chord at the given knobs, N blocks, mark() around each voice render */
/* Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot; const uint8_t *host_sample_buffer[8];
int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
void *host_find_node(void *s, const char *id) { return 0; } void host_node_set_transform(void *n, const int32_t *m) {}
void host_node_set_visible(void *n, int on) {} void host_screen_draw(void *s, int dt) {}
extern const op1_engine_t ENGINE_hollow_saint;
volatile int probe_mark;
__attribute__((noinline)) void mark(int v) { probe_mark = v; }
static int32_t inc_of(double midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }
int main(int argc, char **argv) {
  for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
  int16_t k[8] = { atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), 0, 0, 0, 0 };
  int nb = atoi(argv[5]);
  static const int notes[6] = { 36, 43, 48, 55, 60, 67 };
  op1_beat_t *b = host_beat(); b->len = 22050 << 14; b->next = b->len; b->step = 0x100000;
  for (int v = 0; v < 6; v++) { host_voice_vel[v] = 120 << 8; ENGINE_hollow_saint.note_on(OP1_VOICE(v), k); }
  for (int bl = 0; bl < nb; bl++) {
    for (int v = 0; v < 6; v++) {
      for (int n = 0; n < 128; n++) host_pitch_inc[n] = inc_of(notes[v]);
      int16_t out[128];
      mark(1000 + bl * 10 + v);
      ENGINE_hollow_saint.render(OP1_VOICE(v), out, k);
      mark(0);
    }
    b->next -= 128 << 14; while (b->next < 0) b->next += b->len;
  }
  return 0;
}
