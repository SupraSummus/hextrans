# TODO

Live registry of port work still to do. This is NOT documentation
and NOT a changelog — describing finished work, current status, or
recent progress belongs in git history or the code itself, not here.
If an entry becomes outdated, delete it.

paragraphs not lists;
delete resolved items rather than strikethrough;
add new items as you find them.

## Tests pending migration to hex model

Scenario tests that encode square-grid geometry are commented out in
`tests/all_tests.nut` with a short `// foo: HEX-PORT PENDING` tag —
see `AGENTS.md` → "Tests and the hex port".  The function body stays
in the test file with a short header comment.  Entries below list
what's currently skipped and the restoration trigger; remove an
entry here when its test is re-enabled.

**`ASSERT_WAY_PATTERN` family.**  `ASSERT_WAY_PATTERN` matches
built ways against 4-bit-ribi shape matrices on square-axis layouts;
every test using it is blocked on `ribi_t` widening to 6 bits and
the helper learning 6-bit ribi.  Affected:
`test_way_bridge_build_ground`, `test_way_bridge_build_above_way`,
`test_way_road_build_straight / _parallel / _below_powerline /
_crossing / _upgrade_crossing / _bend / _upgrade_downgrade /
_upgrade_downgrade_across_bridge / _cityroad_{build,
upgrade_with_cityroad, downgrade_with_cityroad,
replace_by_normal_road, replace_keep_existing}`,
`test_way_tram_build_{flat, parallel, on_road, across_road_bridge,
across_crossing, in_tunel}`,
`test_way_tunnel_build_{straight, up_down, above_tunnel_slope,
across_tunnel_slope}`, `test_way_tunnel_make_public`,
`test_wayobj_build_{straight, disconnected}`,
`test_wayobj_upgrade_{downgrade, change_owner}`,
`test_wayobj_electrify_depot`, and the two
`test_scenario_rules_allow_forbid_tool_stacked_{rect,cube}` entries.
Several crossing cases additionally need a hex-axis pair to replace
the square-perpendicular setup.

**Per-vertex grid topology.**
`test_building_build_multi_tile_sloped`,
`test_terraform_raise_lower_land_at_water_corner` and
`test_terraform_raise_lower_land_at_water_edge` depend on 4-way
vertex sharing (hex shares 3 per vertex).  The tests' slope-name
aliases survive the encoding change but `grid_raise` still uses
square 4-tile vertex geometry.  Restore after per-vertex height
storage lands.

**Flood-fill / region walkers.**
`test_terraform_raise_lower_water_level` uses a rectangular
`terraform_volcano` scaffold and exercises the `tool_change_water_height`
flood-fill; `test_trees_plant_forest` uses a rectangular selection
for the forest tool.  Both need hex-aware region walkers and
hex-shaped scaffolds.

**Square-axis halt catchment layouts.**
`test_transport_generate_pax_{walked,no_route}` and
`test_transport_{pax,mail,freight}_valid_route` build halts at
coordinates that rely on `nesw` {N,E,S,W} symmetry.  Under hex
neighbours the source/halt/destination geometry doesn't align.
Restore with hex-axis layouts.

**Adjacency-order policy.**
`test_powerline_build_transformer_multiple` relies on
`leitung_t::suche_fab_neighbour` iterating in N-first order so a
transformer adjacent to both a power plant (N) and a coal mine (W)
connects to the producer.  Under hex iteration the mine is found
first.  Needs a real policy choice in `suche_fab_neighbour` (prefer
producers?  prefer nearest?), not a test edit.

## Per-vertex height storage

The most impactful remaining structural move.  `surface_t::grid_hgts`
is already per-vertex on a square `(x+1) × (y+1)` grid; adapt its
indexing to hex vertex topology (each hex tile owns roughly 2
vertices, 3-way shared with neighbours) and rewrite `get_hoehe` /
`set_grid_hgt_nocheck` accordingly.  The topology primitives
(`hex_corner_t`, `vertex_owners()`) already exist in
`dataobj/koord.h`; what remains is the storage layout + accessor +
call-site rewrite.  Unblocks: `surface_t::calculate_natural_slope`,
`surface_t::recalc_transitions`, `grund_t::display_if_visible`
climate transitions, `terraformer.cc`'s 4-corner-to-8-neighbour
bitmask, the `simtool.cc` water-raise flood-fill, and the tests
under "Per-vertex grid topology" above.  All are tagged
`HEX-PORT TODO` in the source — `grep -rn 'HEX-PORT' src/simutrans/`.

## ribi_t widening — 4 bits → 6 bits

64 combinations instead of 16; lookup tables (`backwards[16]`,
`dirs[16]`, `doppelr[16]`, `nesw[4]`, etc.) all grow.  Retire
`koord::nesw[4]` and the `koord::north/south/east/west` direction
constants in the same commit — they exist today only because they
tie to the 4-bit ribi system; `route.cc`'s A* expansion and
`ai_passenger.cc`'s airport ring-builder still iterate `nesw[4]` and
touch only 4 of 6 hex neighbours.  `slope_t::is_way_ns` / `is_way_ew`
collapse to a 3-axis predicate family at the same time.  Way
connectivity, signal logic and station orientation switch from 2
axes (NS, EW) to 3 axes — this is a real gameplay change, not just
a refactor.

## Viewport projection

`display/viewport.cc` projects world `(x, y, z)` to screen using a
2:1 isometric "diamond" transform.  Replace with a hex-aware
projection that KEEPS the isometric angle — the same look-and-feel
as upstream Simutrans, not the top-down projection the spike uses.
That means "flat-top hexes seen at the existing isometric tilt",
not `hex_spike.cc`'s `hex_to_px` / `px_to_hex`; those are only
useful as reference for the cube-round mouse-pick inverse.  The new
projection still has to line up with the existing 2:1 sprite raster
so the botched square-tile art stays legible during the transition.

## Save format version bump

Vertex topology, slope encoding and ribi width have all shifted or
will shift; the on-disk format is incompatible.  Bump the version
once the structural changes settle.  Either reject old saves
cleanly or write a one-shot square→hex converter (hard because 4
corners do not map cleanly to 6).

## Spike polish (optional)

`tools/hex_spike/` does what it set out to do.  If we want it to
remain a regression test for the geometry code as the port
progresses, the spike's `height_at` is more elaborate than necessary
(full barycentric within 6 wedges where nearest-corner shading
would do) and could be simplified for clarity.  Not blocking
anything.
