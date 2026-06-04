# AGENTS.md

Canonical agent-facing project doc. `CLAUDE.md` in the repo root
is a symlink to this file so Claude Code's default load path keeps
working — edit `AGENTS.md`, both names resolve to it.

## Project context

This fork of Simutrans is transitioning from the upstream square-tile
grid to a **hexagonal grid**. **`TODO.md`** is the live list of items
still to do. Key design decisions (flat-top hex, right-prism
z-stacking, in-place port) are summarised below.

## Approach

Flat-top axial coords `(q, r)`, byte-compatible with the existing
`koord` struct's `(x, y)`. The port is therefore semantic — we redefine
what `koord` means and rewrite the operations on it; storage layout,
`planquadrat_t* plan` indexing, and save-file format all stay.

Hexagonal right prism for z-stacking, *not* FCC close-packing. "Up"
stays a single-cell direction so bridges, tunnels, pillars and
multi-level stations keep working.

No parallel types, no compile-time `HEX` flag. We change the existing
`koord` / `ribi` / `slope_t` in place, accepting a transient period
where the codebase compiles but parts of game logic are inconsistent.
This is a real port, not an experimental side-branch.

## Tripwires over silent shims

The in-place port leaves legacy functions whose signatures still
compile but whose semantics no longer hold under the hex model. The
default move is to replace the body with `dbg->fatal` rather than
quietly route through the wrong slot. The shim becomes a tripwire:
some call paths still work, others fire on first use, and the
ground-truth list of residual sites surfaces as crashes instead of
silent mis-indexing. `surface_t::lookup_hgt` is the canonical
example — see commit `a1a7279` for the move from silent shim to
fatal tripwire and the bugs it shook loose. This is what lets us
defer mass replacing without ending up with green CI propped up by
shims; the codebase compiles, honest paths run, dishonest paths
abort loudly.

Sites that genuinely cannot port yet take a narrow named escape
hatch — `legacy_*` wrappers that preserve the old slot semantics
for clusters with documented retirement triggers in `TODO.md` (e.g.
"after the save-format bump", "after `recalc_natural_slope`
ports"). One escape-hatch name per cluster, not per call site; the
cluster description lives in `TODO.md`, not duplicated across
synonymous wrappers.

## Engine encoding leads the pakset

Engine-side encoding decisions don't wait on shipped pakset art.
When the hex geometry calls for a wider slot count, a new keyed
layout, or a renamed desc field, the engine takes the move and the
paksets follow.  This is not a tradeoff against red CI: tests run
against pak64 with the `tests/test-pak/` overlay baked on top
(`tools/test.py::ensure_test_pak`), and any descriptor shape the
engine needs for test coverage gets a dummy entry in `test-pak/`.
The headless test path renders nothing (`image[-]=-`), so the
dummies don't need real sprite art — just the .dat keys the
engine reads.  `test_road_double.dat` is the canonical example:
opts a single road into `has_double_slopes=1` for the double-slope
tunnel tests, because pak64 itself does not.

Visible-side gaps in real paksets (missing fence combos, missing
cliff atlases, missing way_ground images) are downstream catch-up
work, not engine blockers.  `FENCE_IMAGE_COUNT = 7` is a recent
case: the engine carves the full hex back-wall mask now, and the
visual side waits on the pakset commit that fills indices 3..13.

Leading sometimes means a bigger engine change than a slot count, and
building footprint orientation is the case to know.  Today
`building_desc_t::get_size`/`get_x`/`get_y` derive a building's per-layout
footprint as only two shapes — a rectangle and its transpose, keyed on
`layout & 1` — a square-grid 90°-rotation artifact, both axis-aligned in
`(q, r)`.  Hex's 3rd axis (NE–SW) is a *diagonal* of that rectangle, so
under the current model a multi-tile building has no NE/SW orientation;
only a 1×1 footprint (a single hex, 6 real edges) does.  This is a
missing *engine* capability, not a pakset gate: orienting a multi-tile
building to all 6 hex directions needs the footprint model extended to a
genuine axial rotation of the cell set (per-layout cell lists, or a
parallelogram footprint), and that is engine work to do independent of
any pak — the pakset supplies images once it exists.  Until then the
cheap slice is the layout→direction mapping, correct for 1×1 buildings
as-is and ungated by sprite count: the townhall road seed in
`check_bau_townhall` orients over all 6 hex faces for any 1×1 townhall
(`adjust_layout` folds a missing NE/SW orientation onto an available
image, so even pak64's 1-layout townhall seeds isotropically).
Multi-tile 3rd-axis orientation is deferred engine work; the depot
3rd-axis and powerline crossing entries in `TODO.md` need the same
footprint extension.

## Diff against the upstream `simutrans` branch

The pre-port upstream is tracked on the local `simutrans` branch
(read-only reference; not for development). `main` was forked from
it, so the two share a real merge base and upstream fixes can be
merged in normally — but the everyday use of this branch is reading,
not merging, to evaluate divergence on code we are about to touch.

```sh
git show refs/heads/simutrans:src/simutrans/<file> | sed -n 'A,Bp'
git grep -n '<symbol>' refs/heads/simutrans -- 'src/simutrans/<dir>/*'
```

(`git fetch --unshallow origin` first if `git log` is short.)

To sync upstream: fetch `simutrans/simutrans` master, fast-forward
the local `simutrans` branch onto it, and push to `origin`. Then
merge `origin/simutrans` into `main` (or cherry-pick targeted
commits when the full set isn't wanted). Conflicts are usually in
files the hex port has already rewritten and are resolved by
keeping the hex side; upstream-only files apply clean.

```sh
git fetch https://github.com/simutrans/simutrans master
git push origin FETCH_HEAD:refs/heads/simutrans
git fetch origin simutrans
git merge origin/simutrans   # on main, or on a feature branch
```

The comparison's main value is sharpening claims — when the port
has left a "dead under hex" read or a "this branch only made sense
under square geometry" comment, reading upstream pins down what the
code actually fed and makes "geometric residue vs. real logic"
visible. Use it to confirm a retirement is honest, or to notice
that upstream had a non-geometric reason (save-format
compatibility, an AI heuristic, collision avoidance) that survives
into hex.

Lower diff lowers merge-conflict surface when an upstream sync
eventually happens — a real but soft pressure, not a mandate to
preserve upstream verbatim. Tier the choices:

*Mechanical / incidental divergence.* Stay close. `ribi_t::east` →
`ribi_t::southeast` is the same axial vector under the current
viewport; a future merge sees a localised rename, not restructured
logic.

*Semantic divergence.* Diverge cleanly. `rotate90` is geometrically
wrong under flat-top hex; keeping the name would mislead.
`rotate60` is correct even though the diff grows.

*Abstraction wins.* Best when available: pick a name / signature
that's neutral across both grid models. `rotate_one_step` instead
of either `rotate90` or `rotate60` works in upstream *and* in hex;
the rotation amount becomes an implementation detail. Hex-correct,
upstream-mergeable, organically sorts the layering, and a candidate
for upstreaming back. Reach for this tier when a rename is already
on the table; don't force it where it would be contrived.

## Cross-checking simutrans-extended

`simutrans-extended` is also checked out as a local read-only
reference branch (origin/simutrans-extended, fetched from
`jamespetts/simutrans-extended`). It shares zero commits with our
history — distinct root commit, no merge base — so nothing is
liftable; this is purely for reading.

Extended is not a hex port and most of its divergence is gameplay
depth (passenger routing, signalling, axle loads), orthogonal to
the geometric work. The case where it's worth a look is when we
suspect upstream's solution to a problem is square-grid-specific
*and* extended has had a non-geometric reason to rework the same
code — they may have already factored out a grid-neutral seam we
can copy the *shape* of. Concrete suspects: Manhattan distance
(square-only; extended reportedly uses real distance), turn cost
in the convoy/physics path (90°-only assumptions don't survive
hex), anything else where upstream code reads as "tile counts
standing in for physics".

Don't make this a standing input — it's a targeted probe when a
specific factoring question is already on the table. Read for the
seam, not the code.

## Direction naming convention (pin this — silent-failure landmine)

Flat-top hex has 6 EDGES and 6 VERTICES, and each set has its own
compass labelling. Confusing them gives a 30°-off grid that compiles
fine.

EDGES (and the 6 neighbour directions reached through them):
**N, NE, SE, S, SW, NW**. Flat-top hexes DO have due-N and due-S
edges, so neighbour directions DO include N and S.

VERTICES / CORNERS (6 per tile): **E, SE, SW, W, NW, NE**. Flat-top
hexes have NO due-N or due-S corner; the 6 vertices sit at angles
0°, 60°, 120°, 180°, 240°, 300° from the centre.

`koord::neighbours[]` (in `src/simutrans/dataobj/koord.cc`) is ordered
clockwise starting from the SE neighbour, matching the EDGE convention
above.

## Axial coords vs physical hex symmetry (tentative — one case so far)

`koord(q, r)` is axial, not Cartesian.  `(1, 1)` is NOT a neighbour —
it's two edge-steps (SE+S).  Square-grid intuition treats it as
"diagonally adjacent" and that occasionally leaks into hex reasoning;
PR #201 is the worked example, with `TODO.md` → "Growing-chord
overhang in 60°-pair parallel builds" as the durable artifact.

Tentative framing from that case: pairs of non-opposite hex axes come
in two flavours — **60° apart** (cyclically adjacent: SE↔S, S↔SW, …)
and **120° apart** (skew: SE↔SW, S↔NW, …).  Both classes appear to
be closed under rotation AND reflection — a 60°-pair never maps to a
120°-pair under any hex symmetry.  Sum of axes: `A+B` is itself a
hex axis iff A and B are 120° apart; `A+B` is between axes (a 2-step
"wedge-tip" displacement) iff A and B are 60° apart.  When a
behavioural difference between two inputs looks like anisotropy,
worth checking whether the inputs sit in the same symmetry class
before reaching for a router/detection fix.

Not validated beyond the one case — refresh from PR #201 if a
similar question arises, and update this section if a second case
either confirms the framing or shows it's the wrong abstraction.

## Critical findings driving priority

Per-vertex height storage is the top of the critical path. A hex
vertex is shared by 3 tiles (vs 4 for a square corner). Storing slopes
per-tile, like the legacy code does today, will produce inconsistent
terrain across shared vertices the moment terraforming touches it. The
fix is to store heights per-vertex and derive slopes — the same
pattern as the existing `surface_t::grid_hgts` array, but with a
denser, irregular vertex topology. The terraform, flood-fill and
climate-transition code can't be ported cleanly until this lands.

## TODO file rules

`TODO.md` tracks work in flight — port work, gameplay bugs,
upstream-inherited rough edges, anything else worth tracking
centrally rather than as a scattered code comment. Working rules:

It is not a changelog. When an entry is resolved, or becomes outdated,
**delete it**. Do not strike it through, do not leave a "(done)" note.
Git history is the changelog.

Use paragraphs, not bullet lists. Paragraphs are easier to insert and
delete; lists encourage atomic-bullet thinking and accumulate noise.

If you notice something sketchy while working in the code — a wrong
assumption, a TODO comment that should be tracked, a gameplay bug
introduced by a port move — add a paragraph to `TODO.md` rather than
leaving it as a `// HEX-PORT TODO` comment that drifts away from the
context that produced it.

Every entry must name a concrete next move — what the fix would
actually look like. "Verify in-game once a pakset is available" or
"eyeball when somebody runs the game" aren't next moves, they're
hopes. A trigger is useful when one exists (e.g. "lands with the
third-axis bridge bake — see <other entry>"), but a soft trigger
like "when next refactoring this cluster" or "alongside a future
elegance pass" is fine — the bar is that the work is actionable,
not that something automatically fires. "Implemented but needs
testing" without a way to test belongs in the commit message of
the change that landed it, not here. Same for a thin action
("propose upstream", "open a bug") wrapped in a paragraph of
post-landing state — the action belongs in an issue tracker, the
durable context in the commit message or `AGENTS.md`.

The aim is for `TODO.md` to remain a useful, current map of "what
still needs doing". A growing TODO file is fine; a stale one is not.

To surface stale entries, blame `TODO.md` and sort paragraphs
(blank-line separated) by their max `author-time`: the lowest
non-header values are the entries no edit has touched longest.
`git fetch --unshallow origin` first if history is grafted.

## Commit message rules

Default to short. The diff already shows *what* changed; the message
captures only the *why* a reader can't recover from it. A one-line
subject with no body is the right answer for mechanical fixes and
obvious refactors.

Subject: short, present-tense, scope-prefixed (`hex-port:`,
`hex-port tests:`, `AGENTS.md:`). Keep the prefix consistent across
commits in the same area. ≤ 72 chars, no metrics.

Body: usually 1–2 short paragraphs, often none. Cover the
load-bearing reason, a non-obvious trade-off, a shim's retirement
trigger — anything that would surprise a reader who knows the
codebase but not this commit. Don't:

- Re-explain the surrounding subsystem; link to the file, symbol or
  prior commit.
- Enumerate every line deleted or call site touched.
- Narrate verification ("verified with `cmake --build`", "ASAN
  clean").
- Recap the companion engine/pakset commit; name it and stop.
- Inline durable design context. That belongs in `TODO.md` /
  `AGENTS.md`, where it stays current.

If a body is getting long, prefer splitting the commit or moving the
context into `TODO.md` / `AGENTS.md`.

Voice — write so the maintainer reading the commit in two years
doesn't hear the chat that produced it. Category labels and
metaphors that come up naturally while thinking through a change
("sink", "shape", "siblings", "footgun", "weaponise nine sites at
once") read as author-voice on the way out; they sound like
security research or refactoring talk rather than a description of
the code. Strip them and say what's there: "custom_fatal calls
syslog with an already-expanded buffer" instead of "the syslog
format-string sink"; "the same pattern repeats in pakset, debug,
message, warning, error" instead of "five siblings of the same
shape". Quoted noun phrases that compress a paragraph into a
label — `the "format is consumed once" invariant` — force the
reader to unpack them; write the sentence instead.

Author identity — don't guess from session context.  `git log
--format="%an <%ae>" | sort -u | grep -i <name>` lists what the
repo already uses for a person; pick the existing identity rather
than inventing a new one (the GitHub `users.noreply` form is the
usual default here).  Ask when in doubt.

## Tests and the hex port

The scenario tests under `tests/` encode gameplay invariants. Many of
those invariants survive the hex port, but the specific assertions
(coord choices, direction constants, slope enum names, neighbour
counts) bake in the square-grid model. When the port breaks a test,
do not paper over the regression by preserving legacy square data
structures — parallel neighbour tables, `#ifdef HEX` forks, or silent
auto-conversion shims. Red CI during the port transition is more
honest than green CI propped up by shims; if the legacy code path
needs to stay alive temporarily, use the tripwire pattern above.

Per-test, decide:

*Migrate* when the invariant survives the port and only the
assertions are square-specific (e.g. asserts 4-way vertex sharing,
4-corner slope names, 8-way neighbour iteration). Comment the entry
out of `tests/all_tests.nut` with a short `// foo: HEX-PORT PENDING.`
tag, leave the function body in place with the same tag above
`function foo()`, and add a paragraph to `TODO.md` with the trigger
for restoration (typically "after per-vertex height storage lands"
or "after ribi widens to 6 bits"). The detail lives in `TODO.md`,
not duplicated per-test. Delete the function, the `all_tests.nut`
comment and the TODO entry together at restoration time.

*Delete* when the invariant is a purely square-grid geometric property
— e.g. a test that asserts the exact shape of an 8-neighbour climate
transition mask. Remove the function and the `all_tests.nut` entry
together, without leaving a commented stub. No TODO.md entry either;
git history has the record.

*Fix* when the test is correct and the code is the regression. Fix the
code.

Test isolation caveat: the scenario runner does not reset world state
between tests. A failure mid-test cascades into every subsequent test
that expects a clean map (via `RESET_ALL_PLAYER_FUNDS`' maintenance
check, leftover buildings, raised grid points, etc.). When triaging a
large failing set, expect most failures to be cascades from a smaller
number of actual hex regressions. Work the cascade head first, rerun,
and see what is left before classifying the tail.

Hex test-authoring primer. Coordinate-to-direction: along `+y` (constant
`x`, increasing `y`) is hex S; along `+x` is hex SE under the current
2:1 viewport rename (see "Old-east→hex-SE" in `TODO.md`). Slope names
use a low-edge-X convention: `south_narrow` has S as the *low* edge, so
the tile rises toward N. `slope_type(ribi)` (in `dataobj/ribi.cc`)
returns the slope where a way ascends in the ribi direction —
`slope_type(N) == south_narrow` and so on. Ribi values are in
`dataobj/ribi.h` (SE=1, S=2, SW=4, NW=8, N=16, NE=32).

To set up a single elevated tile with clean 2-corner edge slopes on
its 6 neighbours, use `raise_hex_tile(pl, q, r, z)` from
`test_helpers.nut`: 3 `grid_raise` plus 3 `grid_raise_at_corner` calls
hit all 6 vertices of the centre tile, each shared with one of the 6
surrounding tiles (which inherit edge slopes for free).
`raise_hex_tile_pair_S` is the 2-tile S-axis variant — `(q, r)` and
`(q, r+1)` share 2 vertices across their common edge.
`lower_hex_tile{,_pair_S}` reverse the lift, and applied to flat
ground (`z=-1`) instead dig a flat-floored pit one level down —
`test_terraform_raise_lower_water_level` uses that as a watertight
basin: water poured into the floor stays contained by the 0-height
rim. The square-era 4-grid_raise scaffold (raise the 4 NW corners of
a 2x2 grid square) does not produce a flat hex tile — it hits only 4
of the 6 vertices, leaving an alternating-corner slope.

When the test wants an edge-narrow slope on a single tile (e.g. for
a tunnel mouth or bridge ramp) without lifting the whole tile,
`grid_lower` / `grid_lower_at_corner` the 2 corners of the chosen
edge directly — the tile then carries an `<edge>_narrow` slope at
base z-1 with the chosen edge low.  Each lowered corner is shared
with 2 other tiles, so this only works when those neighbours aren't
blocked by buildings.  `test_powerline_build_underground_transformer_on_powerline`
is the worked example (SE edge of (3,2) lowered for a NW-facing
mouth that tunnels under an adjacent mine tile).

`way_desc_t::has_double_slopes()` reads an explicit `has_double_slopes=`
.dat key (way_desc save version 9); paksets opt each way in or out
independently of which slope sprites they ship.  Pak64 doesn't set it,
so every pak64 way answers `false` and a second consecutive
`setslope(all_down)` on the same underground tunnel tile fails with
"Tile not empty".  Tests that need a 2-step underground staircase
(`test_way_tunnel_build_above_tunnel_slope`, `_across_tunnel_slope`)
select `test_tunnel_double` from `tests/test-pak/` (a road tunnel that
lays the `test_road_double` way) rather than `tunnel_desc_x.get_available_tunnels(wt_road)[0]`.

Multiple tunnel mouths converging on one buried tile: use
`raise_hex_hill_with_six_mouths(pl, q, r, z, desc)` from
`test_helpers.nut` — it raises the hill and drills 6 mouths (one
full S-axis drill + 4 single-mouth branches that `find_end_pos`
joins to the buried network).  The mechanic to know: a no-ctrl
build call on a fresh hex-neighbour tile, when `find_end_pos` finds
existing underground tunnel uphill, adds a single mouth that joins
as a branch — *not* an isolated stub.  Ctrl-flag mouths look similar
but create a way with `ribi = 0` and won't satisfy downstream
single-ribi gates (depot, signal, …).  Branched networks refuse
piecemeal removal ("This tunnel branches.  You can try Control+Click
to remove."), so cleanup wants `set_flags(2)` throughout.

## Externalize the thinking

Long internal monologue on a hard idea is brittle. A private chain
of reasoning has no sanity check beyond the agent's own confidence,
and if the session is interrupted or compacted mid-thought the work
disappears with it. Default to thinking on the page: short, frequent
user-visible checkpoints — one sentence describing what you're
trying to figure out, what you've ruled out, what the next probe is
— beat a ten-paragraph internal deliberation that surfaces only as
a conclusion.

When the idea is concrete enough to live in the codebase, write it
there. A stub with a fatal body and a one-line comment, a paragraph
in `TODO.md`, an exploratory unit test, a half-finished edit
committed on the working branch — all are durable, all are visible,
and all give the user a place to redirect before the agent spends
an hour chasing the wrong shape. Prefer many small visible moves
over one large invisible one.

This applies hardest to the kind of design questions this port
keeps producing — vertex topology, ribi widening, slope encoding.
Those are the problems where a long internal chain is most tempting
and least reliable.

## Design docs

Longer-form design and planning docs live under `documentation/`:

  - `documentation/libcurl-port.md` — live behaviour of the four
    HTTP call sites now routed through libcurl, plus resolved
    decisions (proxy policy, address-family, gating).  Legacy
    in-house socket code remains under `#else SIMUTRANS_USE_CURL`
    for one release cycle as a downstream fallback; retirement
    trigger in `TODO.md` → "other".
  - `documentation/hex-vertex-storage.md` — per-vertex height
    storage on the hex grid, canonical ownership rule and storage
    layout.  Describes live behaviour.
  - `documentation/world-mutation-deferral.md` — why GUI handlers
    can't synchronously call `welt->init()` / `welt->load()` /
    `karte_t::destroy()` (event dispatch nests inside
    `karte_t::step`), and the `tool_t` dispatch path that fixes
    it.  Describes live behaviour.
  - `documentation/fuzz-corpora.md` — what goes in
    `src/fuzz/corpus/` and what stays out-of-band: the committed
    corpus is a regression-and-wiring gate (generated smoke seeds +
    minimized bug reproducers), not a coverage corpus, and the
    grown mutation corpus is not committed.

## Upstream documentation pointers

The upstream Simutrans wiki has development-tagged pages worth a
look when porting unfamiliar subsystems.  The index is at
`https://simutrans-germany.com/wiki/wiki/tiki-browse_freetags.php?tag=development`
and currently has four entries:

  - `en_Developing_Simutrans` — contribution prerequisites, coding
    style, directory structure, debugging tips.
  - `en_GUI_code` — gui_aligned_container_t, gui_frame_t, table
    layout conventions used in info windows.
  - `en_Overview_of_Simutrans_Code` — index to seven chapters
    (world, ways, ribis, signs, vehicles, schedules, convoys).
  - `en_Translation` — `.tab` files, `translator::translate()`,
    the SimuTranslator workflow.

The chapter pages live at `en_Code_Overview_<N>_<Topic>` URLs
(e.g. `en_Code_Overview_4_Signs`).  In practice the chapters are
high-level overviews — useful for "what class lives where" but
thin on the implementation details a porter actually needs (FSMs,
field semantics, save layout); read these as orientation, not as
ground truth.  The code itself remains the source of truth, with
upstream `refs/heads/simutrans` (see "Diff against the upstream
`simutrans` branch") as the practical comparison surface.

There is also an automatically-generated code-reference site at
`https://doc.simutrans-germany.com/Simutrans-Code/` referenced by
the wiki; not exercised here yet.

## Working notes

Compile from the repo root: `cmake --build build -j "$(nproc)"`. The
session-start hook configures `build/` for you.  Makeobj is
`EXCLUDE_FROM_ALL` so it needs an explicit `--target makeobj`.

The cmake build's default warning flags are narrower than the
`src/makeobj/Makefile` set (which adds `-Wall -Wextra
-Wcast-align`).  CI builds makeobj via the Makefile path
(`run-tests.yml` → `make -C src/makeobj`), so lint that's quiet
locally under cmake can fail CI.  Reproduce with `autoconf &&
./configure && CC=clang CXX=clang++ make -C src/makeobj` after a
clean.

`clang-tidy --checks='-*,misc-include-cleaner' -p build <file>.cc`
flags two warning classes: unused direct includes ("is not used
directly") and missing direct includes ("no header providing X is
directly included").  The cleanup is honest only if both halves
run together — removing unused without adding missing just shifts a
file's dependencies further onto transitive includes, not less.
Either half on its own is mechanical divergence from upstream
without a hex-port reason, and the tree relies on transitive
includes pervasively, so a mass sweep just enlarges the merge
surface.  Useful for spot-checking a file you're already rewriting,
or before submitting an upstream-bound PR; not as a working-tree
cleanup pass.

`tools/test.py` runs the scenario suite end-to-end against pak64 — it
fetches pak64 on first invocation (~30 MB, cached) and skips setup it
already did, so warm runs are dominated by sim startup. Pass substring
filters to narrow the run: `tools/test.py halt` runs only tests whose
name contains "halt". This is the cmake-Debug fast loop.

The harness launches the engine with `-fast` (fast-forward + uncapped
`max_acceleration`, so `idle_time` collapses to 0 and sim steps run
back-to-back instead of at the ~5 steps/s NORMAL throttle of
`3200/time_multiplier` in `karte_t::step`). `run-automated-tests.sh`
defaults this on; `SIM_FAST= tools/test.py …` opts back into the
throttle for comparison. Most tests run synchronously inside the
scenario's `start()` and never wait on game time, but the
`sleep()`-bound convoy tests in `test_transport` / `test_way_road`
cost ~65s under the throttle, and `-fast` drops the full suite from
~75s to ~3.5s. Fast-forward is the *more* reproducible mode for these:
the loop advances game time in fixed 100ms quanta (`sync_step(100)` in
`karte_t::interactive`), whereas NORMAL derives `delta_t` from
wall-clock × `time_multiplier` (`interrupt_check` in `simintr.cc`), so
the tick sequence the tests observe is machine-speed-independent under
`-fast`. Assertions are unchanged; only timing moves.

CI (`.github/workflows/run-tests.yml`) runs the same scenario suite
under clang+ASAN+UBSAN on every push, so any hex regression surfaces
there. The local fast loop trades sanitizer coverage for speed; reach
for the autoconf+ASAN recipe in `documentation/claude-code-web-dev.md`
when reproducing a sanitizer-class CI failure. Real-game GUI play is
still not testable in this env; flag assumptions that depend on a
human at the keyboard.

`tools/city_anisotropy.py` measures how non-round city footprints come
out under the hex grid (see `TODO.md` → "City growth anisotropy").  It
maps grown-city tiles through the flat-top basis and reports the
gyration tensor in both the axial and physical frame, separating the
basis shear from genuine algorithmic bias.  `--run` generates a flat
map via the `-flatmap N` hook in `simmain.cc` (a gated test flag in the
default-map path; needs the headless none-backend build) and grows
cities on it; `--series` grows a spread of city sizes and correlates the
m=2 angular mode against footprint size (a scale-invariant m=2 floor is
intrinsic street texture, a decaying one is founding-skeleton bias);
`--self-test` validates the math with no engine.

`tools/nwc_protocol_test/` is the multiplayer wire-protocol suite —
black-box tests that spawn a headless server, send one hand-rolled
NWC_* packet, and assert on the parsed reply. Plain `unittest`,
discovered from the package root: `python3 -m unittest discover -s
tools/nwc_protocol_test -t .` (add `-k <pattern>` to filter, or name a
module like `tools.nwc_protocol_test.test_auth_player` to run one
group). The same step in CI also runs under ASAN/UBSAN; sanitizer
hits on forged packets fail loudly.

`src/fuzz/fuzz_nettool.cc` is the libFuzzer harness for the wire
parser as it is compiled in the standalone `nettool` binary
(`NETTOOL=1`).  It drives the production `recv()` end to end via a
`socketpair()` — the fuzz bytes go in one end, `packet_t(SOCKET)`
reads them out the other and dispatches on the wire id to the
matching `nwc_*::rdwr`.  No parallel parser; what the fuzzer
exercises is exactly what nettool runs against a remote peer,
including the incremental-read state machine inside `recv()`.  Build
with clang + `-DSIMUTRANS_BUILD_FUZZERS=ON` and the existing
sanitizer flags; the target is parser-only (nettool source subset,
no `karte_t`).  CI replays `src/fuzz/corpus/nettool/` through it in
`-runs=0` mode — the committed corpus is generated smoke seeds plus
minimized bug reproducers (each repro pins one fixed defect), and any
crash / leak / sanitizer hit fails the job.  Active mutation is
out-of-band: the same binary takes `-max_total_time=N corpus/` to
explore, and a crash gets minimised via `-minimize_crash=1 -runs=...
crash-input` and committed under `src/fuzz/corpus/nettool/`.  The
corpus-commit policy (what goes in, what stays out-of-band) is in
`documentation/fuzz-corpora.md`.  The
ingame multiplayer server links the full source set rather than the
NETTOOL subset; its wire surface is covered by `fuzz_command_preauth`
(below).

`src/fuzz/fuzz_pak.cc` is the libFuzzer harness for the pak
descriptor reader.  The fuzz bytes are wrapped in a `FILE*` via
`fmemopen` and handed to `pakset_manager_t::load_pak_from_fp` — the
same entry the in-game loader uses per file after `dr_fopen`.  That
covers the magic-prefix scan, the version decoder, and the recursive
node tree that dispatches through `registered_readers` to ~25
`obj_reader_t::read_node` implementations.  The readers self-register
at static init via `OBJ_READER_DEF`, so the type table is populated
before `LLVMFuzzerTestOneInput` runs — but only when the *full*
simutrans source set links, so the fuzz_pak target inherits the
simutrans target's sources via `get_target_property` rather than
duplicating the list.  Needs `-DSIMUTRANS_BACKEND=none` (headless)
and binds the null renderer (`SIMGRAPH_TYPE_NULL`) so the image
reader's `gfx->register_image` doesn't NPE.  Defensive `dbg->fatal`
sites in the readers (e.g. "Cannot handle too new node version")
would normally abort the iteration and drown real findings under
expected aborts; the harness routes them through
`log_t::set_fatal_hook` to a C++ exception that unwinds back to the
per-iteration recovery point, so they read as input-rejection and
only sanitizer-detected bugs fail the run.  Throwing (rather than
`longjmp`) runs the destructors of stack objects live across the
fatal — notably the reader's `node_body` buffer — so they don't leak.
The harness runs with `ASAN_OPTIONS=detect_leaks=1` and calls
`pakset_manager_t::free_all_descriptors()` after each input, which
deletes the whole descriptor DAG and drops the load registries + the
`images_adlers` dedup cache, so the baseline is clean and an active
campaign surfaces only genuinely new leaks (RSS stays flat instead of
growing per iteration).

That teardown is the `TRACK_DESCRIPTORS` build (the
`SIMUTRANS_TRACK_DESCRIPTORS` CMake option, forced on for `fuzz_pak`;
off and zero-cost in the shipping engine and in `makeobj`).  Under the
macro `obj_desc_t` gains a virtual destructor, so `delete` on a tracked
node dispatches to the concrete type (e.g. `image_t::~image_t` frees
`data`) — exhaustively correct for every desc type, with no per-reader
enumeration.  `obj_desc_t::operator new` is the chokepoint that records
each node into a file-local pointer set in `pakset_manager.cc` (so a
desc orphaned by a fatal before `read_nodes` returns is still reclaimed,
and a deduped image node is recorded once); the hooks are free functions,
so `obj_desc.h` keeps no `pakset_manager` dependency.  Deletion is flat,
never recursive through `children`, so no double-free and no stack blow
on deep input.  The same `free_all_descriptors()` is wired into
`simu_main`'s shutdown (after `gfx->exit()`) so a `TRACK_DESCRIPTORS`
engine build can exit leak-clean.

One wrinkle worth knowing: the virtual destructor makes `obj_desc_t`
polymorphic, which would otherwise switch on UBSAN's `-fsanitize=vptr`
for every `get_child<T>` downcast and turn the teardown into an unrelated
type-confusion checker.  `fuzz_pak` compiles with `-fno-sanitize=vptr` to
keep the vtable scoped to destruction; the `get_child` downcast
assumption is tracked separately (see the `get_name()` cluster).

`src/fuzz/fuzz_command.cc` is the libFuzzer harness for network
command *handling* — the post-parse surface (`nwc_tool_t::do_command`
→ `tool->init`/`work`) that `fuzz_nettool` does not reach.  Unlike the
two parser-only harnesses it stands up a real `karte_t` and so needs a
pakset + base files on disk at runtime (`SIMUTRANS_FUZZ_BASE` /
`SIMUTRANS_FUZZ_PAK`; same full-source inheritance + fatal-hook
recovery as fuzz_pak).  It is a POC, not yet in CI; the in-process
per-input world reset is too slow for a real campaign (~2-5
execs/sec), so the planned campaign mode is a fork-after-init engine —
see `TODO.md` for the measurements and next steps.

`src/fuzz/fuzz_command_preauth.cc` is the libFuzzer harness for the
pre-auth network surface of the ingame multiplayer server: wire bytes
through `packet_t::recv()`, then the real `read_from_packet` (the
full ingame dispatch including `nwc_tool_t` / `nwc_chg_player_t` /
`nwc_scenario_*`, which sit outside `fuzz_nettool`'s NETTOOL=1
subset), then for `NWC_TOOL` a direct `init_tool()` call to fire
`create_tool(attacker_tool_id)` and the per-tool `rdwr_custom_data`
overrides — the same path that `nwc_tool_t::clone` runs *before* the
authentication check.  No `karte_t`,
no pakset — `dbg` + null `gfx` and the throwing fatal hook, plus the
minimum global state a real server exposes to the wire parser:
`env_t::server` set (so `nwc_service_t::rdwr` doesn't short-circuit
and the post-rdwr `failed()` paths enable in nwc_nick / nwc_chat /
nwc_sync / nwc_game / nwc_check), and the receiving socket registered
via `socket_list_t::add_client` per iteration (so paths that look the
sender up don't trip an assert).  Runs at ~9000 execs/sec and is the
CI replay gate for the pre-auth attack surface.

`detect_leaks=1` is meaningful here under *both* CI replay and active
mutation, using the same recovery idiom as `fuzz_pak`: the fatal hook
throws (`fuzz_fatal`) and the production parse path is exception-safe,
so an input-driven `dbg->fatal` deep in `rdwr` unwinds the stack
leak-clean rather than leaking the in-flight command.  The load-bearing
production change is in `read_from_packet`: it owns the command in a
`std::unique_ptr` while `receive()` parses (and `receive()` has already
adopted the packet into `nwc->packet`), so a throw mid-`rdwr` frees
both, and the normal receive-failure return frees both without a manual
delete.  The harness then calls the real `read_from_packet` — full
interface, not a reimplemented dispatch — holds the result in its own
`unique_ptr` for the one allocation that lands *after* it returns
(`init_tool`'s tool, which `~nwc_tool_t` frees on unwind), and catches
`fuzz_fatal` at the iteration boundary.  The per-iteration
`socket_list_t::reset()` deletes the client slot so no standing
`socket_info_t` leaks either.  A first-contact finding from this
harness — `nwc_nick_t` / `nwc_chat_t` calling
`get_client(get_client_id(sender))` without guarding the OOB
sentinel — is tracked in `TODO.md`.

Claude Code on the web checks out a shallow clone — `git log` only
reaches back a handful of commits and `git blame` on older lines
returns "(grafted)". Run `git fetch --unshallow origin` when the
question actually needs history (tracing upstream intent, bisecting
across the port commit); default to staying shallow.

The repo is checked out locally at `/home/user/hextrans` (and the
pakset at `/home/user/hextrans-pak128`). Edit and inspect through
the local working tree — `Read`, `Edit`, `git grep`, `cmake --build`
— not the GitHub API. The MCP `mcp__github__*` tools are for
genuine GitHub operations (PRs, issues, CI status, cross-repo
search); using them for local file reads or edits costs round-trips
and skips the file-tooling that's already wired up.
