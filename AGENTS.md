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
the change that landed it, not here.

The aim is for `TODO.md` to remain a useful, current map of "what
still needs doing". A growing TODO file is fine; a stale one is not.

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
`lower_hex_tile{,_pair_S}` reverse the lift. The square-era
4-grid_raise scaffold (raise the 4 NW corners of a 2x2 grid square)
does not produce a flat hex tile — it hits only 4 of the 6 vertices,
leaving an alternating-corner slope.

`way_desc_t::has_double_slopes()` reads an explicit `has_double_slopes=`
.dat key (way_desc save version 9); paksets opt each way in or out
independently of which slope sprites they ship.  Pak64 doesn't set it,
so every test-suite way answers `false` and a second consecutive
`setslope(all_down)` on the same underground tunnel tile fails with
"Tile not empty".  Tests that need a 2-step underground staircase
(`test_way_tunnel_build_above_tunnel_slope`, `_across_tunnel_slope`)
stay disabled until pak64 sets the flag — see TODO.md → "Tunnel tests
blocked on `has_double_slopes=1` opt-in".

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
    it.  Live coverage + one residual outlier still to migrate.

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

`tools/test.py` runs the scenario suite end-to-end against pak64 — it
fetches pak64 on first invocation (~30 MB, cached) and skips setup it
already did, so warm runs are dominated by sim startup. Pass substring
filters to narrow the run: `tools/test.py halt` runs only tests whose
name contains "halt". This is the cmake-Debug fast loop.

CI (`.github/workflows/run-tests.yml`) runs the same scenario suite
under clang+ASAN+UBSAN on every push, so any hex regression surfaces
there. The local fast loop trades sanitizer coverage for speed; reach
for the autoconf+ASAN recipe in `documentation/claude-code-web-dev.md`
when reproducing a sanitizer-class CI failure. Real-game GUI play is
still not testable in this env; flag assumptions that depend on a
human at the keyboard.

`tools/nwc_protocol_test/` is the multiplayer wire-protocol suite —
black-box tests that spawn a headless server, send one hand-rolled
NWC_* packet, and assert on the parsed reply. Plain `unittest`,
discovered from the package root: `python3 -m unittest discover -s
tools/nwc_protocol_test -t .` (add `-k <pattern>` to filter, or name a
module like `tools.nwc_protocol_test.test_auth_player` to run one
group). The same step in CI also runs under ASAN/UBSAN; sanitizer
hits on forged packets fail loudly.

`src/fuzz/fuzz_network.cc` is the libFuzzer harness for the wire
parser.  It drives the production `recv()` end to end via a
`socketpair()` — the fuzz bytes go in one end, `packet_t(SOCKET)`
reads them out the other and dispatches on the wire id to the
matching `nwc_*::rdwr`.  No parallel parser; what the fuzzer
exercises is exactly what the in-game network thread runs against a
remote peer, including the incremental-read state machine inside
`recv()`.  Build with clang + `-DSIMUTRANS_BUILD_FUZZERS=ON` and the
existing sanitizer flags; the target is parser-only (nettool source
subset, no `karte_t`).  CI replays `src/fuzz/corpus/network/` through
it in `-runs=0` mode — each committed corpus file pins one fixed bug
as a regression and a new crash there fails the job.  Active
mutation is out-of-band: the same binary takes `-max_total_time=N
corpus/` to explore, and a crash gets minimised via
`-minimize_crash=1 -runs=... crash-input` and committed under
`src/fuzz/corpus/network/`.

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
