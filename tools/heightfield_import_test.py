#!/usr/bin/env python3
"""Imports a relief image as a new map and checks the tile heights.

Drives the engine's -heightfield test hook (simmain.cc), which builds
the map the way the new-world dialog's "Load relief" does, prints the
base height of every tile and saves.  The expected heights come from a
model written here from the geometry alone: a pixel (i, j) is tile
(q = i, r = j), the corners of a tile sit at the axial offsets below,
heights are interpolated bilinearly between pixel centres, rounded, and
raised by one.

Run from anywhere:  python3 tools/heightfield_import_test.py
Needs a built simutrans (build/, build-headless/ or ./sim) and a pakset
in simutrans/pak.  Before the hex importer the same run ended in the
dbg->fatal "heightfield import is not hex-aware".
"""
import math
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Axial offset of corner c (E, SE, SW, W, NW, NE) from its tile centre,
# in thirds of a tile.
CORNER_DQ3 = (2, 1, -1, -2, -1, 1)
CORNER_DR3 = (-1, 1, 2, 1, -1, -2)

GROUNDWATER = -2             # settings_t default
MIN_HEIGHT, MAX_HEIGHT = -12, 32   # settings_t world limits


def find_simutrans():
    for candidate in (ROOT / "build-headless" / "simutrans" / "simutrans",
                      ROOT / "build" / "simutrans" / "simutrans",
                      ROOT / "sim"):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    sys.exit("no simutrans binary found; build with cmake or autoconf first")


def write_ppm(path, width, height, grey):
    """P6 image whose pixel (x, y) has grey level grey(x, y)."""
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (width, height))
        for y in range(height):
            for x in range(width):
                v = grey(x, y)
                f.write(bytes((v, v, v)))


def pixel_height(p):
    """height_map_loader_t::rgb_to_height in HEIGHT_CONV_LINEAR mode."""
    return MIN_HEIGHT + (6 * p * (MAX_HEIGHT - MIN_HEIGHT)) // 0x5FA


def expected_tiles(width, height, grey):
    """Expected `z` per tile, "W" for water, as a list of rows."""
    field = [[pixel_height(grey(x, y)) for x in range(width)] for y in range(height)]

    def sample(fq, fr):
        fq = min(max(fq, 0.0), width - 1)
        fr = min(max(fr, 0.0), height - 1)
        i0, j0 = int(fq), int(fr)
        i1, j1 = min(i0 + 1, width - 1), min(j0 + 1, height - 1)
        tq, tr = fq - i0, fr - j0
        top = field[j0][i0] * (1 - tq) + field[j0][i1] * tq
        bottom = field[j1][i0] * (1 - tq) + field[j1][i1] * tq
        return top * (1 - tr) + bottom * tr

    rows = []
    for r in range(height):
        row = []
        for q in range(width):
            corners = [math.floor(sample(q + CORNER_DQ3[c] / 3, r + CORNER_DR3[c] / 3) + 0.5) + 1
                       for c in range(6)]
            if max(corners) <= GROUNDWATER:
                row.append("W")
            else:
                # a tile reaching below the water is raised to it
                row.append(max(min(corners), GROUNDWATER))
        rows.append(row)
    return rows


def import_relief(width, height, grey, timeout=120):
    """Runs the hook; returns (rows of "z"/"W" strings, saved map exists)."""
    tmp = Path(tempfile.mkdtemp(prefix="heightfield-import-"))
    try:
        image = tmp / "relief.ppm"
        write_ppm(image, width, height, grey)
        userdir = tmp / "user"
        userdir.mkdir()
        (userdir / "simuconf.tab").write_text(
            "frames_per_second = 100\nfast_forward_frames_per_second = 100\nautosave = 0\n")
        env = os.environ.copy()
        env.setdefault("SDL_VIDEODRIVER", "dummy")
        proc = subprocess.Popen(
            [str(find_simutrans()),
             "-set_basedir", str(ROOT / "simutrans"), "-set_userdir", str(userdir),
             "-objects", "pak", "-lang", "en", "-nomidi", "-nosound", "-mute",
             "-debug", "2", "-heightfield", str(image)],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        # The hook quits on its own once the map is saved; a teardown
        # that hangs is not what this checks, so cut it off.
        try:
            out, _ = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
        rows = {}
        for line in out.splitlines():
            if line.startswith("heightfield: "):
                number, cells = line[len("heightfield: "):].split(" ", 1)
                rows[int(number)] = cells.split(",")
        saved = (userdir / "autosave-pak.sve").is_file()
        if not rows:
            sys.stderr.write(out[-3000:])
        return [rows[r] for r in sorted(rows)], saved
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


class HeightfieldImport(unittest.TestCase):
    def check(self, width, height, grey):
        tiles, saved = import_relief(width, height, grey)
        self.assertEqual(len(tiles), height, "one dump row per tile row")
        for row in tiles:
            self.assertEqual(len(row), width)
        self.assertTrue(saved, "the imported map was saved")

        want = expected_tiles(width, height, grey)
        wrong, beaches = [], 0
        for r in range(height):
            for q in range(width):
                got = tiles[r][q]
                if got == str(want[r][q]):
                    continue
                if want[r][q] == "W" and got == str(GROUNDWATER):
                    # create_beaches turns a water tile with little water
                    # around it, as at the map edge, into flat land at the
                    # water level; it is not part of the import.
                    beaches += 1
                    continue
                wrong.append((q, r, got, want[r][q]))
        self.assertEqual(wrong[:10], [], "%d tiles differ: (q, r, got, want)" % len(wrong))
        flat = [c for row in want for c in row]
        water = flat.count("W")
        self.assertGreater(water, 0, "the image reaches below the water level")
        self.assertLess(water, len(flat), "the image has land")
        self.assertLessEqual(beaches * 10, water, "few beaches")

    def test_ramp_square_image(self):
        # 2x + y: steeper along the image's x axis than its y axis, so a
        # transposed, sheared or mirrored import comes out different.
        self.check(64, 64, lambda x, y: min(2 * x + y, 255))

    def test_non_square_image(self):
        # width != height: the row stride is the image width.
        self.check(40, 56, lambda x, y: min(3 * x + 2 * (55 - y), 255))


if __name__ == "__main__":
    unittest.main()
