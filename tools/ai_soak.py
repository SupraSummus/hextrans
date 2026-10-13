#!/usr/bin/env python3
"""Soak the computer players: `-ai_soak` on a few seeds, fail on any abort.

Usage:
    tools/ai_soak.py                         # seeds 1 2 3 to 1933.1 on 128x128
    tools/ai_soak.py 4 5 --until 1931.1      # other seeds, shorter run
    tools/ai_soak.py --size 192 --exe build/simutrans/simutrans 7
    tools/ai_soak.py --shape 48,40,32 --until 1931.1  # a W x H x D hexagon
    tools/ai_soak.py --policy tools/ai_policies/pak64-goods.policy 2  # a goods AI policy
    tools/ai_soak.py --passenger-policy tools/ai_policies/explore.policy 2

Each seed is one engine run (see AGENTS.md, `-ai_soak`): a generated map with
cities and industry (square, or with --shape the W x H x D hexagon of
documentation/map-shape.md), one goods and one passenger AI, stepped flat out
to the given game month.  A run passes if the engine exits cleanly and its last
status line is the requested month; an assert, a fatal error or a sanitizer
hit (all SIGABRT) or a crash fails it, and the tail of its log is printed.
The busy phase of the AIs is the first few game years; both go bankrupt soon
after, so a much longer horizon mostly soaks an idle world.

The engine is --exe, else the first of build-headless/ and build/ (cmake) or ./sim
(CI's make build) that exists, as in tools/heightfield_import_test.py.
pak64 is fetched into simutrans/pak on first use, as tools/test.py does.
Seeds run in parallel (--jobs, default one per core), each with its own user dir
($XDG_DATA_HOME/simutrans under build/ai-soak-data), so runs leave ~/simutrans alone.
"""
import argparse
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
USER_DATA = ROOT / "build" / "ai-soak-data"
LOG_DIR = ROOT / "build" / "ai-soak-logs"
STATUS = re.compile(r"^ai_soak: (\d{4})\.(\d{2}) convoys=(\d+) halts=(\d+) cities=(\d+) ais=(\d+) rng=(\w+)$")


def find_exe(arg):
    candidates = [Path(arg)] if arg else [ROOT / "build-headless" / "simutrans" / "simutrans",
                                         ROOT / "build" / "simutrans" / "simutrans",
                                         ROOT / "sim"]
    for exe in candidates:
        if exe.is_file():
            return exe.resolve()
    sys.exit("no engine found (tried %s); build one or pass --exe" % ", ".join(map(str, candidates)))


def ensure_pak():
    pak = ROOT / "simutrans" / "pak"
    if pak.is_dir() and any(pak.iterdir()):
        return
    print("Fetching pak64 ...", flush=True)
    subprocess.check_call(["../tools/get_pak.sh", "pak64"], cwd=ROOT / "simutrans")


def until_month(until):
    """'1933.1' -> '1933.01', how the status line spells the month it stops at."""
    year, _, month = until.partition(".")
    return "%04d.%02d" % (int(year), int(month or 1))


def run_seed(exe, size, seed, until, timeout, env, policies):
    """Runs one seed; returns (passed, report text)."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    env = dict(env, XDG_DATA_HOME=str(USER_DATA / str(seed)))
    Path(env["XDG_DATA_HOME"]).mkdir(parents=True, exist_ok=True)
    log = LOG_DIR / ("soak_%s_%d.log" % (size.replace(",", "x"), seed))
    cmd = [str(exe), "-use_workdir", "-objects", "pak", "-nosound", "-nomidi", "-debug", "2",
           "-ai_soak", size, str(seed), "-until", until]
    for option, policy in policies:
        cmd += [option, str(Path(policy).resolve())]
    start = time.monotonic()
    with open(log, "w") as out:
        try:
            rc = subprocess.call(cmd, cwd=ROOT / "simutrans", env=env, stdout=out,
                                 stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired:
            rc = "timeout after %ds" % timeout
    secs = time.monotonic() - start

    last = None
    for line in log.read_text(errors="replace").splitlines():
        m = STATUS.match(line)
        if m:
            last = m
    reached = "%s.%s" % (last.group(1), last.group(2)) if last else "never started"
    ok = rc == 0 and reached == until_month(until)
    detail = "reached %s" % reached
    if last:
        detail += " convoys=%s halts=%s ais=%s" % (last.group(3), last.group(4), last.group(6))
    report = "seed %d: %s, %s, %.0fs" % (seed, "ok" if ok else "FAILED (exit %s)" % rc, detail, secs)
    if not ok:
        tail = log.read_text(errors="replace").splitlines()[-30:]
        report += "\n--- tail of %s\n%s\n---" % (log, "\n".join(tail))
    return ok, report


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("seeds", nargs="*", type=int, default=[1, 2, 3])
    ap.add_argument("--size", type=int, default=128, help="map size (default 128)")
    ap.add_argument("--shape", metavar="W,H,D",
                    help="W x H x D hexagon instead of a SIZE x SIZE map")
    ap.add_argument("--until", default="1933.1", help="YEAR.MONTH to stop at (default 1933.1)")
    ap.add_argument("--policy", help="goods AI policy file (-ai_policy)")
    ap.add_argument("--passenger-policy", help="passenger AI policy file (-ai_policy_passenger)")
    ap.add_argument("--exe", help="engine binary")
    ap.add_argument("--timeout", type=int, default=900, help="seconds per seed (default 900)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1,
                    help="seeds to run at once (default: cores)")
    args = ap.parse_args()

    exe = find_exe(args.exe)
    ensure_pak()
    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("ASAN_OPTIONS", "print_stacktrace=1 abort_on_error=1 detect_leaks=0")
    env.setdefault("UBSAN_OPTIONS", "print_stacktrace=1 abort_on_error=1")

    size = args.shape or str(args.size)
    if args.shape and not re.fullmatch(r"\d+,\d+,\d+", args.shape):
        sys.exit("--shape wants W,H,D, got %s" % args.shape)
    policies = [(option, policy) for option, policy in
                (("-ai_policy", args.policy), ("-ai_policy_passenger", args.passenger_policy)) if policy]
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = [pool.submit(run_seed, exe, size, seed, args.until, args.timeout, env, policies)
                   for seed in args.seeds]
        results = []
        for future in futures:  # in seed order
            ok, report = future.result()
            print(report, flush=True)
            results.append(ok)
    failed = [seed for seed, ok in zip(args.seeds, results) if not ok]
    if failed:
        sys.exit("ai_soak FAILED for seeds %s" % ", ".join(map(str, failed)))
    print("ai_soak: %d seed(s) ok" % len(results))


if __name__ == "__main__":
    main()
