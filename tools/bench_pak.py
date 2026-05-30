#!/usr/bin/env python3
"""Perf-test rig for pakset loading.

Builds the `bench_pak` binary (the descriptor-read benchmark in
`src/bench/`), fetches a few paksets of different tile sizes, and times
loading each one.

Usage:
    tools/bench_pak.py                 # pak64, pak128, pak192.comic
    tools/bench_pak.py pak64 pak128    # only the named sets
    tools/bench_pak.py --iterations 9  # more passes per pakset
    tools/bench_pak.py --json          # machine-readable output
    tools/bench_pak.py --dir PATH      # bench an arbitrary pakset dir too

Paksets are downloaded once (~5-40 MB each) into build-bench/paks/ and
reused.  The headless bench build lives in build-bench/ and is configured
with -DSIMUTRANS_BACKEND=none -DSIMUTRANS_BUILD_BENCH=ON.

Scope: this measures the per-file decode + node-tree build + image
registration phase, the part that scales with tile size.  It does not run
xref resolution / checksum (see src/bench/bench_pak.cc).
"""
import argparse
import os
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-bench"
PAKS = BUILD / "paks"
BIN = BUILD / "src" / "bench" / "bench_pak"

# name -> download URL.  Ordered small to large tile size; pak192.comic is
# the "something bigger" set (192px tiles, larger image nodes).  URLs track
# tools/get_pak.sh.
PAKSETS = {
    "pak64": "http://downloads.sourceforge.net/project/simutrans/pak64/124-4/simupak64-124-4.zip",
    "pak128": "http://downloads.sourceforge.net/project/simutrans/pak128/pak128%20for%20ST%20124.4up%20%282.10.1%29/simupak128-2-10-1-for124-4up.zip",
    "pak192.comic": "http://downloads.sourceforge.net/project/simutrans/pak192.comic/pak192.comic%20V0.7.2/pak192-comic.zip",
}


def configure_and_build():
    """Configure the headless bench build dir (idempotent) and build bench_pak."""
    if not (BUILD / "CMakeCache.txt").exists():
        print(f"Configuring {BUILD.name}/ ...")
        subprocess.check_call(
            ["cmake", "-S", str(ROOT), "-B", str(BUILD),
             "-DSIMUTRANS_BACKEND=none", "-DSIMUTRANS_BUILD_BENCH=ON",
             "-DCMAKE_BUILD_TYPE=Release"],
        )
    print("Building bench_pak ...")
    nproc = os.cpu_count() or 1
    subprocess.check_call(
        ["cmake", "--build", str(BUILD), "--target", "bench_pak", "-j", str(nproc)],
    )


def find_pakset_dir(root: Path):
    """Return the directory under `root` that holds the pakset.

    A real pakset always ships ground.Outside.pak at its top level, so the
    directory containing it is the one to point the benchmark at — robust to
    however the archive nests things (simutrans/<name>/...)."""
    for marker in root.rglob("ground.Outside.pak"):
        return marker.parent
    return None


def ensure_pakset(name: str):
    """Download + extract `name` into PAKS if missing; return its pak directory."""
    dest = PAKS / name
    if dest.is_dir():
        found = find_pakset_dir(dest)
        if found is not None:
            return found

    url = PAKSETS.get(name)
    if url is None:
        sys.exit(f"bench_pak: unknown pakset '{name}'. Known: {', '.join(PAKSETS)}")

    dest.mkdir(parents=True, exist_ok=True)
    zip_path = dest / "pakset.zip"
    print(f"Fetching {name} ...")
    urllib.request.urlretrieve(url, zip_path)
    print(f"Extracting {name} ...")
    with zipfile.ZipFile(zip_path) as zf:
        zf.extractall(dest)
    zip_path.unlink()

    found = find_pakset_dir(dest)
    if found is None:
        sys.exit(f"bench_pak: extracted {name} but found no ground.Outside.pak under {dest}")
    return found


def main():
    ap = argparse.ArgumentParser(description="Perf-test pakset loading.")
    ap.add_argument("paksets", nargs="*",
                    help=f"named paksets to bench (default: all of {', '.join(PAKSETS)})")
    ap.add_argument("--iterations", type=int, default=5,
                    help="parse passes per pakset (default 5)")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--dir", action="append", default=[],
                    help="also bench an arbitrary pakset directory (repeatable)")
    ap.add_argument("--no-build", action="store_true",
                    help="skip configure/build, use the existing binary")
    args = ap.parse_args()

    if not args.no_build:
        configure_and_build()
    if not BIN.exists():
        sys.exit(f"bench_pak: binary not found at {BIN} (run without --no-build)")

    names = args.paksets or list(PAKSETS)
    dirs = [str(ensure_pakset(n)) for n in names]
    dirs += list(args.dir)

    cmd = [str(BIN), "--iterations", str(args.iterations)]
    if args.json:
        cmd.append("--json")
    cmd += dirs
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
