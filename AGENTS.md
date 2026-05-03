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

Pakset art is out of scope for now. Reuse the existing square-tile
sprites where they still fit and use small engine-generated placeholder
sprites (e.g. synthetic hex ground) where square art hides engine
geometry; porting/regenerating real pakset art is a separate, much
larger task that follows the engine work.

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

`TODO.md` tracks port work in flight. Working rules:

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

## Working notes

Compile from the repo root: `cmake --build build -j "$(nproc)"`. The
session-start hook configures `build/` for you.

The build cannot be tested end-to-end without a pakset (see
`documentation/claude-code-web-dev.md`). For now, "the binary compiles
and launches" is the only signal we have. Behaviour of the ported
codebase under a real game cannot be validated in this env; flag any
assumption that depends on running the game.

CI (`.github/workflows/run-tests.yml`) does have a pakset — it
installs pak64 and runs the full scenario suite under clang+ASAN+UBSAN
on every push, so any hex regression will surface there. To reproduce
a CI failure locally, see `documentation/claude-code-web-dev.md` →
"Running automated tests".

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
