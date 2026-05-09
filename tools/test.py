#!/usr/bin/env python3
"""Run the Simutrans automated-test scenario, optionally filtered to a subset.

Usage:
    tools/test.py                # run every test in tests/all_tests.nut
    tools/test.py halt           # run tests whose name contains "halt"
    tools/test.py halt terraform # OR-match: name contains "halt" OR "terraform"

First run downloads pak64 (~30 MB) into simutrans/pak/; subsequent runs
reuse it. Other setup (sim symlink, simuconf, scenario symlink) is also
idempotent — anything already in place is left alone.

Requires the cmake build dir at build/ (the session-start hook configures it).
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()
FILTER = ROOT / "tests" / "filter.nut"


def ensure_pak():
    pak = ROOT / "simutrans" / "pak"
    if pak.is_dir() and any(pak.iterdir()):
        return
    print("Fetching pak64 ...")
    subprocess.check_call(["../tools/get_pak.sh", "pak64"], cwd=ROOT / "simutrans")


def ensure_simuconf():
    cfg = HOME / "simutrans" / "simuconf.tab"
    if cfg.exists():
        return
    cfg.parent.mkdir(parents=True, exist_ok=True)
    cfg.write_text("frames_per_second = 100\nfast_forward_frames_per_second = 100\n")


def ensure_scenario_link():
    link = HOME / "simutrans" / "addons" / "pak" / "scenario" / "automated-tests"
    target = ROOT / "tests"
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.is_symlink() and Path(os.readlink(link)) == target:
        return
    if link.exists() or link.is_symlink():
        link.unlink()
    link.symlink_to(target, target_is_directory=True)


def ensure_sim_symlink():
    sim = ROOT / "sim"
    target = Path("build/simutrans/simutrans")
    if sim.is_symlink() and Path(os.readlink(sim)) == target:
        return
    if sim.exists() or sim.is_symlink():
        sim.unlink()
    sim.symlink_to(target)


def build():
    nproc = os.cpu_count() or 1
    subprocess.check_call(
        ["cmake", "--build", "build", "--target", "simutrans", "makeobj",
         "-j", str(nproc)],
        cwd=ROOT,
    )


def ensure_test_pak():
    """Bake `tests/test-pak/` into `~/simutrans/addons/pak/test-pak.pak`.

    Carries descriptors with engine flags pak64 itself does not opt
    into (e.g. `has_double_slopes=1`), so scenario tests can exercise
    those code paths without waiting on a pakset commit. CI wires the
    same bake / link via .github/workflows/run-tests.yml.
    """
    pak_out = ROOT / "tests" / "test-pak" / "test-pak.pak"
    subprocess.check_call(
        [str(ROOT / "build" / "src" / "makeobj" / "makeobj"),
         "QUIET", "PAK", str(pak_out), str(pak_out.parent) + "/"],
    )
    addons_link = HOME / "simutrans" / "addons" / "pak" / "test-pak.pak"
    addons_link.parent.mkdir(parents=True, exist_ok=True)
    if (addons_link.is_symlink()
            and Path(os.readlink(addons_link)) == pak_out):
        return
    if addons_link.exists() or addons_link.is_symlink():
        addons_link.unlink()
    addons_link.symlink_to(pak_out)


def active_test_names():
    """Names of uncommented test functions listed in tests/all_tests.nut."""
    text = (ROOT / "tests" / "all_tests.nut").read_text()
    body = text.split("all_tests <- [", 1)[1].rsplit("]", 1)[0]
    return re.findall(r"^\s*([A-Za-z_]\w*)\s*,", body, re.MULTILINE)


def write_filter(patterns):
    body = "test_filter_patterns <- [" + ", ".join(json.dumps(p) for p in patterns) + "]\n"
    FILTER.write_text(body)


def main(argv):
    patterns = argv[1:]
    if patterns:
        matches = [n for n in active_test_names() if any(p in n for p in patterns)]
        if not matches:
            sys.exit(f"no active tests match {patterns}")
        print(f"{len(matches)} test(s) match: {', '.join(matches)}")
    ensure_pak()
    ensure_simuconf()
    ensure_scenario_link()
    ensure_sim_symlink()
    build()
    ensure_test_pak()
    write_filter(patterns)
    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("ASAN_OPTIONS", "print_stacktrace=1 abort_on_error=1 detect_leaks=0")
    env.setdefault("UBSAN_OPTIONS", "print_stacktrace=1 abort_on_error=1")
    try:
        rc = subprocess.call(["tools/run-automated-tests.sh"], cwd=ROOT, env=env)
    finally:
        write_filter([])
    if rc != 0:
        print("FAILED. tail simutrans/output.log for details")
    sys.exit(rc)


if __name__ == "__main__":
    main(sys.argv)
