#!/usr/bin/env python3
"""Climate shares of generated maps, for before/after comparisons.

Drives the engine's -mapstats test hook (simmain.cc): each run
generates a SIZE x SIZE map from a seed with the normal terrain, lakes
and rivers but no cities or industry, and prints how many tiles got
each climate.  This script runs a grid of sizes, seeds and climate
generators and prints the shares as percentages of all tiles, plus the
mean over the seeds.

Compare two engine builds on the same maps by running it once per
binary:

    tools/mapgen_metrics.py --binary /path/to/simutrans-before
    tools/mapgen_metrics.py

The map number seeds the terrain noise and the generator behind the
climate passes, so the same seed gives the same terrain in both builds.
A change that draws a different number of random values (a different
climate patch shape, say) still moves the later stream, so compare
means over several seeds rather than single maps.

Needs a built simutrans (build-headless/, build/ or ./sim) and a
pakset in simutrans/pak.  A headless build is fastest:
    cmake -B build-headless -DSIMUTRANS_BACKEND=none -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-headless --target simutrans -j $(nproc)
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CLIMATES = ("water", "desert", "tropic", "mediterran", "temperate",
            "tundra", "rocky", "arctic")


def find_simutrans():
    for candidate in (ROOT / "build-headless" / "simutrans" / "simutrans",
                      ROOT / "build" / "simutrans" / "simutrans",
                      ROOT / "sim"):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    sys.exit("no simutrans binary found; build with cmake or autoconf first, "
             "or pass --binary")


def parse_mapstats(output):
    """{climate name: tile count} from the "mapstats: climate NAME N" lines."""
    counts = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[0] == "mapstats:" and parts[1] == "climate":
            counts[parts[2]] = int(parts[3])
    return counts


def run_mapstats(binary, size, seed, generator, timeout=600):
    """Runs the hook; returns the climate counts, or None when it failed."""
    tmp = Path(tempfile.mkdtemp(prefix="mapgen-metrics-"))
    try:
        (tmp / "simuconf.tab").write_text(
            "frames_per_second = 100\nfast_forward_frames_per_second = 100\nautosave = 0\n")
        env = os.environ.copy()
        env.setdefault("SDL_VIDEODRIVER", "dummy")
        log = tmp / "out.log"
        with open(log, "w") as out:
            proc = subprocess.Popen(
                [str(binary),
                 "-set_basedir", str(ROOT / "simutrans"), "-set_userdir", str(tmp),
                 "-objects", "pak", "-lang", "en", "-nomidi", "-nosound", "-mute",
                 "-debug", "2", "-mapstats", str(size), str(seed), generator],
                env=env, stdout=out, stderr=subprocess.STDOUT, cwd=ROOT / "simutrans")
            # The hook quits on its own once the map is saved; a teardown
            # that hangs is not what this measures, so cut it off.
            try:
                proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        counts = parse_mapstats(log.read_text(errors="replace"))
        return counts or None
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def shares(counts):
    total = sum(counts.values())
    return [100.0 * counts.get(name, 0) / total for name in CLIMATES]


def print_row(label, values):
    print("%-22s" % label + "".join("%11.1f" % v for v in values))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--binary", type=Path, help="engine binary (default: the local build)")
    ap.add_argument("--sizes", default="128,256", help="map sizes, comma separated")
    ap.add_argument("--seeds", default="1,2,3,4,5,6", help="map numbers, comma separated")
    ap.add_argument("--generators", default="humidity,height",
                    help="climate generators: humidity, height")
    args = ap.parse_args()

    binary = args.binary or find_simutrans()
    sizes = [int(s) for s in args.sizes.split(",")]
    seeds = [int(s) for s in args.seeds.split(",")]
    generators = args.generators.split(",")

    print("binary: %s" % binary)
    print("climate share of all tiles, %")
    print("%-22s" % "" + "".join("%11s" % name for name in CLIMATES))
    failed = 0
    for generator in generators:
        for size in sizes:
            rows = []
            for seed in seeds:
                counts = run_mapstats(binary, size, seed, generator)
                if counts is None:
                    print("%-22s no mapstats output" % ("%s %d #%d" % (generator, size, seed)))
                    failed += 1
                    continue
                row = shares(counts)
                rows.append(row)
                print_row("%s %d #%d" % (generator, size, seed), row)
            if rows:
                print_row("%s %d mean" % (generator, size),
                          [sum(col) / len(rows) for col in zip(*rows)])
            print()
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
