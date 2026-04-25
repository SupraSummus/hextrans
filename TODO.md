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

**Square-grid terrain-mutation cascade tests.**
`test_climate_cliff` raises a 3x3 plateau via `setslope all_up_slope`,
exercises `tool_set_climate` (including a partial water fill), then
walks the plateau back down via `setslope all_down_slope`.  The
cleanup setslope returns "Tile not empty." on the second tile of the
top row.  `test_factory_build_pp` / `_with_fields` / `_climate` /
`_on_water_occupied` all build factories at `coord3d(3, 4, 0)` after
the climate-flat / climate-invalid tests run, and get "No suitable
ground!" — the test passes in isolation but cascades after earlier
terrain-mutation tests run.  Both clusters share the same root
cause: the test bodies bake in 4-corner / 8-neighbour terrain
propagation, so a `setslope` or a multi-tile setclimate that under
square model affected exactly the named tiles now under hex 6-edge
propagation reaches one tile further (or stops one short).  The
invariants survive (build-after-flatten, climate-set-on-cliff)
but the specific tile choices and the assertions about which
neighbours are mutated do not.  Restore alongside a hex-aware
rewrite of the propagation patterns these tests probe.

**`ASSERT_WAY_PATTERN` family.**  `ASSERT_WAY_PATTERN` matches
built ways against 4-bit-ribi shape matrices on square-axis layouts.
`ribi_t` is now 6 bits; the remaining blocker is the Squirrel-side
helper learning 6-bit ribi and gaining hex-axis shape matrices.
Affected: `test_way_bridge_build_{ground, above_way, at_slope,
at_slope_stacked, above_runway}`,
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

**Bridge geometry.**  `test_halt_build_multi_tile` runs a
`tool_remove_way` over a bridge span whose footprint shifted under
hex (the "2 tiles on top of each other" sub-test builds a bridge from
(3, 3) to (3, 5) and tries to remove (3, 2)→(3, 6); the span endpoints
no longer match).  `test_way_bridge_planner` iterates
`interesting_slopes()` and asserts which counter-slopes
`bridge_planner_x.find_end` accepts — the body is hex-ready, but
the working_slopes whitelist (`[ slope.north ]`) reflects what the
hex bridge planner actually accepts and may need the 4 hex-only
edge slopes (NE, SE, SW, NW) once those land.

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
vertex sharing (hex shares 3 per vertex).  With the terraformer
now hex-correct, `grid_raise` propagates to the 3 hex-vertex owners
rather than 4 square-vertex owners, so the tests' specific
assertions (4 tiles with 4 specific slopes each, 2x2 water-corner
reachability patterns) no longer hold.  These need per-test coord
rewrites, not a deeper engine fix — the invariants ("raising a
vertex affects the tiles sharing it", "lowering into water connects
reachable water tiles") survive, but the test bodies bake in square
arithmetic throughout.

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

**Hill-with-sloped-neighbours test setup.**
`test_depot_build_on_tunnel_entrance`, `test_halt_build_on_tunnel_entrance`,
`test_halt_make_public_underground` and `test_powerline_build_transformer`
each build their terrain by
raising the 4 corners of a 2x2 grid-point square — which under
square-terraformer propagation produced one raised tile plus 4
cardinal single-slope neighbours.  Under hex's 3-way vertex sharing
that pattern raises only 3 corners of the centre tile and misses
the hex-only edges on the neighbours.  Migrating via
`setslope(all_up_slope) + setslope(single_edge)` produces the right
grund_t slopes but leaves the per-vertex height storage inconsistent.
Now that the NW-corner-only writers are ported, restoration
needs a hex-aware test scaffold that raises the right vertices
directly rather than 4 corners of a 2x2 square.

## Per-vertex height storage — remaining writer-side ports

Storage is per-hex-vertex (see `documentation/hex-vertex-storage.md`,
`surface_t::grid_hgts`): two canonical slots (E, SE) per tile, plus
boundary padding.  `perlin_hoehe` now samples at world-vertex
positions and writes both canonical slots, so freshly generated
terrain is self-consistent across shared vertices — the three owners
of any shared vertex all resolve to the same slot and get the same
noise value by construction.

The two NW-corner-only writers (`hausbauer.cc:457` and
`simtool.cc:1600/1597`) are now hex-aware via the
`(koord, hex_corner_t::NW)` overload — building removal and the
setslope tool's grid-correction step write the right vertex.
Remaining writers that still need the same treatment: the `simtool.cc`
water-raise flood-fill (square 4-neighbour); the heightfield-load path
in `karte_t::init_tiles` (replicates the last square-grid row into the
doubled slot layout — needs a hex-aware importer or a clean rejection);
`karte_t::rotate90`'s heightmap-rotation loop (90° is not a valid hex
symmetry; the whole rotation path needs a refusal or a real hex
rotation, tied to the viewport port).  `perlin_hoehe`'s own rotation
code is still the legacy 90° square formula — it produces a
deterministic but geometrically wrong map when rotation != 0; fix in
the same pass as `rotate90`.

The `lookup_hgt(x, y)` / `set_grid_hgt_nocheck(x, y)` shim in
`surface.h` is `dbg->fatal` — every residual call is a crash so the
port can't accidentally regress new sites onto the old E-slot of
tile `(x-1, y-1)`.  Two narrow escape hatches survive in
`surface.h` for known-bubble-consistent uses (`legacy_grid_hgt` and
`legacy_set_grid_hgt_nocheck`); their callers are listed below by
retirement trigger.  Remaining shim call sites still pending port
audit into these clusters:

*Square-corner writer ritual (partial)* — `simtool.cc:2302-2312`
(8 sites) writes 4 "corners" at `(k, k)`, `(k+1, k)`, `(k, k+1)`,
`(k+1, k+1)` for partial-water grid heights in
`tool_change_water_height_t`.  Under hex these four shim coords land
on four unrelated tiles' E corners.  Currently fatal — building or
running the water-raise tool aborts.  Blocked on the water-flood-fill
port; once that lands, the writes gain a hex-corner name.  The
terraformer portion of this cluster retired with the 6-edge
propagation port.

*Square-corner reader ritual* — `surface.cc` 12 sites in
`get_neighbour_heights`'s `[8][4]` boundary fallback, routed through
`legacy_grid_hgt` to keep the function compiling.  Slot semantics
match the old shim (E corner of tile `(x-1, y-1)`); the values are
geometrically wrong under hex but consistent with what the
`recalc_natural_slope` consumer expects.  Retires together with the
`recalc_natural_slope` hex port — the function still iterates 8
square-grid neighbours and composes 4 corners per neighbour, none of
which match the 6-corner hex model.

*"Tile reference height" readers — semantic drift bubble (partial)* —
the shim's old "what is this tile's reference height" pattern picked
a single slot on tile `(x-1, y-1)`, which was geometrically wrong
under hex but bubble-consistent with the NW-corner-only writers
(now ported) that wrote it.  Five sites already ported with hex
semantics chosen per-call-site rather than via one global decision:
`wasser.cc:68` (`min_hgt`, water surface), `wegbauer.cc:599`
(`min_hgt`, "any corner below water"), `simtool.cc:2080-2085`
(`min_hgt`, climate water filter), `simplay.cc:225` (`min_hgt`,
floating-message anchor), `minimap.cc:735` (`get_hoehe + corner_sw`,
SW corner colour pick).  Remaining sites still firing the fatal
shim: `simtool.cc:1011, 1021, 2721`, `wegbauer.cc:1778`,
`tunnelbauer.cc:198`, `enlarge_map_frame.cc:201`.  Each needs a
small per-site decision (most fit `min_hgt`, but some — like
`enlarge_map_frame` — may want a different anchor); migrate as the
relevant test exercises them.

*Save-cycle round-trippers* — `grund.cc:175, 287, 297, 307, 316`
(5 sites), `simplan.cc:312, 317` (2 sites).  Read-then-write during
the rdwr save/load cycle and during `planquadrat_t` changes; routed
through `legacy_grid_hgt` / `legacy_set_grid_hgt_nocheck` to bypass
the fatal shim.  Bubble-consistent by construction.  Retires with
the save-format version bump.

*Explicit out-of-scope* — `simworld.cc:4673` heightfield load (1
site, blocked on import decision as noted above).

Each cluster above has an independent trigger; retire separately.

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

**Rotation cascade.**  `koord::rotate90` is `dbg->fatal` and the 6
top-level callers (`fabrikbauer` retry loops ×4, `karte_t::destroy`
/ `karte_t::save` retry loops, the `tool_rotate90` player tool) are
gated so the cascade is unreachable in normal play.  The underlying
`karte_t::rotate90` body and every `obj_t::rotate90()` override
still compile (so the binary builds and any hex-aware rotation
replacement can drop in cleanly), and the `ribi_t::rotate_for_map_rotate90`
/ `rotate_perpendicular` stubs (currently `rotate60`) stay for the
same reason.  Replace with a real design when the viewport port
lands — either a hex 60° rotation, a viewport-only rotation, or a
formal removal.

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

**`crossing_logic_t::get_dir` binary → 3-axis.**  `crossing_logic.cc:291, 368`
uses `crossing_t::get_dir() ? northwest : north` to scan for
mergeable neighbours, i.e. binary — handles 2 of 3 hex axes.  Any
3rd-axis crossing (road+tram on NE-SW, etc.) fails to merge.  Needs
`crossing_t::get_dir()` to return a 3-valued axis identifier.
Previously flagged only under the powerline entry; road/rail/tram
crossings use the same code path.

**`koord_random` / `clip_min` / `clip_max` rhombus caveat.**  These
are rectangular in axial `(q, r)` — rhombus-shaped in world space
under hex.  Current 11 callers all use them for map-bound clamps /
bounding-box iteration, which matches the tile array's rhombus
shape.  Flag so a future caller wanting a hex-circle / hex-radius
region writes its own helper instead of overloading these.

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
projection that KEEPS the isometric angle — flat-top hexes seen at
the existing isometric tilt, not a top-down projection.  The new
projection still has to line up with the existing 2:1 sprite raster
so the botched square-tile art stays legible during the transition.
Mouse-picking is the inverse transform plus a cube-round to snap to
the nearest hex.

## Save format version bump

Vertex topology, slope encoding and ribi width have all shifted or
will shift; the on-disk format is incompatible.  Bump the version
once the structural changes settle.  Either reject old saves
cleanly or write a one-shot square→hex converter (hard because 4
corners do not map cleanly to 6).

