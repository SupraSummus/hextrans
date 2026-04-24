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
built ways against 4-bit-ribi shape matrices on square-axis layouts.
`ribi_t` is now 6 bits; the remaining blocker is the Squirrel-side
helper learning 6-bit ribi and gaining hex-axis shape matrices.
Affected: `test_way_bridge_build_ground`, `test_way_bridge_build_above_way`,
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

**Script `dir` class port.**  `simutrans/script/script_base.nut`
still declares the `dir` class with old 4-bit values (`north=1,
east=2, south=4, west=8, northsouth=5, eastwest=10, ...`) — these
no longer match the C++ `ribi_t::_ribi` bit layout (SE=1, S=2,
SW=4, NW=8, N=16, NE=32).  Any script that references
`dir.northsouth`, `dir.northeast`, `dir.all`, etc. silently gets
the wrong ribi.  Port the class to hex values and restore the 9
`test_dir_*` tests (`_is_single / _is_twoway / _is_threeway /
_is_curve / _is_straight / _double / _backward / _to_slope /
_to_coord`).  Some test bodies will need rewriting rather than
restoring — old assertions like `ASSERT_EQUAL(dir.double(dir.east),
dir.eastwest)` bake the 2-axis model that hex's 3 axes don't
satisfy.

**Slope iteration over the full 6-corner space.**
`test_depot_build_sloped`, `test_depot_build_on_tunnel_entrance`,
`test_halt_build_rail_single_tile`, `test_halt_build_multi_tile`,
`test_slope_to_dir`, `test_slope_max_height_diff`,
`test_way_bridge_build_at_slope` (and `_stacked` / `_above_runway` /
`_planner`) all iterate over slope integers or exercise bridge
geometry that assumed 4-corner slopes.  Under hex the slope space
is 3^6 = 729 values; many are double-height and setslope returns
"" for pakset without `double_grounds`.  Restore after the slope-
iteration helpers are hex-aware (walk only single-height subsets,
use hex corner names) and bridge-over-slope geometry is ported.

**Powerline 3rd hex axis.**  `test_powerline_connect / _build_below_powerbridge /
_build_powerbridge_above_powerline / _build_transformer_multiple /
_remove_powerbridge / _ways` each expect crossings / powerlines
on the 2 square-era axes (N-S and old E-W).  Under hex there are
3 axes and the 3rd (NE-SW) has no powerline crossing sprite or
connection FSM support (`leitung2.cc` diagonal-image table is keyed
on 4 old-combo values).  Restore after the crossing-cluster /
3rd-axis work lands.  `_transformer_multiple` additionally depends
on `leitung_t::suche_fab_neighbour`'s adjacency order — see
"Adjacency-order policy" below.

**Sign / traffic-light 2-axis FSM.**  `test_sign_build_oneway /
_build_trafficlight / _remove_trafficlight / _build_private_way /
_build_signal / _build_signal_multiple / _replace_signal /
_signal_when_player_removed` bake the 2-phase traffic-light FSM
(state 0 = N-S axis, state 1 = old-E-W) and 4-direction sign
rotation layouts from `roadsign.cc`.  Under hex, 3 axes / 6
rotations.  Real gameplay design choice, not a test edit; restore
when `roadsign_t` and the trafficlight info UI get their hex port.

**Runway layout.**  `test_way_runway_build_rw_flat / _tw_flat /
_mixed_flat` bake a 4-direction airport layout (runway + taxiway
cross at 90°).  The `ai_passenger.cc` airport builder was ported
to a hex diamond (taxiway crosses on N-S and old-E-W = hex SE-NW,
with 4 of 6 hex edges used), but the runway/taxiway geometry
tests assume the square 3x3.  Restore after a proper hex airport
layout is designed.

**Rect/cube scenario scaffolds.**  `test_scenario_rules_allow_forbid_way_tool_rect /
_cube` use square-coordinate rect/cube selection regions to verify
scenario rule coverage.  Under hex the region shapes are different;
restore after the region-selection tools are hex-aware.

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

**Adjacency-order policy.**
`test_powerline_build_transformer_multiple` relies on
`leitung_t::suche_fab_neighbour` iterating in N-first order so a
transformer adjacent to both a power plant (N) and a coal mine (W)
connects to the producer.  Under hex iteration the mine is found
first.  Needs a real policy choice in `suche_fab_neighbour` (prefer
producers?  prefer nearest?), not a test edit.

## Per-vertex height storage — remaining writer-side ports

Storage is per-hex-vertex (see `documentation/hex-vertex-storage.md`,
`surface_t::grid_hgts`): two canonical slots (E, SE) per tile, plus
boundary padding.  `perlin_hoehe` now samples at world-vertex
positions and writes both canonical slots, so freshly generated
terrain is self-consistent across shared vertices — the three owners
of any shared vertex all resolve to the same slot and get the same
noise value by construction.

Other writers still use the legacy `set_grid_hgt_nocheck(x, y)` API
(which lands in the E slot only) and need porting next: `terraformer.cc`'s
raise/lower bitmask (still 4-corner-to-8-neighbour, tagged `HEX-PORT TODO`);
the `simtool.cc` water-raise flood-fill (square 4-neighbour); the
heightfield-load path in `karte_t::init_tiles` (replicates the last
square-grid row into the doubled slot layout — needs a hex-aware
importer or a clean rejection);  `karte_t::rotate90`'s heightmap-rotation
loop (90° is not a valid hex symmetry; the whole rotation path needs
a refusal or a real hex rotation, tied to the viewport port).
`perlin_hoehe`'s own rotation code is still the legacy 90° square
formula — it produces a deterministic but geometrically wrong map
when rotation != 0; fix in the same pass as `rotate90`.

The `lookup_hgt(x, y)` / `set_grid_hgt_nocheck(x, y)` shim in
`surface.h` routes every `(x, y)` to the E canonical slot.  It is
harmless today (old writers + old readers stay internally
consistent), but once every writer and reader is ported to the
hex-aware `(tile, corner)` overloads the shim will silently drop
SE data on the floor.  Retire it at the end of the
writer-and-reader port, not before.

Remaining hex-aware readers still 4-corner: `recalc_natural_slope`
with its `get_neighbour_heights[8][4]` scaffold, and the
climate-transition bitmask in `recalc_transitions` /
`grund_t::display_if_visible`.  Both are bigger than the readers
that already ported — they don't just read six heights, they walk
six neighbours and compose per-corner data.  Sketch the 6-corner
equivalent before diving in.

## ribi_t — audit surfaces

These are the shim / stub patterns spread across the caller port
that need a second sweep once their trigger condition lands.  Each
is named / tagged so a global grep surfaces all sites.

**`ribi_t::rotate_for_map_rotate90` sweep.**  Every `obj_t::rotate90()`
override that holds a ribi (ways, vehicles, signs, water flow,
wind direction) calls `ribi_t::rotate_for_map_rotate90(x)`, currently
stubbed to `rotate60(x)` — "one step forward when the world ticks
one step forward", the same single-bit-rotation semantic the old
4-ribi `rotate90` had on a 4-direction grid.  Triggered by the
`karte_t::rotate90` decision (refuse map rotation under hex?  rewrite
to 60°?).  When that lands, update the helper body in one place (or
delete the helper and all callers).

**`ribi_t::rotate_perpendicular` / `_l` sweep.**  Square-era "90°
off this direction" sites — crossroads collision avoidance
(`road_vehicle.cc`, `pedestrian.cc`, `vehicle_base.cc`),
broad-tunnel side tiles (`tunnel.cc`, `tunnelbauer.cc`), canal
orthogonality (`wasser.cc`), diagonal-bend detection (`weg.cc`,
`wayobj.cc`).  Also stubbed to `rotate60` (the "one step over" axis).
Triggered by the crossing-cluster port: per-site review, some may
want both 60° and 120° (test both adjacent axes), some may redesign
the check entirely for hex 3-axis geometry.

**Old-east→hex-SE, old-west→hex-NW rename convention.**  ~30+ sites
in rendering, signs, and leaf files mechanically renamed
`ribi_t::east`→`ribi_t::southeast` and `ribi_t::west`→`ribi_t::northwest`.
The rename is legitimate *under the current 2:1 isometric viewport*
(both names refer to the same axial displacement vector).  When the
viewport port lands and the projection changes — or if NE/SW ever
need sprite representations — every rename site needs re-audit.
Grep: `HEX-PORT.*east\|HEX-PORT.*west\|\b(southeast|northwest)\b`
inside rendering-cluster files.

**`ribi_t::{northwest, southeast, northeast, southwest}` silent
semantic shift.**  These names existed before the structural flip
as 4-bit combo constants (`northwest` = N|W = 9, `northeast` = N|E
= 3, `southeast` = S|E = 6, `southwest` = S|W = 12).  Under the
hex flip they're now single-edge bits (8, 32, 1, 4 — the 4 hex-only
edges).  Same names, different numeric values, silent in the
compiler.  ~60 call sites using these names need manual audit:
which sites relied on the combo-value semantics (must be changed
to explicit `(ribi_t::a | ribi_t::b)` combos) vs which sites are
actually single-edge tests (already correct).  No easy grep filter
— every occurrence deserves a look.

**`ribi_t::is_perpendicular` 2-axis vs 3-axis.**  Under hex there
is no true 90° axis relation; the current predicate returns true
when x and y together span more than one hex axis (= "different
axes").  ~12 callers use this for collision avoidance, signal
logic, crossing detection.  Each needs review: some want
"different axis" (current semantic fits), some may want "specific
axis pair" for crossings that only care about 2 of 3 hex axes.
Not a silent bug today but the semantic shift from 2-axis to
3-axis is a gameplay choice.

**Vehicle direction enum — compound 2-step displacements.**
`vehicle_base_t::calc_set_direction(start, end)` is called both with
adjacent (1-step) and 2-apart (2-step) position pairs — the caller
decides which based on what it wants, e.g. `calc_set_direction(pos_next,
pos_next_next)` for the "what direction is the vehicle about to move"
query vs `calc_set_direction(get_pos(), pos_next_next)` for the "what
2-tile diagonal is this visually".  Under the square grid that produced
8 displacement values (4 cardinal 1-steps + 4 diagonal 2-steps), which
the old enum captured exactly.  Under hex the space explodes: 6
1-step hex neighbours, 6 "same-direction" 2-steps (2,0)/(2,-2)/...,
12 "turn" 2-steps like (1,1) from S+SE or (2,-1) from NW+SE, and (0,0)
for u-turns.  Around 18 distinct visual states in total, none of which
fit cleanly in the 8-value `ribi_t::_dir` sprite enum either.  The
current port pattern-matches on the sign of (di, dj) and maps to the
closest of the 6 hex neighbour ribis; the (1,1) and (-1,-1) cases
(produced only by S+SE, SE+S, N+NW, NW+N pairs) fall into a defensive
`ribi_t::none` fallback with dy=±2, dx=0.  Downstream consumers
(collision avoidance, sprite offsets, disp_lane) will be visually
wrong in exactly the cases where 2-step compound paths arise.  Proper
fix is a hex-aware vehicle direction model — likely 12 states (6 hex
edges × 2 magnitudes for 1-step vs 2-step) or a distinct
`path_t` that carries both entry and exit edge names — and lands
together with the viewport / sprite port.  The dir enum in ribi.h
similarly needs widening; `get_dir()` currently projects 6 hex edges
onto 4 square sprite slots and drops information.

Additional follow-ups that did NOT land in the structural commit:

`slope_t::is_way_ns` / `is_way_ew` still live in `ribi.h` at the
slope level (not the ribi level) and still split on the 2 legacy
axes.  Collapse to a 3-axis predicate family once the slope-edge
constants for the 4 hex-only edge slopes (NE, SE, SW, NW) land.

Save-file format: `weg_t`'s in-memory ribi is now two full bytes
(was a packed 4-bit bitfield that silently truncated hex bits 4-5),
and the rdwr load path masks against `ribi_t::all` instead of `15`.
The on-disk byte layout is unchanged for now, but old saves load
their 4-bit values into the new low-4-bits slots — which under
hex mean SE/S/SW/NW, not the original N/E/S/W.  Needs a save
version bump + a one-shot converter (or clean rejection) before
any pre-port saved game survives a round-trip.  See also the
"Save format version bump" section below.

`ribi_t::_dir` sprite enum is still 4-direction; `get_dir()` returns
`dir_invalid` for NE/SE/SW/NW.  Tied to sprite port.  See also the
vehicle-direction compound-displacement note above — 18 distinct
visual states under hex, 8 slots in the current dir enum.

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
