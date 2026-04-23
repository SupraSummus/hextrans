# AGENTS.md

This file is the canonical agent-facing project doc. `CLAUDE.md` in the
repo root is a symlink pointing at this file — both names load the same
content. Edit `AGENTS.md`. The symlink exists so Claude Code (which
reads `CLAUDE.md` by default) keeps working without forking the
content; the symlink is checked into git, so a fresh clone gets it for
free. Mentioning it explicitly here because filesystem symlinks are
easy to miss when grepping or browsing the tree.

## Project context

This fork of Simutrans is transitioning from the upstream square-tile
grid to a **hexagonal grid**. See `tools/hex_spike/findings.md` for
what a 30-minute prototype surfaced, and **`TODO.md` for the live list
of items still to do**. Key design decisions (flat-top hex,
right-prism z-stacking, in-place port) are summarised below.

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

Art is out of scope for now. Reuse the existing square-tile sprites
botched onto the hex grid; porting/regenerating pakset art is a
separate, much larger task that follows the engine work.

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
denser, irregular vertex topology. This has to be designed before the
slope and terraform code can be ported.

The 6-corner slope encoding fits in `uint16_t` (3⁶ = 729). Slope width
is the easy part; the spike confirmed that.

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

## Tests and the hex port

The scenario tests under `tests/` encode gameplay invariants. Many of
those invariants survive the hex port, but the specific assertions
(coord choices, direction constants, slope enum names, neighbour
counts) bake in the square-grid model. When the port breaks a test,
do not paper over the regression by preserving legacy square data
structures — parallel neighbour tables, `#ifdef HEX` forks, or silent
auto-conversion shims. That is parallel types in spirit even if not in
name, and contradicts the in-place port approach above. Red CI during
the port transition is more honest than green CI propped up by shims.

Per-test, decide:

*Migrate* when the invariant survives the port and only the assertions
are square-specific (e.g. asserts 4-way vertex sharing, 4-corner slope
names, 8-way neighbour iteration). Remove the entry from
`tests/all_tests.nut`, keep the function body in its test file with a
header comment that names the invariant and explains what needs to
change under hex, and add a paragraph to `TODO.md` listing it as
pending with the trigger for restoration (typically "after slope_t is
6-corner" or "after ribi widens to 6 bits"). The function is left in
place as part of the live migration plan — while the port is in
flight it is useful reference for the hex rewrite; delete the function
together with the TODO entry at restoration time.

*Delete* when the invariant is a purely square-grid geometric property
— e.g. a test that asserts the exact shape of an 8-neighbour climate
transition mask. Remove the function and the `all_tests.nut` entry
together. No TODO.md entry either; git history has the record.

*Fix* when the test is correct and the code is the regression. Fix the
code.

Test isolation caveat: the scenario runner does not reset world state
between tests. A failure mid-test cascades into every subsequent test
that expects a clean map (via `RESET_ALL_PLAYER_FUNDS`' maintenance
check, leftover buildings, raised grid points, etc.). When triaging a
large failing set, expect most failures to be cascades from a smaller
number of actual hex regressions. Work the cascade head first, rerun,
and see what is left before classifying the tail.

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

Claude Code on the web checks out a shallow clone, so `git log` only
reaches back a handful of commits and `git blame` on older lines
returns "(grafted)". If you need real history — tracing a square-grid
convention to its upstream intent, bisecting a regression across the
port commit, or just following a function's evolution — run `git
fetch --unshallow origin` first. Default to staying shallow; unshallow
only when the question actually needs the history.
