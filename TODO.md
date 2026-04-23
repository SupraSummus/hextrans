# TODO

Live list of port work in flight. See `AGENTS.md` for rules on how
to use this file (paragraphs not lists; delete resolved items rather
than strikethrough; add new items as you find them).

Items are roughly ordered by dependency — earlier items unblock later
ones. The overall design is in `documentation/hex-grid-plan.md`.

## Tests pending migration to hex model

The scenario suite under `tests/` still runs on CI. As the port moves
forward, tests that encode square-grid geometry get skipped rather
than deleted when their invariant is valuable but their assertions are
square-specific. See `AGENTS.md` → "Tests and the hex port" for the
classification rules. Items below are the currently-skipped tests; the
function bodies stay in place in their test files with a header
comment explaining the invariant and what needs to change. Remove an
item here when its replacement test has been written and added back to
`all_tests.nut`.

`test_building_build_multi_tile_sloped` in `tests/tests/test_building.nut`
asserts that build-then-remove on sloped terrain restores the natural
slope. The invariant survives the port but the assertions encode 4-way
vertex sharing (hex shares 3 per vertex) and 4-corner slope names
(`southeast`/`southwest`/`northeast`/`northwest`). Restore after
`slope_t` becomes 6-corner.

## Known regressions from the first port commit

The neighbour-table change from 8 square offsets to 6 axial offsets
broke a handful of square-grid algorithms; they compile and don't
crash but produce wrong-for-hex output until the slope and ribi work
below lands. All affected sites are tagged `HEX-PORT TODO` in the
source — `grep -rn 'HEX-PORT' src/simutrans/` to find them. The
clusters worth being aware of: `surface_t::calculate_natural_slope`
and `surface_t::recalc_transitions` (4-corner / 8-neighbour bitmask
with `& 7` masking, climate transitions wrong on every tile);
`karte_t::raise_lake_to` and the `simtool.cc` water-raise tool
(flood-fill state machines whose stored neighbour-index values are
now reinterpreted under a different ordering); the headland and
beach heuristics in `karte_t` (numeric thresholds tuned for 8
neighbours need re-tuning for 6); and the seasonal-water tool slice
that hardcoded indices 3..5 to mean (s, se, e), which under the new
ordering means (NW, N, NE) instead.

## Per-vertex height storage

The single most important next port move. `surface_t::grid_hgts`
already exists as per-vertex height storage on a square `(x+1) ×
(y+1)` grid. Adapt its indexing to the hex vertex topology — each
hex tile owns roughly 2 vertices, with 3-way sharing across
neighbours — and convert `get_hoehe` / `set_grid_hgt_nocheck`
accordingly. Once heights are per-vertex, slopes become derived
rather than stored, which is the prerequisite for the slope rewrite
below.

## slope_t rewrite — 4 corners → 6 corners

6-corner encoding fits in `uint16_t` (3⁶ = 729); the spike confirmed
this. All 81-entry lookup tables (`flags[81]`, `from_hang[81]`, etc.)
need 6-corner equivalents, or we drop double-height for the initial
port (2⁶ = 64 slopes, fits in `sint8`) and add it back later. The
`corner_sw` / `corner_se` / `corner_ne` / `corner_nw` macros and
`encode_corners(sw, se, ne, nw)` need 6-corner equivalents.
`slope_t::rotate90` becomes `slope_t::rotate60`. This commit unblocks
every `HEX-PORT TODO` listed above.

## ribi_t widening — 4 bits → 6 bits

64 combinations instead of 16; lookup tables (`backwards[16]`,
`dirs[16]`, `doppelr[16]`, `nesw[4]`, etc.) all grow. Once ribi is
6-bit, retire `koord::nesw[4]` and the `koord::north/south/east/west`
direction constants — they exist today only because they're tied to
the still-4-bit ribi system. Way connectivity, signal logic and
station orientation switch from 2 axes (NS, EW) to 3 axes; this is a
real gameplay change, not just a refactor.

## Pathfinding & heuristic audit

`koord_distance` is now hex distance for all 86 callers. Most are A*
heuristics where switching from Manhattan to hex distance is benign
or improves accuracy, but some are "is target in range?" checks where
the range now means a different shape. Audit each call site and flag
any that depend on the old square-distance semantics.

## Viewport projection

`display/viewport.cc` projects world `(x, y, z)` to screen using a
2:1 isometric "diamond" transform. Replace with a flat-top hex
projection. Mouse-picking inverse needs the standard fractional-axial
→ cube → round routine. The spike has a working reference
implementation in `tools/hex_spike/hex_spike.cc` (`hex_to_px`,
`px_to_hex`).

## Save format version bump

Once vertex topology, slope encoding, or ribi width have changed, the
on-disk save format is incompatible. Bump the version; either reject
old saves cleanly or write a one-shot square→hex converter (the
latter is hard because 4 corners do not map cleanly to 6).

## Spike polish (optional)

`tools/hex_spike/` does what it set out to do. If we want it to
remain a regression test for the geometry code as the port progresses,
the spike's `height_at` is more elaborate than necessary (full
barycentric within 6 wedges where nearest-corner shading would do)
and could be simplified for clarity. Not blocking anything.
