#!/usr/bin/env python3
"""Measure the spatial anisotropy of city growth on the hex grid.

City growth runs in *axial* (q, r) coordinates — the byte-compatible
reinterpretation of the old square (x, y) koord — but "anisotropy" only
means anything in *physical* space, after the flat-top hex basis is
applied.  This tool maps a city's tiles through that basis and reports
how far the footprint departs from circular, in which direction, and
whether the departure respects hex symmetry.

It reports the gyration tensor in BOTH frames on purpose:

  * round in axial but elongated in physical  -> the shear in the
    axial->physical map is the cause; the algorithm produced an
    axial-symmetric blob and the basis distorted it.
  * elongated in axial too                    -> genuine directional
    bias in the growth algorithm itself, on top of any shear.

A single physical-frame number cannot tell those apart; the gap between
the two frames can.

Usage:
    tools/city_anisotropy.py --self-test         # validate the math
    tools/city_anisotropy.py --coords FILE       # analyze a coord dump
    tools/city_anisotropy.py --run               # generate flat map, measure
    tools/city_anisotropy.py --run --single      # one city instead of a grid
    tools/city_anisotropy.py --run --map M.sve   # measure on an existing map

A coord dump is whitespace-separated "q r" pairs, one per line; lines
that do not parse as two integers are ignored, so the raw scenario
stdout can be fed in directly (the dump lines are tagged, see below).

--run generates a large flat empty map (via the -flatmap hook in the
headless none-backend build, cached under ~/.cache/hextrans-aniso),
grid-seeds cities on it, grows each by a citizen delta, waits for the
houses to build, then dumps the footprints.  Flat ground matters: a
small map clips the city against its borders and terrain caps/reshapes
growth, both of which confound the measurement.  Map generation is the
slow step (Debug new-world gen + fast-forward); it is cached per size
under ~/.cache/hextrans-aniso, so only the first run for a given size
pays it.  Build RelWithDebInfo for a markedly faster generator:
    cmake -B build-headless -DSIMUTRANS_BACKEND=none -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build-headless --target simutrans -j $(nproc)
"""
import math
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()

# Flat-top hex axial->physical basis, screen coords (y down).
#   e_q = SE neighbour = (sqrt3/2,  1/2)
#   e_r = S  neighbour = (0,        1  )
# Both unit length, 60 deg apart, so the lattice is locally isotropic;
# any physical anisotropy measured below is injected by the algorithm,
# not by the grid.  See AGENTS.md "Direction naming convention" and
# koord::neighbours[] for the basis vectors.
_SQRT3_2 = math.sqrt(3.0) / 2.0


def axial_to_physical(q, r):
    return (_SQRT3_2 * q, 0.5 * q + r)


def _centroid(points):
    n = len(points)
    sx = sum(p[0] for p in points)
    sy = sum(p[1] for p in points)
    return (sx / n, sy / n)


def _eig2x2_sym(a, b, c):
    """Eigenvalues/vectors of [[a, b], [b, c]], l1 >= l2.

    Returns (l1, l2, angle1) where angle1 is the bearing of the l1
    eigenvector in radians, atan2(y, x).
    """
    tr = a + c
    disc = math.sqrt((a - c) * (a - c) + 4.0 * b * b)
    l1 = (tr + disc) / 2.0
    l2 = (tr - disc) / 2.0
    # eigenvector for l1: (b, l1 - a) unless b ~ 0 (already diagonal)
    if abs(b) > 1e-12:
        vx, vy = b, l1 - a
    elif a >= c:
        vx, vy = 1.0, 0.0
    else:
        vx, vy = 0.0, 1.0
    return l1, l2, math.atan2(vy, vx)


def gyration(points):
    """Second-moment (gyration) tensor of a point cloud about its centroid.

    Returns dict with eigenvalues, aspect ratio sqrt(l1/l2), anisotropy
    index (l1-l2)/(l1+l2) in [0,1], and the principal-axis bearing in
    degrees within [0,180).
    """
    cx, cy = _centroid(points)
    a = b = c = 0.0
    for x, y in points:
        dx, dy = x - cx, y - cy
        a += dx * dx
        b += dx * dy
        c += dy * dy
    n = len(points)
    a, b, c = a / n, b / n, c / n
    l1, l2, ang = _eig2x2_sym(a, b, c)
    aspect = math.sqrt(l1 / l2) if l2 > 1e-12 else float("inf")
    aniso = (l1 - l2) / (l1 + l2) if (l1 + l2) > 1e-12 else 0.0
    bearing = math.degrees(ang) % 180.0
    return {
        "centroid": (cx, cy),
        "l1": l1,
        "l2": l2,
        "aspect": aspect,
        "anisotropy": aniso,
        "bearing_deg": bearing,
    }


def angular_modes(points, center, modes=(2, 4, 6)):
    """m-fold angular density moments about `center`.

    C_m = mean over tiles of exp(i*m*theta); |C_m| in [0,1] measures how
    much the tiles cluster into m angular lobes (0 = uniform in angle).

    m=6 is the benign hex-lattice fingerprint; m=2 (elongation) and m=4
    are not hex symmetries, so nonzero values there are the signature of
    square-grid leakage rather than honest lattice texture.  The phase
    gives the lobe orientation.
    """
    cx, cy = center
    out = {}
    for m in modes:
        sr = si = 0.0
        n = 0
        for x, y in points:
            dx, dy = x - cx, y - cy
            if dx == 0.0 and dy == 0.0:
                continue
            th = math.atan2(dy, dx)
            sr += math.cos(m * th)
            si += math.sin(m * th)
            n += 1
        amp = math.hypot(sr, si) / n if n else 0.0
        phase = (math.degrees(math.atan2(si, sr)) / m) % (360.0 / m)
        out[m] = {"amp": amp, "phase_deg": phase}
    return out


def analyze(coords, center_qr=None):
    """coords: list of integer (q, r) tiles. center_qr: optional townhall."""
    if len(coords) < 3:
        raise ValueError("need at least 3 tiles to measure a shape")
    phys = [axial_to_physical(q, r) for q, r in coords]
    ax_gyr = gyration([(float(q), float(r)) for q, r in coords])
    ph_gyr = gyration(phys)
    if center_qr is not None:
        center = axial_to_physical(*center_qr)
    else:
        center = ph_gyr["centroid"]
    modes = angular_modes(phys, center)
    return {"n": len(coords), "axial": ax_gyr, "physical": ph_gyr, "modes": modes}


def format_report(res):
    ax, ph, modes = res["axial"], res["physical"], res["modes"]
    lines = []
    lines.append("city tiles measured: %d" % res["n"])
    lines.append("")
    lines.append("gyration tensor (aspect = long/short axis, 1.00 = round):")
    lines.append("  axial frame (q,r):     aspect %.3f  anisotropy %.3f  bearing %5.1f deg"
                 % (ax["aspect"], ax["anisotropy"], ax["bearing_deg"]))
    lines.append("  physical frame:        aspect %.3f  anisotropy %.3f  bearing %5.1f deg"
                 % (ph["aspect"], ph["anisotropy"], ph["bearing_deg"]))
    lines.append("")
    lines.append("angular density modes about centre (|C_m| in [0,1]):")
    lines.append("  m=2 elongation  : %.3f  (lobe @ %5.1f deg)   <- square leak if large"
                 % (modes[2]["amp"], modes[2]["phase_deg"]))
    lines.append("  m=4             : %.3f  (lobe @ %5.1f deg)   <- square leak if large"
                 % (modes[4]["amp"], modes[4]["phase_deg"]))
    lines.append("  m=6 hex         : %.3f  (lobe @ %5.1f deg)   <- benign lattice texture"
                 % (modes[6]["amp"], modes[6]["phase_deg"]))
    lines.append("")
    lines.append("interpretation hints:")
    lines.append("  * physical aspect >> axial aspect -> basis shear dominates")
    lines.append("  * axial aspect also large         -> algorithmic directional bias")
    lines.append("  * physical bearing ~60 deg, aspect ~1.73 -> square sampling box")
    lines.append("    (a square axial AABB maps to a 60-deg rhombus, long axis @ 60 deg)")
    return "\n".join(lines)


def parse_coord_dump(text):
    coords = []
    for line in text.splitlines():
        parts = line.split()
        # accept "q r" or "TAG q r ..." — grab the first two integer tokens
        ints = []
        for tok in parts:
            try:
                ints.append(int(tok))
            except ValueError:
                continue
        if len(ints) >= 2:
            coords.append((ints[0], ints[1]))
    return coords


# ---------------------------------------------------------------------------
# self-test: synthetic clouds with known answers
# ---------------------------------------------------------------------------
def _self_test():
    import random
    rng = random.Random(1)
    ok = True

    def check(name, cond):
        nonlocal ok
        print(("  PASS " if cond else "  FAIL ") + name)
        ok = ok and cond

    # 1. Disk in AXIAL space: round in axial, should shear to ~sqrt(3)
    #    aspect in physical, principal axis along the e_q+e_r bisector.
    axial_disk = []
    for _ in range(20000):
        q = rng.uniform(-50, 50)
        r = rng.uniform(-50, 50)
        if q * q + r * r <= 2500:
            axial_disk.append((q, r))
    res = analyze(axial_disk)
    check("axial disk: axial aspect ~1.0",
          abs(res["axial"]["aspect"] - 1.0) < 0.05)
    check("axial disk: physical aspect ~1.73 (sqrt3)",
          abs(res["physical"]["aspect"] - math.sqrt(3.0)) < 0.08)
    check("axial disk: physical bearing ~60 deg",
          abs(res["physical"]["bearing_deg"] - 60.0) < 6.0)

    # 2. Disk in PHYSICAL space: round in physical (the isotropic ideal),
    #    low low-order angular modes.
    phys_disk_q = []
    for _ in range(20000):
        x = rng.uniform(-50, 50)
        y = rng.uniform(-50, 50)
        if x * x + y * y <= 2500:
            # invert the basis to get the (q,r) that lands here
            q = x / _SQRT3_2
            r = y - 0.5 * q
            phys_disk_q.append((q, r))
    res2 = analyze(phys_disk_q)
    check("physical disk: physical aspect ~1.0",
          abs(res2["physical"]["aspect"] - 1.0) < 0.05)
    check("physical disk: m=2 amplitude ~0",
          res2["modes"][2]["amp"] < 0.03)

    print("self-test: " + ("all passed" if ok else "FAILURES"))
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# sim driving: grow a city in a scenario and dump its tiles
# ---------------------------------------------------------------------------
# The growth algorithm lives in the engine, so we drive a headless scenario
# (squirrel) that adds one city, grows it to a target population, then sweeps
# every tile and prints the (q, r) of each city building.  The math above
# then runs on that dump.  Tagged lines look like "ANISO 12 7" / "ANISO 8 8 TH".
_SCENARIO_NAME = "anisotropy-probe"
_BEGIN, _END = "ANISO_BEGIN", "ANISO_END"

_SCENARIO_TMPL = '''//
// Generated by tools/city_anisotropy.py -- city-growth anisotropy probe.
// Not a pass/fail test: founds cities on the loaded map and dumps each
// city building's axial (q, r), tagged with its town-hall coord, for the
// offline analyzer.  City construction runs over game steps, so the dump
// waits a few ticks after founding before sweeping (and sweeps in chunks
// to stay under the scenario instruction budget).
//
map.file = "map.sve"

scenario.short_description = "city anisotropy probe"
scenario.author = "city_anisotropy.py"
scenario.version = "0.1"

function get_rule_text(pl)   {{ return "probe" }}
function get_goal_text(pl)   {{ return "probe" }}
function get_info_text(pl)   {{ return "probe" }}
function get_result_text(pl) {{ return "probe" }}
function is_tool_allowed(pl, tool_id, wt, name) {{ return true }}

WAIT <- 10
tick <- 0
gx <- 0
sz <- null
began <- false

function start()
{{
{seeds}
}}

function resume_game() {{ start() }}

// wait for founding/growth to materialise, then dump in column chunks
function is_scenario_completed(pl)
{{
	if (sz == null) sz = world.get_size()
	tick++
	if (tick < WAIT) return 5
	if (!began) {{ print("{begin}"); began = true }}
	local cols = 0
	while (cols < 8 && gx < sz.x) {{
		for (local y = 0; y < sz.y; y++) {{
			local b = tile_x(gx, y, 0).find_object(mo_building)
			if (b == null) continue
			local c = b.get_city()
			if (c == null) continue
			local cp = c.get_pos()
			print("ANISO " + gx + " " + y + " " + cp.x + " " + cp.y + (b.is_townhall() ? " TH" : ""))
		}}
		gx++
		cols++
	}}
	if (gx >= sz.x) {{ print("{end}"); return 100 }}
	return 50
}}
'''


def _ensure_harness():
    """Mirror tools/test.py setup: pak64, simuconf, sim symlink."""
    pak = ROOT / "simutrans" / "pak"
    if not (pak.is_dir() and any(pak.iterdir())):
        subprocess.check_call(["../tools/get_pak.sh", "pak64"], cwd=ROOT / "simutrans")
    cfg = HOME / "simutrans" / "simuconf.tab"
    if not cfg.exists():
        cfg.parent.mkdir(parents=True, exist_ok=True)
        cfg.write_text("frames_per_second = 100\nfast_forward_frames_per_second = 100\n")
    sim = ROOT / "sim"
    target = Path("build/simutrans/simutrans")
    if not (sim.is_symlink() and Path(os.readlink(sim)) == target):
        if sim.exists() or sim.is_symlink():
            sim.unlink()
        sim.symlink_to(target)
    if not (ROOT / target).exists():
        raise SystemExit("build missing: cmake --build build --target simutrans")


_HEADLESS = ROOT / "build-headless" / "simutrans" / "simutrans"


def _ensure_flat_map(size):
    """Generate (and cache) a flat, empty size x size .sve via the -flatmap
    hook in the headless (none-backend) build, which saves on -until quit."""
    cache = HOME / ".cache" / "hextrans-aniso"
    cache.mkdir(parents=True, exist_ok=True)
    out = cache / ("flat-%d.sve" % size)
    if out.exists():
        return out
    if not _HEADLESS.exists():
        raise SystemExit(
            "headless build needed to generate a flat map:\n"
            "  cmake -B build-headless -DSIMUTRANS_BACKEND=none -DCMAKE_BUILD_TYPE=Debug\n"
            "  cmake --build build-headless --target simutrans -j $(nproc)")
    gen = cache / "gen"
    if gen.exists():
        shutil.rmtree(gen)
    gen.mkdir(parents=True)
    (gen / "simuconf.tab").write_text("frames_per_second=100\nfast_forward_frames_per_second=100\nautosave=0\n")
    print("generating flat %dx%d map ..." % (size, size))
    subprocess.run(
        ["../build-headless/simutrans/simutrans", "-use_workdir",
         "-set_userdir", str(gen), "-objects", "pak", "-lang", "en",
         "-debug", "1", "-flatmap", str(size), "-until", "1930.7"],
        cwd=ROOT / "simutrans", timeout=1200,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    saved = gen / "autosave-pak.sve"
    if not saved.exists():
        raise SystemExit("flat-map generation produced no save")
    shutil.copyfile(saved, out)
    return out


def _seed_grid(map_size, spacing=16, margin=8):
    """Grid of city seed coords, inset from the map edges."""
    seeds = []
    c = margin
    while c < map_size - margin:
        d = margin
        while d < map_size - margin:
            seeds.append((c, d))
            d += spacing
        c += spacing
    return seeds


def _write_scenario(map_sve, seeds, pop):
    scen = HOME / "simutrans" / "addons" / "pak" / "scenario" / _SCENARIO_NAME
    scen.mkdir(parents=True, exist_ok=True)
    # Per seed: found a city (public player), then grow it by the citizen
    # delta (human player — public-player change_size does not grow).  Literal
    # coords, not a city_list_x()/get_pos() loop, which did not drive growth.
    seed_src = "\n".join(
        ('\tcommand_x(tool_add_city).work(player_x(1), coord3d(%d, %d, 0), "0")\n'
         '\tcommand_x(tool_change_city_size).work(player_x(0), coord3d(%d, %d, 0), "%d")'
         % (x, y, x, y, pop))
        for x, y in seeds)
    (scen / "scenario.nut").write_text(
        _SCENARIO_TMPL.format(seeds=seed_src, begin=_BEGIN, end=_END))
    shutil.copyfile(map_sve, scen / "map.sve")
    return scen


def _run_scenario(timeout):
    """Run sim on the probe scenario, return collected stdout text."""
    # Use the headless (none-backend) binary: it fast-forwards efficiently
    # so each scenario poll spans game months, letting change_size growth
    # materialise.  -until fast-forwards; the probe prints {end} and we
    # terminate well before this date is reached.  Falls back to ../sim.
    binary = ("../build-headless/simutrans/simutrans"
              if _HEADLESS.exists() else "../sim")
    cmd = [binary, "-use_workdir", "-objects", "pak", "-lang", "en",
           "-scenario", _SCENARIO_NAME, "-addons", "-debug", "2", "-until", "9999.1"]
    proc = subprocess.Popen(cmd, cwd=ROOT / "simutrans",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1)
    lines, deadline, done = [], time.time() + timeout, False
    try:
        for line in proc.stdout:
            lines.append(line)
            if _END in line or "ANISO_ERROR" in line:
                done = True
                break
            if "</error>" in line or "Call function failed" in line:
                break
            if time.time() > deadline:
                break
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
    if not done:
        sys.stderr.write("".join(lines[-20:]))
        raise SystemExit("scenario did not reach %s (timeout/error)" % _END)
    return "".join(lines)


def _parse_multi_dump(text):
    """Return {(cq,cr): {"tiles": [(q,r)...], "th": (q,r) or None}}."""
    block = text.split(_BEGIN, 1)[1].split(_END, 1)[0]
    cities = {}
    for line in block.splitlines():
        if "ANISO" not in line:
            continue
        toks = line.split()
        i = toks.index("ANISO")
        q, r = int(toks[i + 1]), int(toks[i + 2])
        cq, cr = int(toks[i + 3]), int(toks[i + 4])
        c = cities.setdefault((cq, cr), {"tiles": [], "th": None})
        c["tiles"].append((q, r))
        if line.rstrip().endswith("TH"):
            c["th"] = (q, r)
    return cities


def _circular_spread(bearings_deg, period=180.0):
    """Concentration of bearings on a circle of given period.

    Returns (mean_bearing, R) where R in [0,1]: R~1 => all bearings align
    (systematic anisotropy), R~0 => bearings random (no preferred axis).
    """
    s = c = 0.0
    for b in bearings_deg:
        ang = 2.0 * math.pi * b / period
        c += math.cos(ang)
        s += math.sin(ang)
    n = len(bearings_deg)
    R = math.hypot(c, s) / n if n else 0.0
    mean = (math.degrees(math.atan2(s, c)) / 2.0) % period
    return mean, R


def cmd_run(argv):
    def opt(name, default):
        return argv[argv.index(name) + 1] if name in argv else default

    pop = int(opt("--pop", "7000"))
    timeout = int(opt("--timeout", "900"))
    min_tiles = int(opt("--min-tiles", "25"))
    map_size = int(opt("--map-size", "160"))
    # spacing must keep neighbours clear of each other: at pop 7000 a city
    # spans ~110 tiles (radius ~7), and < ~40 lets footprints collide, which
    # inflates the aspect and is no longer measuring the growth rule alone.
    spacing = int(opt("--spacing", "40"))

    _ensure_harness()
    # default: generate a flat empty map of the requested size and grid-seed it
    map_sve = Path(opt("--map", "")) if "--map" in argv else _ensure_flat_map(map_size)
    if "--single" in argv:
        seeds = [(map_size // 2, map_size // 2)]
    else:
        seeds = _seed_grid(map_size, spacing)
    _write_scenario(map_sve, seeds, pop)
    print("running probe: map=%s  seeds=%d  growth delta=%d citizens/city"
          % (map_sve.name, len(seeds), pop))
    out = _run_scenario(timeout)
    if "ANISO_ERROR" in out:
        for ln in out.splitlines():
            if "ANISO_ERROR" in ln:
                raise SystemExit(ln.strip())
    cities = _parse_multi_dump(out)

    rows = []
    for (cq, cr), info in sorted(cities.items()):
        tiles = info["tiles"]
        if len(tiles) < min_tiles:
            continue
        res = analyze(tiles, center_qr=info["th"])
        rows.append(((cq, cr), len(tiles), res))

    print("\nmeasured %d cities (>= %d tiles); %d total dumped\n"
          % (len(rows), min_tiles, sum(len(c["tiles"]) for c in cities.values())))
    print("per-city physical-frame footprint:")
    print("  townhall      tiles   aspect  bearing   m2     m4     m6")
    for (cq, cr), n, res in rows:
        ph, m = res["physical"], res["modes"]
        print("  (%3d,%3d)   %5d   %6.2f   %5.1f   %.3f  %.3f  %.3f"
              % (cq, cr, n, ph["aspect"], ph["bearing_deg"],
                 m[2]["amp"], m[4]["amp"], m[6]["amp"]))

    if rows:
        aspects = [r[2]["physical"]["aspect"] for r in rows]
        bearings = [r[2]["physical"]["bearing_deg"] for r in rows]
        ax_aspects = [r[2]["axial"]["aspect"] for r in rows]
        mean_b, R = _circular_spread(bearings)
        print("\naggregate (the systematic-anisotropy signal):")
        print("  physical aspect : mean %.2f  (min %.2f, max %.2f)"
              % (sum(aspects) / len(aspects), min(aspects), max(aspects)))
        print("  axial aspect    : mean %.2f" % (sum(ax_aspects) / len(ax_aspects)))
        print("  bearing alignment R = %.3f  (1=all aligned, 0=random)" % R)
        print("  mean bearing    : %.1f deg" % mean_b)
        print("\ninterpretation:")
        print("  * aspect >> 1 AND R near 1 -> systematic directional bias in growth")
        print("  * mean bearing ~60 deg      -> consistent with the square axial AABB")
        print("    mapping to a 60-deg physical rhombus")
        print("  * physical aspect >> axial aspect -> basis shear is the bigger share")
    return 0


def main(argv):
    if "--self-test" in argv:
        return _self_test()
    if "--coords" in argv:
        path = argv[argv.index("--coords") + 1]
        with open(path) as f:
            coords = parse_coord_dump(f.read())
        print(format_report(analyze(coords)))
        return 0
    if "--run" in argv:
        return cmd_run(argv)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
