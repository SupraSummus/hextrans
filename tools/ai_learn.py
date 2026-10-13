#!/usr/bin/env python3
"""Fit an AI's link choice to what its links earned in -ai_soak games.

Usage:
    tools/ai_learn.py                        # the goods AI, 4 rounds of 12 games, 1930 -> 1950
    tools/ai_learn.py --ai passenger         # the passenger AI's choice of town pairs
    tools/ai_learn.py --eval tools/ai_policies/pak64-goods.policy --seeds 9 10 11 12
    tools/ai_learn.py --eval none            # the hand-written goods AI
    tools/ai_learn.py --fit-only build/ai-learn/links.csv

A game logs one ai_line line per link an AI tries to build (its features,
capital cost, added upkeep and convoy ids) and one ai_convoy line per AI
convoy and month, and when the goods AI retires one
(src/simutrans/player/ai_policy.h).  Each link gets a label, what it is worth
over a planning horizon:

    (profit - upkeep) per month alive * horizon - capex + vehicle value

A logistic regression of "label > 0" on the features of the built links is
the next policy: its score is the log odds that a link pays back less the
log odds of --threshold.

A round plays --games new maps with the current policy, which builds a random
candidate with probability --explore, refits on all links so far, and scores
the fit on --seeds.  A score is the AI's net wealth at the end over the
starting money, so 1.0 is what an AI that never builds keeps; a bankrupt AI
scores -1 plus the share of the game it survived.  The other AI plays its
hand-written choices.

The engine is --exe, else build-release/ (a headless Release build:
`cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DSIMUTRANS_BACKEND=none`),
else whatever tools/ai_soak.py would pick.  Each game gets a fresh user dir.
Results go to --out: links.csv, round-N.policy and best.policy.
"""
import argparse
import csv
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ai_soak import ROOT, ensure_pak, find_exe  # noqa: E402

# the AI a run fits: its -ai_soak player slot, the engine option for its
# policy and the features of its links (player/ai_policy.h)
AIS = {
    "goods": (2, "-ai_policy", ["revenue", "distance", "log_distance", "density", "production",
                                "margin", "loads", "raw", "stock", "end", "downstream", "served",
                                "water"]),
    "passenger": (3, "-ai_policy_passenger", ["population_from", "population_to", "distance",
                                              "log_distance", "new_hubs", "across_water"]),
}

STATUS = re.compile(r"^ai_soak: (\d{4})\.(\d{2}) convoys=")
MONEY = re.compile(r"^ai_soak: starting_money=(-?\d+)")


def patterns(player):
    """The log lines of one player: wealth, bankruptcy, links and convoys."""
    p = str(player)
    return (re.compile(r"^ai_soak: \d{4}\.\d{2} player " + p + r" \(type \d+\) .* wealth=(-?\d+)"),
            re.compile(r"^ai_soak: (\d{4})\.(\d{2}) player " + p + r" bankrupt"),
            re.compile(r"^ai_line: (\d{4})\.(\d{2}) player " + p + r" (\w+) mode=(\S+) from=(\S+) "
                       r"to=(\S+) good=(\S+) capex=(-?\d+) upkeep=(-?\d+) convoys=(\S+) (.*)$"),
            re.compile(r"^ai_convoy: (\d{4})\.(\d{2}) player " + p +
                       r" id=(\d+) profit=(-?\d+) value=(-?\d+)$"))


def month(year, mon):
    return int(year) * 12 + int(mon) - 1


def play(exe, args, seed, policy_file, env):
    """One game -> (fitness, detail, log)."""
    cmd = [str(exe), "-use_workdir", "-objects", "pak", "-nosound", "-nomidi", "-debug", "2",
           "-ai_soak", args.size, str(seed), "-until", args.until]
    if policy_file:
        cmd += [args.option, str(policy_file)]
    with tempfile.TemporaryDirectory(prefix="ai-learn-") as user:
        try:
            run = subprocess.run(cmd, cwd=ROOT / "simutrans", env=dict(env, XDG_DATA_HOME=user),
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                 timeout=args.timeout, text=True, errors="replace")
        except subprocess.TimeoutExpired:
            return -2.0, "timeout", ""
    wealth_re, bankrupt_re, _, _ = patterns(args.player)
    money = first = wealth = bankrupt = None
    for line in run.stderr.splitlines():
        if (m := MONEY.match(line)):
            money = float(m.group(1))
        elif (m := STATUS.match(line)):
            first = first if first is not None else month(m.group(1), m.group(2))
        elif (m := wealth_re.match(line)):
            wealth = float(m.group(1))
        elif (m := bankrupt_re.match(line)):
            bankrupt = month(m.group(1), m.group(2))
    if run.returncode != 0 or money is None or first is None:
        return -2.0, "engine failed (exit %s)" % run.returncode, ""
    if bankrupt is not None:
        year, mon = args.until.partition(".")[::2]
        survived = (bankrupt - first) / max(1, month(year, mon or 1) - first)
        return -1.0 + survived, "bankrupt %d.%02d" % (bankrupt // 12, bankrupt % 12 + 1), run.stderr
    if wealth is None:
        return -2.0, "no wealth logged (stopped early?)", ""
    return wealth / money, "wealth %.0f" % wealth, run.stderr


def links_of_game(log, args):
    """One row per link the AI built or failed to build in this game."""
    _, _, line_re, convoy_re = patterns(args.player)
    links = []
    owner = {}  # convoy id -> link, the latest that claimed it
    last = None
    for line in log.splitlines():
        if (m := STATUS.match(line)):
            last = month(m.group(1), m.group(2))
        elif (m := line_re.match(line)):
            feats = dict(kv.split("=") for kv in m.group(11).split())
            link = {
                "built": month(m.group(1), m.group(2)), "outcome": m.group(3), "mode": m.group(4),
                "from": m.group(5), "to": m.group(6), "good": m.group(7),
                "capex": float(m.group(8)), "upkeep": float(m.group(9)),
                "profit": 0.0, "value": {}, "seen": None,
                **{f: float(feats[f]) for f in args.features},
            }
            links.append(link)
            if m.group(10) != "-":
                for cid in m.group(10).split(","):
                    owner[cid] = link
        elif (m := convoy_re.match(line)):
            link = owner.get(m.group(3))
            when = month(m.group(1), m.group(2))
            if link is not None and when >= link["built"]:
                link["profit"] += float(m.group(4))
                link["value"][m.group(3)] = float(m.group(5))
                link["seen"] = when
    rows = []
    for link in links:
        if link["outcome"] != "built":
            # revenue booked while it tried can make capex negative
            label = -max(0.0, link["capex"])
        else:
            # alive until its last convoy, or to the end of the game
            seen = link["seen"] if link["seen"] is not None else link["built"]
            alive = (last if seen >= last - 1 else seen + 1) - link["built"]
            if alive < args.min_months:
                continue  # built too late to say what it earns
            net = link["profit"] / alive - link["upkeep"]
            label = net * args.horizon - link["capex"] + sum(link["value"].values())
        row = {k: v for k, v in link.items() if k not in ("value", "seen")}
        row["label"] = label / 1000.0
        rows.append(row)
    return rows


def fit(rows, features, ridge, threshold):
    """Logistic regression of "label > 0" -> (bias, {feature: weight}, AUC).

    Only built links count: a failed try costs little but the month, and
    mixing them in would make every link look unlikely to pay.
    """
    rows = [r for r in rows if r["outcome"] == "built"]
    x = np.array([[r[f] for f in features] for r in rows])
    y = np.array([r["label"] > 0 for r in rows], dtype=float)
    mean, std = x.mean(axis=0), x.std(axis=0)
    std[std == 0] = 1.0
    z = np.c_[np.ones(len(x)), (x - mean) / std]
    penalty = ridge * len(rows) * np.diag([0.0] + [1.0] * len(features))
    w = np.zeros(z.shape[1])
    for _ in range(50):  # Newton steps
        p = 1 / (1 + np.exp(-(z @ w)))
        hess = (z.T * (p * (1 - p))) @ z + penalty + 1e-9 * np.eye(len(w))
        w -= np.linalg.solve(hess, z.T @ (p - y) + penalty @ w)
    weights = w[1:] / std
    bias = w[0] - (weights * mean).sum() - np.log(threshold / (1 - threshold))
    rank = np.argsort(np.argsort(z @ w))
    n1 = y.sum()
    auc = (rank[y == 1].sum() - n1 * (n1 - 1) / 2) / max(1, n1 * (len(y) - n1))
    return bias, dict(zip(features, weights)), auc


def policy_from(bias, weights, args, explore):
    return {"bias": bias, **{"w_" + f: w for f, w in weights.items()},
            "explore": explore, "cash_reserve": args.cash_reserve, "retire_loss": args.retire_loss,
            "retire_line": args.retire_line, "hq_reserve": args.hq_reserve,
            **({"city_stops": args.city_stops} if args.ai == "passenger" else {})}


def policy_text(policy, comment):
    return "\n".join(["# " + comment] + ["%s = %.6g" % kv for kv in policy.items()]) + "\n"


def write_policy(path, policy, comment):
    Path(path).write_text(policy_text(policy, comment))


def evaluate(pool, exe, args, env, policy_file):
    """Mean fitness over args.seeds, and the per-seed (fitness, detail)."""
    results = list(pool.map(lambda s: play(exe, args, s, policy_file, env)[:2], args.seeds))
    return sum(f for f, _ in results) / len(results), results


def learn(pool, exe, args, env):
    out = Path(args.out).resolve()
    rows = []
    best = None
    # round 0 builds only random candidates
    policy = policy_from(0.0, dict.fromkeys(args.features, 0.0), args, 1.0)
    for rnd in range(args.rounds + 1):
        train = out / ("round-%d-train.policy" % rnd)
        write_policy(train, policy, "round %d, training" % rnd)
        seeds = range(args.first_seed + rnd * args.games, args.first_seed + (rnd + 1) * args.games)
        games = list(pool.map(lambda s: play(exe, args, s, train, env), seeds))
        if not any(log for _, _, log in games):
            sys.exit("round %d: no game ran (%s)" % (rnd, games[0][1]))
        logs = [log for _, _, log in games]
        new = [dict(row, round=rnd, seed=seed) for seed, log in zip(seeds, logs)
               for row in links_of_game(log, args)]
        rows += new
        with open(out / "links.csv", "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print("round %d: %d links, %d built" % (
            rnd, len(new), sum(r["outcome"] == "built" for r in new)), flush=True)
        if sum(r["outcome"] == "built" for r in rows) < 2 * len(args.features):
            continue  # too few to fit; explore another round
        bias, weights, auc = fit(rows, args.features, args.ridge, args.threshold)
        path = out / ("round-%d.policy" % rnd)
        write_policy(path, policy_from(bias, weights, args, 0.0),
                     "round %d, fitted on %d links, training AUC %.3f" % (rnd, len(rows), auc))
        fitness, results = evaluate(pool, exe, args, env, path)
        print("  AUC %.3f, fitness %.3f (%s)" % (
            auc, fitness, ", ".join("%.2f" % f for f, _ in results)), flush=True)
        if best is None or fitness > best:
            best = fitness
            write_policy(out / "best.policy", policy_from(bias, weights, args, 0.0),
                         "fitness %.3f on seeds %s, round %d" % (
                             fitness, " ".join(map(str, args.seeds)), rnd))
        policy = policy_from(bias, weights, args, args.explore)
    if best is not None:
        print("best fitness %.3f: %s" % (best, out / "best.policy"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ai", choices=sorted(AIS), default="goods", help="the AI to fit")
    ap.add_argument("--rounds", type=int, default=4)
    ap.add_argument("--games", type=int, default=12, help="training games per round")
    ap.add_argument("--first-seed", type=int, default=1000, help="seed of the first training map")
    ap.add_argument("--seeds", nargs="+", type=int, default=list(range(1, 9)),
                    help="maps that score a policy")
    ap.add_argument("--size", default="128", help="map size, or W,H,D for a hexagon")
    ap.add_argument("--until", default="1950.1")
    ap.add_argument("--explore", type=float, default=0.3,
                    help="chance of a random candidate in training games")
    ap.add_argument("--horizon", type=float, default=120, help="planning horizon in months")
    ap.add_argument("--min-months", type=int, default=12)
    ap.add_argument("--threshold", type=float, default=0.3,
                    help="build links that pay back with at least this probability")
    ap.add_argument("--ridge", type=float, default=0.01)
    ap.add_argument("--cash-reserve", type=float, default=0.3)
    ap.add_argument("--retire-loss", type=float, default=0.5)
    ap.add_argument("--retire-line", type=float, default=1.5,
                    help="close a road link earning less than this times its road's upkeep")
    ap.add_argument("--hq-reserve", type=float, default=100.0)
    ap.add_argument("--city-stops", type=float, default=6,
                    help="passenger AI: stops of the bus line it lays in each town it joins")
    ap.add_argument("--eval", metavar="POLICY",
                    help="only score POLICY on --seeds ('none': the hand-written AI)")
    ap.add_argument("--fit-only", metavar="LINKS_CSV", help="only fit LINKS_CSV and print the policy")
    ap.add_argument("--exe")
    ap.add_argument("--out", default=str(ROOT / "build" / "ai-learn"))
    ap.add_argument("--timeout", type=int, default=900, help="seconds per game")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = ap.parse_args()
    args.player, args.option, args.features = AIS[args.ai]

    if args.fit_only:
        with open(args.fit_only) as f:
            rows = [{k: v if k in ("outcome", "good", "mode", "from", "to") else float(v)
                     for k, v in r.items()} for r in csv.DictReader(f)]
        bias, weights, auc = fit(rows, args.features, args.ridge, args.threshold)
        print(policy_text(policy_from(bias, weights, args, 0.0), "training AUC %.3f" % auc), end="")
        return

    release = ROOT / "build-release" / "simutrans" / "simutrans"
    exe = Path(args.exe).resolve() if args.exe else release if release.is_file() else find_exe(None)
    ensure_pak()
    Path(args.out).mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, SDL_VIDEODRIVER=os.environ.get("SDL_VIDEODRIVER", "dummy"))
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        if args.eval:
            policy_file = None if args.eval == "none" else Path(args.eval).resolve()
            fitness, results = evaluate(pool, exe, args, env, policy_file)
            for seed, (f, detail) in zip(args.seeds, results):
                print("seed %d: %.3f (%s)" % (seed, f, detail))
            print("fitness %.3f" % fitness)
        else:
            learn(pool, exe, args, env)


if __name__ == "__main__":
    main()
