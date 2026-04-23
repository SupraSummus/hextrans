# TODO

Live list of port work in flight. See `AGENTS.md` for rules on how
to use this file (paragraphs not lists; delete resolved items rather
than strikethrough; add new items as you find them).

Items are roughly ordered by dependency — earlier items unblock later
ones. Design decisions driving the ordering are summarised in
`AGENTS.md`.

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

`test_terraform_raise_lower_land_at_water_corner` and
`test_terraform_raise_lower_land_at_water_edge` in
`tests/tests/test_terraform.nut` assert that raise/lower at a water-body
corner or edge converts water/land in a predictable pattern. The
invariant survives the port but the assertions walk the 2×2 block of
tiles meeting at one grid vertex (square 4-way corner share) and name
each operation by a 4-corner or 4-edge label. Hex vertices are shared
by 3 tiles and hex has 6 edge directions. Restore together after
per-vertex height storage lands.

`test_terraform_raise_lower_water_level` in
`tests/tests/test_terraform.nut` exercises the water-height tool. The
test scaffolds with `terraform_volcano` (a rectangular raised
perimeter) and asserts that the flood-fill in
`tool_change_water_height` correctly floods / drains the ring's
interior. The invariant is valuable under hex, but both the scaffold
and the flood-fill itself are square-grid — the flood-fill is one of
the known HEX-PORT regressions tagged in `simtool.cc`. Restore after
that flood-fill is ported to hex neighbours AND the scaffold is
rewritten to use a hex-shaped containing ring.

`test_trees_plant_forest` in `tests/tests/test_trees.nut` asserts that
the forest tool plants trees inside the selected rectangle and nothing
outside. The invariant is valuable but the selection is a 2D rectangle
and the inside/outside predicate is rectangular. Restore after the
forest tool's region walker is ported to hex iteration and the
selection shape is replaced with a hex-shaped region.

`test_way_bridge_build_ground` and `test_way_bridge_build_above_way` in
`tests/tests/test_way_bridge.nut` assert that bridges can be built —
first on flat ground, then spanning a perpendicular way — and produce
the expected way patterns. The invariants survive the port, but the
endpoints define square-axis lines, the way-pattern matrices encode
4-bit ribi, and `build_above_way` also sets up bridgeheads with
4-corner slope names (`slope.south`, `slope.north`). Restore after
`ribi_t` widens to 6 bits, `slope_t` becomes 6-corner, and
`ASSERT_WAY_PATTERN` handles 6-bit ribi.

`test_way_road_build_straight`, `test_way_road_build_parallel`,
`test_way_road_build_below_powerline`, `test_way_road_build_crossing`
and `test_way_road_upgrade_crossing` in `tests/tests/test_way_road.nut`
assert road patterns, T-junction connectivity, build-under-powerline
rules and road/rail crossing semantics against 4-bit ribi matrices on
square-axis layouts. The crossing tests additionally hardcode a
rail/road perpendicular pair (square-axis orthogonality) that has no
direct hex equivalent. Same problem shape as the bridge tests. Restore
alongside them after `ribi_t` widens and `ASSERT_WAY_PATTERN` handles
6-bit ribi; the crossing cases also need a chosen pair of hex axes to
replace the square-perpendicular setup.

`test_way_tram_build_parallel` in `tests/tests/test_way_tram.nut`
asserts that 16 parallel tram rails along one square axis stay
independent and match two specific 16×16 ribi matrices. Same problem
shape as the road tests — 4-bit ribi on a square-axis layout. Restore
after `ribi_t` widens, `ASSERT_WAY_PATTERN` handles 6-bit ribi, and a
hex axis is chosen for the parallel-rails layout.

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
now reinterpreted under a different ordering); and the headland and
beach heuristics in `karte_t` (numeric thresholds tuned for 8
neighbours need re-tuning for 6).

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
the still-4-bit ribi system; the A* expansion in `route.cc` and the
airport ring-builder in `ai_passenger.cc` both currently iterate
`nesw[4]` and so touch only 4 of 6 hex neighbours, and get swept in
this same commit. Way connectivity, signal logic and station
orientation switch from 2 axes (NS, EW) to 3 axes; this is a real
gameplay change, not just a refactor.

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
