/* Run the OP-1's own plugin loader (kennel synth_sdk/kennel/kn_load.c, what kn_boot.c calls at boot) on the PC:
 *   kn_load_host FILE.knl BASE [NAME=ADDR ...] [-o image.bin]
 * Parses the file with kn_parse (API 1, as the 3.0.3 core), prints what the loader sees, relocates the image to BASE
 * with kn_relocate and the given imports, and writes it (to compare with the reference loader tools/kennel/reloc.py).
 * Exit 1 when the loader refuses the file. (The SDK's own driver, harness/kennel_load_host.c, is not in the source
 * bundle; this is a stand-in for it.)
 * Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission. */
#include "kennel/kn_load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: kn_load_host FILE.knl BASE [NAME=ADDR ...] [-o out.bin]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static uint8_t file[1 << 20];
    const uint32_t size = (uint32_t)fread(file, 1, sizeof file, f);
    fclose(f);
    const uint32_t base = (uint32_t)strtoul(argv[2], 0, 0);
    kn_export ex[64]; int nex = 0; const char *out = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) { out = argv[++i]; continue; }
        char *eq = strchr(argv[i], '=');
        if (!eq || nex == 64) continue;
        *eq = 0; ex[nex].name = argv[i]; ex[nex].addr = (uint32_t)strtoul(eq + 1, 0, 0); nex++;
    }
    kn_info in; const char *err = 0;
    if (kn_parse(file, size, 1, &in, &err)) { printf("kn_parse refused: %s\n", err); return 1; }
    printf("kn_parse ok: \"%s\" kind %u order %u flags %#x defaults %d %d %d %d page2 %d %d %d %d\n", in.type_name,
           in.kind, in.legacy_order, in.flags, in.defaults[0], in.defaults[1], in.defaults[2], in.defaults[3],
           in.page2[0], in.page2[1], in.page2[2], in.page2[3]);
    printf("  image %u (text %u), load %u, align %u, entry %#x, screen %#x, svg %u bytes, crc %#010x\n", in.image_size,
           in.text_size, in.load_size, in.image_align, in.entry_offset, in.screen_offset, in.svg_size, in.crc);
    printf("  author \"%s\" version \"%s\" claims \"%s\"\n", in.author ? in.author : "-", in.version ? in.version : "-",
           in.claims ? in.claims : "-");
    static uint8_t img[1 << 20];
    if (kn_relocate(file, &in, img, base, sizeof img, ex, nex, &err)) { printf("kn_relocate refused: %s\n", err); return 1; }
    printf("kn_relocate ok at %#x\n", base);
    if (out) { FILE *o = fopen(out, "wb"); if (!o) { perror(out); return 2; } fwrite(img, 1, in.load_size, o); fclose(o); }
    return 0;
}
