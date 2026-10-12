#!/usr/bin/env python3
"""Imports a relief image as a new map and checks the tile heights.

Drives the engine's -heightfield test hook (simmain.cc), which builds
the map the way the new-world dialog's "Load relief" does, prints the
base height of every tile and saves.  The expected heights come from a
model written here from the geometry alone, with exact fractions so
that ties round the way the engine rounds them (up).

The image is in the frame of documentation/map-shape.md: the physical
plane in offset coordinates.  For the W x H x D hexagon, t = D - 1,
W' = W + t and H' = H + t, tile (q, r) sits at column i = q, row
j = r + (q - t) / 2 of an image of W' columns by H + W' // 2 rows,
the columns with q - t odd half a row lower.  So the image size fixes
W = columns - D + 1 and H = rows - columns // 2.  A tile corner c
(E, SE, SW, W, NW, NE) sits at the offsets below, heights are
interpolated bilinearly between pixel centres, rounded half up, and
raised by one.  Tiles off the hexagon (the cut corners and, at D = 1,
the two triangles of the parallelogram) are dumped as "-".

Run from anywhere:  python3 tools/heightfield_import_test.py
Needs a built simutrans (build/, build-headless/ or ./sim) and a pakset
in simutrans/pak.  Before the hex importer the same run ended in the
dbg->fatal "heightfield import is not hex-aware".
"""
import math
from fractions import Fraction
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Offset of corner c (E, SE, SW, W, NW, NE) from its tile's position in
# the image frame: columns in thirds, rows in halves of a pixel.
CORNER_DX3 = (2, 1, -1, -2, -1, 1)
CORNER_DY2 = (0, 1, 1, 0, -1, -1)

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


def on_map(q, r, width, rows_h, d):
    """Whether tile (q, r) is on the hexagon, W' = width, H' = rows_h."""
    t = d - 1
    return (0 <= q < width and 0 <= r < rows_h
            and t <= q + r <= width + rows_h - 2 - t)


def shape_of(width, height, d):
    """(W, H, W', H') of the map an image of width x height pixels gives."""
    w = width - d + 1
    h = height - width // 2
    return w, h, w + d - 1, h + d - 1


def expected_tiles(width, height, grey, d):
    """Expected `z` per tile, "W" for water, "-" off the map, as rows,
    and the number of tiles that were water before the beaches."""
    t = d - 1
    w, h, wp, hp = shape_of(width, height, d)
    field = [[pixel_height(grey(x, y)) for x in range(width)] for y in range(height)]

    def sample(fx, fy):
        fx = min(max(fx, Fraction(0)), Fraction(width - 1))
        fy = min(max(fy, Fraction(0)), Fraction(height - 1))
        i0, j0 = int(fx), int(fy)
        i1, j1 = min(i0 + 1, width - 1), min(j0 + 1, height - 1)
        tx, ty = fx - i0, fy - j0
        top = field[j0][i0] * (1 - tx) + field[j0][i1] * tx
        bottom = field[j1][i0] * (1 - tx) + field[j1][i1] * tx
        return top * (1 - ty) + bottom * ty

    rows = []
    for r in range(hp):
        row = []
        for q in range(wp):
            if not on_map(q, r, wp, hp, d):
                row.append("-")
                continue
            corners = []
            for c in range(6):
                x = q + Fraction(CORNER_DX3[c], 3)
                y = r + Fraction(q - t, 2) + Fraction(CORNER_DY2[c], 2)
                # round half up, then raised by one
                corners.append(math.floor(sample(x, y) + Fraction(1, 2)) + 1)
            if max(corners) <= GROUNDWATER:
                row.append("W")
            else:
                # a tile reaching below the water is raised to it
                row.append(max(min(corners), GROUNDWATER))
        rows.append(row)
    water = sum(row.count("W") for row in rows)
    return apply_beaches(rows, d), water


# koord::neighbours: SE, S, SW, NW, N, NE
NEIGHBOURS = ((1, 0), (0, 1), (-1, 1), (-1, 0), (0, -1), (1, -1))


def apply_beaches(rows, d):
    """karte_t::create_beaches' first pass on the expected tiles.

    A water tile with less than three quarters of its on-map neighbours
    water (opposite neighbours count twice) becomes flat land at the water
    level.  The pass walks the rows in order, so a new beach counts as
    land for the tiles after it.
    """
    hp, wp = len(rows), len(rows[0])
    for r in range(hp):
        for q in range(wp):
            if rows[r][q] != "W":
                continue
            water = [False] * 6
            total = 0
            count = 0
            for i, (dq, dr) in enumerate(NEIGHBOURS):
                nq, nr = q + dq, r + dr
                if on_map(nq, nr, wp, hp, d):
                    total += 1
                    if rows[nr][nq] == "W":
                        water[i] = True
                        count += 1
            for i in range(6):
                if water[i] and (water[(i + 2) % 6] or water[(i + 3) % 6] or water[(i + 4) % 6]):
                    count += 1
            if count < (3 * total) // 4:
                rows[r][q] = GROUNDWATER
    return rows


def import_relief(width, height, grey, d, timeout=120):
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
             "-debug", "2", "-heightfield", str(image), str(d)],
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
    def check(self, width, height, grey, d):
        """Imports a width x height image into the map of depth d."""
        w, h, wp, hp = shape_of(width, height, d)
        self.assertGreaterEqual(min(w, h), 1)
        tiles, saved = import_relief(width, height, grey, d)
        self.assertEqual(len(tiles), hp, "one dump row per row of the bounding rhombus")
        for row in tiles:
            self.assertEqual(len(row), wp)
        self.assertTrue(saved, "the imported map was saved")

        want, water = expected_tiles(width, height, grey, d)
        wrong = []
        for r in range(hp):
            for q in range(wp):
                if tiles[r][q] != str(want[r][q]):
                    wrong.append((q, r, tiles[r][q], want[r][q]))
        self.assertEqual(wrong[:10], [], "%d tiles differ: (q, r, got, want)" % len(wrong))
        flat = [c for row in want for c in row if c != "-"]
        # W*H + H*D + D*W - W - H - D + 1 tiles on the map
        self.assertEqual(len(flat), w * h + h * d + d * w - w - h - d + 1)
        self.assertGreater(water, 0, "the image reaches below the water level")
        self.assertLess(water, len(flat), "the image has land")
        return want

    def test_ramp_square_image(self):
        # 2x + y: steeper along the image's x axis than its y axis, so a
        # transposed, sheared or mirrored import comes out different.
        # 64 x 64 pixels are a 64 x 32 rhombus.
        self.check(64, 64, lambda x, y: min(2 * x + y, 255), 1)

    def test_non_square_image(self):
        # width != height: the row stride is the image width.
        self.check(40, 56, lambda x, y: min(3 * x + 2 * (55 - y), 255), 1)

    def test_hexagon_9x7x5(self):
        # The W x H x D = 9 x 7 x 5 hexagon: a 13 x 13 image, 11 rows of
        # 13 columns on the map with the cut corners dumped as "-".  A
        # ramp over the image gives the six corner tiles different heights,
        # so a shifted, mirrored or transposed frame shows in them.
        want = self.check(13, 13, lambda x, y: 40 + 5 * x + 3 * y, 5)
        w, h, wp, hp = shape_of(13, 13, 5)
        self.assertEqual((w, h, wp, hp), (9, 7, 13, 11))
        t = 4
        corners = [(t, 0), (wp - 1, 0), (wp - 1, hp - 1 - t),
                   (wp - 1 - t, hp - 1), (0, hp - 1), (0, t)]
        for q, r in corners:
            self.assertTrue(on_map(q, r, wp, hp, 5))
        self.assertGreaterEqual(len({want[r][q] for q, r in corners}), 5)
        # the cut corners and nothing else are off the map
        self.assertEqual(want[0][0], "-")
        self.assertEqual(want[hp - 1][wp - 1], "-")

    def test_regular_hexagon(self):
        # A regular hexagon has a square image: 15 x 15 for n = 8.
        self.check(15, 15, lambda x, y: 40 + 5 * x + 3 * y, 8)


if __name__ == "__main__":
    unittest.main()
