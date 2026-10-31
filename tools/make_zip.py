#!/usr/bin/env python3
"""Pack the download zip from release/: release/hollow_saint-<version>.zip.

The zip holds the prebuilt files only (nothing to build): hollow_saint.knl (kennel), mods/hollow_saint.opmod (Opus 2),
the presets as synth/hollow saint/, an install note, SHA256SUMS.txt and the licence. Fixed timestamps and order, so
the same files give the same zip. Run it after tools/release.sh and tools/build_opmod.sh have refreshed release/.

Copyright (C) 2026 the hollow_saint authors. GPL-3.0-or-later with the OP-1 firmware permission.
"""
import hashlib
import json
import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = json.load(open(os.path.join(ROOT, "plugin/hollow_saint/plugin.json")))["version"]
TOP = "hollow_saint-%s" % VERSION
STAMP = (2026, 1, 1, 0, 0, 0)

INSTALL = """Cathedral of the Hollow Saint %(v)s: a haunted hurdy-gurdy for the original OP-1
https://github.com/SammiBabe666/Project_Hollow_Saint

Nothing to build: everything here is ready to install. Pick the one that matches your OP-1.

hollow_saint.knl            Hundekraft OP-1 Workshop 3.0.x (kennel)
mods/hollow_saint.opmod     Opus 2 (core 2.6 or later)
synth/hollow saint/         the 8 presets (both)
SHA256SUMS.txt              checksums of the two plugin files

HUNDEKRAFT 3.0.x (kennel)
1. The OP-1 must already run Hundekraft 3.0.x (see the Workshop's quick start).
2. In the Workshop, open "Your plugins", add hollow_saint.knl, connect the OP-1 by USB and press "Update OP-1".
   Review, send, then press T1 on the OP-1 to confirm. It installs on the restart.
3. On the OP-1: synth, then Shift + T1, choose "hollow saint".
If the OP-1 does not start after adding a plugin: power-cycle it; it starts once without plugin files, then remove
the plugin in the Workshop.

OPUS 2
1. The OP-1 runs Opus 2 with core 2.6 or later.
2. Opus's menu (hold synth + drum + tape + mixer), T1: switch developer mode on for the first tries.
3. Disk mode (Shift + COM, then T3). Copy hollow_saint.opmod into the disk's "mods" folder.
4. Eject, switch off and on. In the menu, T2 (the mods list), "hollow saint" should say "loaded".
5. synth, Shift + T1, choose "hollow saint".
Don't also load hollow_saint.knl through opus.bridge: both claim the sound "hollow saint".

PRESETS (both)
Disk mode (Shift + COM, then T3). Copy the folder "hollow saint" (inside synth/ here) INTO the OP-1's "synth"
folder, then eject. Copy the folder itself, not this zip's whole synth folder: replacing the OP-1's synth folder
would delete your own patches.

Not yet run on an OP-1: tested on a PC and in a Blackfin simulator only. Please report how it goes.
GPL-3.0-or-later with the OP-1 firmware permission (LICENSE). Source: the repository above.
"""


def main():
    rel = os.path.join(ROOT, "release")
    files = [
        ("hollow_saint.knl", os.path.join(rel, "hollow_saint.knl")),
        ("mods/hollow_saint.opmod", os.path.join(rel, "opus/hollow_saint.opmod")),
    ]
    pdir = os.path.join(rel, "presets/synth/hollow saint")
    for name in sorted(os.listdir(pdir)):
        if name.endswith(".aif"):
            files.append(("synth/hollow saint/" + name, os.path.join(pdir, name)))
    sums = "".join("%s  %s\n" % (hashlib.sha256(open(p, "rb").read()).hexdigest(), a) for a, p in files[:2])
    out = os.path.join(rel, "%s.zip" % TOP)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        def add(arc, data):
            info = zipfile.ZipInfo("%s/%s" % (TOP, arc), STAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
        add("INSTALL.txt", INSTALL % {"v": VERSION})
        for arc, path in files:
            add(arc, open(path, "rb").read())
        add("SHA256SUMS.txt", sums)
        add("LICENSE", open(os.path.join(ROOT, "LICENSE"), "rb").read())
        add("NOTICE", open(os.path.join(ROOT, "NOTICE"), "rb").read())
    print(sums, end="")
    print("%s: %d bytes" % (os.path.relpath(out, ROOT), os.path.getsize(out)))


if __name__ == "__main__":
    main()
