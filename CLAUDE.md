# CLAUDE.md

## Project context

This fork of Simutrans is transitioning from the upstream square-tile
grid to a **hexagonal grid**. See `documentation/hex-grid-plan.md` for
the original design and `tools/hex_spike/findings.md` for what a
30-minute prototype surfaced.

## Approach

- **Flat-top axial coords `(q, r)`**, byte-compatible with the existing
  `koord` struct's `(x, y)`. The port is therefore semantic — we
  redefine what `koord` means and rewrite the operations on it; storage
  layout, `planquadrat_t* plan` indexing, and save-file format all stay.
- **Hexagonal right prism for z-stacking**, *not* FCC close-packing.
  "Up" stays a single-cell direction so bridges, tunnels, pillars and
  multi-level stations keep working.
- **No parallel types, no compile-time `HEX` flag.** We change the
  existing `koord` / `ribi` / `slope_t` in place, accepting a transient
  period where the codebase compiles but parts of game logic are
  inconsistent. This is a real port, not an experimental side-branch.
- **Art is out of scope for now.** Reuse the existing square-tile
  sprites botched onto the hex grid; porting/regenerating pakset art is
  a separate, much larger task that follows the engine work.

## Critical findings driving priority

1. **Per-vertex height storage is the top of the critical path.** A hex
   vertex is shared by **3 tiles** (vs 4 for a square corner). Storing
   slopes per-tile, like the legacy code does today, will produce
   inconsistent terrain across shared vertices the moment terraforming
   touches it. The fix is to store heights per-vertex and *derive*
   slopes — the same pattern as the existing `surface_t::grid_hgts`
   array, but with a denser, irregular vertex topology. This has to be
   designed before the slope and terraform code can be ported.
2. **Direction naming is a silent-failure landmine.** Flat-top hexes
   have *no* N or S corner — those are edges. Six corners sit at angles
   0/60/120/180/240/300° (`E SE SW W NW NE`). Every existing
   `north/south/east/west` constant in the codebase has to be retired
   or carefully repurposed; getting it wrong gives you a 30°-off grid
   that compiles fine.
3. The 6-corner slope encoding fits in `uint16_t` (3⁶ = 729). Slope
   width is the *easy* part; the spike confirmed that.

## Status

- `documentation/hex-grid-plan.md` — design plan.
- `documentation/claude-code-web-dev.md` — how to compile + test inside
  Claude Code on the web.
- `tools/hex_spike/` — geometry spike + findings (no production code).
- Engine port: in progress. See git log for the porting commits.

## Working notes

- Compile from the repo root: `cmake --build build -j "$(nproc)"`. The
  session-start hook configures `build/` for you.
- The build cannot be tested end-to-end without a pakset (see
  `documentation/claude-code-web-dev.md`). For now, "the binary
  compiles and launches" is the only signal we have. Behaviour of the
  ported codebase under a real game cannot be validated in this env;
  flag any assumption that depends on running the game.
