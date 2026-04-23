# Hex-grid port — TODO

Living list. Items roughly in dependency order; fix the top of the
list before the bottom. See `documentation/hex-grid-plan.md` for the
overall design and `CLAUDE.md` for working ground rules.

## Known regressions from the first port commit

The neighbour table change from 8 square offsets to 6 axial offsets
broke a handful of square-grid algorithms. They compile and don't
crash but produce wrong-for-hex output until the items below land.
All are tagged `HEX-PORT TODO` in the source. To find them:

```sh
grep -rn 'HEX-PORT' src/simutrans/
```

- `surface_t::calculate_natural_slope` — 4-corner / 8-neighbour bitmask
  with `& 7` masking. Output: nonsense slopes near map edges and after
  terraforming.
- `surface_t::recalc_transitions` — climate-corner scan, same shape.
  Output: wrong climate-transition art on every tile.
- `karte_t::raise_lake_to` — the river/lake flood-fill in `simworld.cc`
  uses `our_stage[]` and `from_dir[]` tables whose stored values are
  *neighbour indices*. The neighbour ordering changed (was clockwise
  from NW, now clockwise from E), so saved/in-flight state is
  reinterpreted wrong. Currently only an issue during map generation.
- `simtool.cc` water-raise tool — same `from_dir[]` issue; the
  4-corner-to-8-neighbour mapping `((i>>1)+1)&3` is square-grid math.
- Headland / beach heuristics in `karte_t` — thresholds were tuned for
  8 neighbours; with 6 they need re-tuning.
- `koord::neighbours[i]` indices 3..5 in the seasonal-water tool used
  to mean (s, se, e); they now mean (W, NW, NE). Different neighbours
  get checked.

## Next port moves, in order

### 1. Pin the direction-naming convention (header-only, 0 LoC churn)

Flat-top hexes have **edges** at N, S, NE, SE, NW, SW and **vertices**
at E, SE, SW, W, NW, NE. The legacy code says `north/south/east/west`
and means edges; the spike's `corner6` enum says `N, NE, SE, S, SW, NW`
but those labels don't correspond to flat-top vertex positions
(see the NOTE in `tools/hex_spike/hex_spike.cc`).

Decide once, document in a header, and use the same labels everywhere.
Without this every later commit risks silent 30°-off bugs.

### 2. Per-vertex height storage (the spike's #1 finding)

`surface_t::grid_hgts` already exists as per-vertex height storage on a
square `(x+1) × (y+1)` grid. Adapt its **indexing** to the hex vertex
topology (~2 vertices owned per tile, 3-way sharing) and convert
`get_hoehe` / `set_grid_hgt_nocheck` accordingly.

Once heights are per-vertex, slopes become *derived*, not stored. That
is the prerequisite for everything below.

### 3. `slope_t` rewrite — 4 corners → 6 corners

- 6-corner encoding fits in `uint16_t` (3⁶ = 729). Spike confirmed.
- All 81-entry lookup tables (`flags[81]`, `from_hang[81]`, etc.) need
  6-corner equivalents. Or: drop double-height for the initial port
  (2⁶ = 64 slopes, fits in `sint8`) and add it back later.
- `corner_sw / corner_se / corner_ne / corner_nw` macros and
  `encode_corners(sw, se, ne, nw)` to be replaced with 6-corner
  equivalents.
- `slope_t::rotate90` becomes `slope_t::rotate60`.

This commit unblocks every `HEX-PORT TODO` listed above.

### 4. `ribi_t` widening — 4 bits → 6 bits

- 64 combinations instead of 16; lookup tables (`backwards[16]`,
  `dirs[16]`, `doppelr[16]`, `nesw[4]`, etc.) all grow.
- Once ribi is 6-bit, retire `koord::nesw[4]` and the
  `koord::north/south/east/west` direction constants — they exist
  today only because they're tied to the still-4-bit ribi system.
- Way connectivity / signal logic / station orientation switch from
  2 axes (NS, EW) to 3 axes (NS-SW-NE / etc. depending on chosen
  vertex/edge naming). This is a real gameplay change.

### 5. Pathfinding & heuristic audit

`koord_distance` is now hex distance for all 86 callers. Most are
A* heuristics where switching from Manhattan to hex distance is
benign or improves accuracy, but some are "is target in range?"
checks where the range now means a different shape. Audit each call
site and flag any that depend on the old square-distance semantics.

### 6. Viewport projection

`display/viewport.cc` projects world `(x, y, z)` to screen using a 2:1
isometric "diamond" transform. Replace with a flat-top hex projection.
Mouse-picking inverse needs the standard fractional-axial → cube → round
routine. The spike has a working reference implementation in
`tools/hex_spike/hex_spike.cc` (`hex_to_px`, `px_to_hex`).

### 7. Save format version bump

Once vertex topology / slope encoding / ribi width have changed, the
on-disk save format is incompatible. Bump the version; either reject
old saves or write a one-shot square→hex converter (the latter is
hard because 4 corners do not map cleanly to 6).

## Out of scope (deferred)

- **Pak art.** Reuse legacy square-tile sprites botched onto the hex
  grid. Porting / regenerating sprites is a separate, much larger
  effort and follows the engine work — see CLAUDE.md.
- **FCC z-stacking.** Decision locked to right-prism; do not revisit
  unless we add a feature whose semantics actually want 3-up / 3-down
  branching. See CLAUDE.md for the reasoning.
- **Parallel coord types / `#ifdef HEX` flag.** Decision locked to
  in-place port. See CLAUDE.md.

## Spike cleanup

`tools/hex_spike/` did its job (surfaced the per-vertex-storage
finding) but is not elegant code:

- `corner6` labels (`N, NE, SE, S, SW, NW`) don't match flat-top
  vertex positions; the file even admits this. Either fix the labels
  to match the actual angles (`E, SE, SW, W, NW, NE`) or use neutral
  names (`C0..C5`).
- `corner_offset()` has a "see note" comment because the labels
  contradict the positions — fix or rename.
- `height_at` triangulates into 6 wedges with barycentric
  interpolation; for the question the spike was answering, simpler
  shading (e.g. nearest-corner height) would have done.
- The committed `hex_spike.png` is a binary build artifact — drop it
  and have `make run` regenerate.
- Diagnostics are mostly `printf` instead of asserts; convert to
  `assert(...)` so the spike is a real test that fails loudly when
  geometry breaks.

Optional — only worth doing if we want the spike to remain a
regression test for the geometry code as the port progresses.

## Process

- One port commit per logical layer; each commit must compile and
  the binary must boot.
- Tag broken-but-compiling areas with `HEX-PORT TODO` and add a line
  here so we don't lose track.
- No pakset is available in CI / the web sandbox; "binary boots" is
  the only automated signal. End-to-end behaviour has to be checked
  on a developer machine with a pakset installed.
