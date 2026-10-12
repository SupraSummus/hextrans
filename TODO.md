# TODO

Live registry of work still to do — port work, gameplay bugs,
upstream-inherited rough edges, anything else with a concrete
next move and a concrete trigger.  This is NOT documentation and
NOT a changelog — describing finished work, current status, or
recent progress belongs in git history or the code itself, not here.
If an entry becomes outdated, delete it.

## Tests pending migration to hex model

Scenario tests that encode square-grid geometry are commented out in
`tests/all_tests.nut` with a short `// foo: HEX-PORT PENDING` tag —
see `AGENTS.md` → "Tests and the hex port".  The function body stays
in the test file with a short header comment.  Entries below list
what's currently skipped and the restoration trigger; remove an
entry here when its test is re-enabled.

**`ASSERT_WAY_PATTERN` protocol (for the tests still pending below).**
`ASSERT_WAY_PATTERN` takes an array-of-arrays of 6-bit ribi integers
(`-1` = don't care, `0` = no way)
and tripwires on string rows
because Squirrel indexes `row[x]` to ASCII codes
(so the legacy `"..5....."` shorthand
silently produced wrong expected values),
and 6-bit hex ribis don't fit a single digit besides.
When migrating a pending test,
the original patterns assume 4-bit square-axis paths,
so the actual hex-pathfinder route has to be reasoned out
(or read off a temporary dump) —
at least for `wt_road`
the builder hugs allowed/buildable tiles into a hex staircase
rather than the square L-path.
Worked examples:
`test_way_road_build_{straight, parallel, crossing, below_powerline}`,
`test_scenario_rules_allow_forbid_tool_stacked_{rect, cube}`,
`test_way_road_upgrade_{downgrade, crossing}`,
`test_way_tram_build_{on_road, in_tunel}`,
`test_way_road_cityroad_*`,
`test_way_tunnel_{build_straight, build_up_down, make_public}`;
the tunnel ones demonstrate the 6-vertex hex-hill scaffold
from the AGENTS.md primer,
and `test_way_tram_build_in_tunel` additionally shows
a 2-axis tunnel cross (S-axis road + SE-axis tram)
threading one buried hex tile with ribi 16|2|8|1=27.

**Legacy `slope.east` / `slope.west` in tests.**  The square-era 2-corner
diagonals are still admitted as way slopes by the post-slope-way
predicate (side-chord branch with samples both at 1), but the names
project poorly onto the hex axes and read as historical baggage.  Four
call sites (`test_building_rotate_harbour`, the "east-west direction"
block in `test_depot_build_on_bridge_end` and the matching block in
`test_halt_build_on_bridge_end`, `test_powerline_remove_powerbridge`)
migrated to `slope.southeast_narrow` / `slope.northwest_narrow`, the hex axis they were
already projecting onto.

**Deep-water grid-height clobber in `tool_change_water_height_t`.**
The hex apply loop ends with `set_grid_hgt_nocheck(kt, c, hn[c])` for
all six corners, `hn[c] = max(new_water_height, h0[c])`.  Upstream
square (`origin/simutrans`) never writes the grid array here — it keeps
`grid_hgts` at the sea floor and tracks the surface only via
`set_hoehe` / `set_water_hgt_nocheck`, so terrain survives a fill/drain
cycle.  The hex version raises the shared vertices to the water surface
once water goes deep, which loses the floor (drain can't restore it)
and reshapes the dry neighbours that derive slopes from those vertices.
Observable by digging a 2-deep hex pit, pouring twice, then draining:
the rim flips double→single on the deep fill and never returns, and the
floor ends at `-1` not `-2`.  Candidate fix: write the floor `h0[c]`,
not `hn[c]` (`hn==h0` for shallow/drain, so only deep water changes).
Blocking a blind fix: whether the flooded tile's own slope is stored
(`set_grund_hang`) or derived from the grid under the partially-landed
per-vertex model (see "Per-vertex height storage"), and whether deep
water still renders flat with the floor left in the grid — not
verifiable headless.  When fixed, add the deep-water subcase to
`test_terraform_raise_lower_water_level`: 2-deep pit, assert rim is
`*_double` shallow / `*_narrow` deep, drain, assert full restore.

**Powerline 3rd hex axis.**  `test_powerline_ways` builds powerlines
along a diagonal road and across the NE-SW direction.  Under hex there
are 3 axes and the 3rd (NE-SW) has no powerline crossing sprite or
connection FSM support (`leitung2.cc` diagonal-image table is keyed on
4 old-combo values).  Restore after the crossing-cluster / 3rd-axis
work lands.

**Upstream 125 stage 3 planner and bridge tests.**
The upstream terraform tests assume a single-height slope
refuses the step across it unless terraforming is on.
Under hex, `slope_allows_way_axis` admits every single-height slope
on all 6 axes (ramp or chord),
so `is_allowed_step` accepts those steps with or without the flag;
only a double slope refuses a non-axis step
(`slope.south_double`: N/S admitted, the other 4 refused without
terraforming and admitted with it).
Pending on that:
`test_way_planner_step_cost_slope`,
`test_way_planner_terraform_{defaults_to_off, across_slope,
matches_executor, not_kept_by_the_planner}`,
`test_way_road_build_terraform_{across_slope, single_step,
forbidden_by_scenario, not_kept_by_the_tool}` and
`test_way_road_build_road_terraform_across_slope`.
Next move: re-seat them on `slope.south_double` with an SE-axis step
and re-derive the post-terraform slopes from the hex edge rule in
`way_builder_t::check_terraforming`;
they then give `check_terraforming` and its replay in
`terraforming_leaves_way_slopes` the end-to-end coverage they lack.
`test_way_bridge_maintenance_failed_build` uses a span across a bridge
head as its refusal, which hex builds as a stacked bridge (see
"Bridge double-height policy" below);
swap in a refusal the hex builder honours,
e.g. the span over a runway from `test_way_bridge_build_above_runway`.
`test_way_tunnel_planner_rotated_map` and `test_schedule_route_rotate`
call `karte_t::rotate90` through the `sroute_rotate` test hook
and hit its tripwire;
they come back with the hex rotation (see "Rotation cascade").

**`ASSERT_THROWS` helper for scenario tests.**
43 sites across `tests/tests/*.nut` hand-roll the same
`local caught = false; try { ... } catch (e) { ASSERT_EQUAL(e, "<msg>"); caught = true } ASSERT_TRUE(caught)`
block to assert a tool call throws an expected message.
Add `ASSERT_THROWS(expected_msg, callable)` to `test_helpers.nut`
and collapse the sites to one line each.
Soft trigger:
fold in next time a test file in this set is being rewritten anyway.

## `is_allowed_step` prefer_parallel branches near-duplicated

`strasse`, `schiene`, and `schiene_tram` in `wegbauer.cc::is_allowed_step`
now carry three near-identical prefer_parallel cost branches (the
last added on the hex 1-step diagonal motivation).  All three follow
the same shape: cheap if `has_neighbour_with_way(from, wt)`, otherwise
`way_count_no_way`, with a fall-through `way_count_straight` /
`way_count_no_way` when prefer_parallel is off.  Extract a helper
`prefer_parallel_cost(from, wt)` and drop the upstream dead branch
(`if (to->get_weg(wt))` inside `if (!sch && ...)` is unreachable —
`!sch` already excluded it).  Refactor, not a behaviour change;
defer until the next time any of these branches needs a real
edit.

## Lower_to water-tile NW-only gate

`terraformer_t::lower_to` short-circuits water tiles unless the NW
corner is being lowered.  The NW pick is the legacy "tile reference
height" corner from the square era — under hex no single corner has
that role.  In practice the gate decides "did the corner the water
table is keyed off of drop", and the answer probably wants to be
"did `min_corner` drop" or "did any corner that touches a
neighbour-with-higher-water drop".  Real semantic choice, not a
mechanical refactor; lands together with the wider hex-aware
water-table propagation pass when that gets scheduled.
`can_lower_tile_to` mirrors the same gate so its noop short-circuit
matches `lower_to` exactly; both retire together.  `raise_to`
already migrated to a corner-neutral `!any_up` short-circuit — the
"flat-at-water-level → land" reconciliation that used to keep the
water-side gate open is now driven by the targeted-corner lift in
`surface_t::grid_raise`.

## Bridge double-height policy: art-permissive

`bridge_desc_t::has_double_start()` and `has_double_ramp()` are
satisfied by `max_height == 0 || max_height >= 2` —
the desc's declared height cap,
not whether the pakset ships matching `*_Start2` / `*_Ramp2` art.
Under pak64 that art is never shipped;
the engine still builds planar double-slope and 2× flat-tile bridges
and renders empty or with the single-edge sprite at the wrong height.
Per `AGENTS.md` → "Engine encoding leads the pakset",
missing pakset art is downstream catch-up work, not an engine blocker —
crooked graphics rather than refusing to build.
`bridge_img_valid` remains the one img-side gate
(rejects the `img_t_count` "no slot for this slope" sentinel
that catches `all_up_slope`
and other shapes the get_start switch doesn't cover).
Restore art-presence probes
(or replace them with explicit per-slot pakset declarations)
once Start2 / Ramp2 art is shipped.

## build_bridge auto-extension misses delta-2 ramps

`bridge_builder_t::build_bridge` (`brueckenbauer.cc:911`) walks
each endpoint after the bridge is built and, when the bridgehead's
way ribi is single, runs a 2-tile `way_builder_t::calc_route` to
lay a connecting road stub on the kartenboden behind the ramp.
Under hex, delta-1 narrow ramps still get the stub but delta-2
planar-double ramps don't — `test_way_bridge_build_stacked`'s
asymmetric cleanup is the worked example.  Probable cause is that
`way_builder_t::calc_route` doesn't admit a brueckenboden ramp
tile with `weg_hang = *_double` as a valid start.  Next move is
to either teach the planner to start on planar-double ramps, or
short-circuit the auto-extension for those — a `way_builder` is
overkill when one flat neighbour is the target, a direct
`neuen_weg_bauen` would do.

## Way-object slope-up sprites — still 4 of 6 hex edges

`way_obj_writer.cc` iterates `slope = 3, 6, 9, 12` for
`frontimageup` / `backimageup` under the square-era pattern:
4 of 6 hex edges,
missing the two third-axis edges
(raised E+SE and raised NE+E pairs,
see `koord_hex.cc` neighbour case table).
Needs the same widening that `way_writer.cc` got —
18 slots covering all 6 hex edges × {narrow, wide, double} —
plus a re-key of the readers
`way_obj_desc_t::get_front_slope_image_id` /
`get_back_slope_image_id`,
which still switch over the square-era slope values
4, 12, 28, 36 and their doubles 8, 24, 56, 72.
Land alongside the first hex sloped way-object asset.
`ground_writer.cc` was already widened to scan `slope_t::max_slopes`
so sparse hex slope indices round-trip into `.pak`.

## Way .dat migration to hex ribi keys

`way_writer.cc`'s flat-image table is now 64 slots indexed by full
6-bit hex ribi (keys `Image[se]`, `Image[se_nw]`, …; `_` separator
because `tabfile_t::find_parameter_expansion` treats `,` and `-`
inside `[…]` as parameter-list mode).  The legacy 4-bit `Image[N]`
/ `Image[NSE]` / `Image[NSE1]` keys are gone.  Only
`rail_060_tracks` is migrated on the pakset side as a worked
example — every other way .dat (rail_080..rail_400, roads, trams,
runways, kanals, narrowgauge, monorail, maglev, plus catenary /
elevated variants) compiles cleanly but draws blank for every
connectivity except `Image[-]`.  Migrate per family, repointing
existing pak cells onto the matching hex direction by onscreen
heading (pak `N` = upper-right = hex NE; pak `E` = lower-right =
hex SE; pak `S` = lower-left = hex SW; pak `W` = upper-left = hex
NW).

River ways have a narrow engine-side visibility fallback:
`way_desc_t::get_image_id` maps missing `type_river` multi-ribi slots
to an available single-edge river sprite,
and `weg_t::pick_image_slot` falls back
from missing river slope sprites to the flat-ribi path.
This is not a general way-art shim;
it exists because natural map generation can create rivers
before pak128 has complete 64-slot river art.
Delete the fallback when `landscape.rivers`
ships real hex flat and slope river sprites.

The hex ribi rework didn't bump the on-disk `way_desc` save version
(the `has_double_slopes=` opt-in did, taking it from 8 to 9, but
that's an unrelated additive field with a legacy default).  Pre-built
paks (pak64 from `tools/get_pak.sh` in CI; any cached pak128 build)
still load without fatal — their 16-entry flat imagelist gets
indexed by the 6-bit hex ribi, so `ribi >= 16` returns IMG_EMPTY and
`ribi 0..15` returns the upstream sprite at that slot drawn for a
different hex direction than the index now means.  Rendering is wrong
but graceful; gameplay-only scenario tests still pass because they
don't assert on visuals.  Bump the version *for the hex ribi cutover*
once the migration is worth a hard fail, or rebuild the CI pak from
source against the new makeobj when one becomes available.

The same migration story applies to slope-up keys.  `way_writer.cc`
now expects 54 keys: 18 full-axis crossings (`ImageUp[n]` …
`ImageUp[nw]` plus `_wide` and `_double` variants) routed by
`way_desc_t::get_slope_image_id`, and 36 half-slope stubs
(`ImageUp[n_low_half]` / `ImageUp[n_high_half]` and the matching
`_wide_*` / `_double_*` forms) routed by
`way_desc_t::get_slope_half_image_id` — picked when a way's
single-bit ribi terminates on a ramp's low or high edge.  The
legacy `ImageUp[3/6/9/12]` and `imageup2[*]` keys are gone.
`rail_060_tracks` and the eight hex-migrated road tiers
(cityroad_030, highway_110, road_030..road_090) ship the full set
including half-slope cells via `road_params.make_tier()`'s
`bake_pakset`; every other way .dat (rail_080..rail_400, trams,
runways, kanals, narrowgauge, monorail, maglev, plus
catenary / elevated variants) compiles cleanly but draws blank
for slope and half-slope sprites.  Migrate per family alongside
the flat-image migration above.

## Per-vertex height storage — remaining writer-side ports

`karte_t::rotate90`'s heightmap-rotation loop (behind its tripwire)
is still the legacy 90° square formula,
and `perlin_hoehe`'s own rotation code shares the bug.
Both are rewritten with the rotation cascade (see below):
the slot permutation is `canonical_vertex(shape.rotate_vertex(v))`,
pinned by `tools/hex_map_shape_test`,
and `perlin_hoehe` un-rotates with `hex_axial_rotate_inv`.

The `lookup_hgt(x, y)` / `set_grid_hgt_nocheck(x, y)` shim in
`surface.h` is `dbg->fatal` — every residual call is a crash so the
port can't accidentally regress new sites onto the old E-slot of
tile `(x-1, y-1)`.  Gameplay code should not call the fatal
`lookup_hgt` / `lookup_hgt_nocheck` `(x,y)` / `(koord)` overloads;
use `(tile, hex_corner_t)`, `min_hgt` / `max_hgt`, or
`grund_t::get_hoehe` as appropriate.

The natural-height channel (`natural_grid_hgts`) is not yet persisted
in saves.  On load it is seeded from `grid_hgts` as a best effort, so
saves that include set-slope-tool overlays will round-trip with those
overlays looking like natural ground to a post-load
`recalc_natural_slope`.  The loss is contained — the visible grid is
unchanged across save/load, only the natural-vs-artificial
distinction at already-overlaid vertices is lost — and a save-format
bump will fix it.  Tied to the wider save-format cluster.

`karte_t::calc_humidity_map_region` branches on `ribi_t::northwest`
/ `southeast` / `north` for wind direction, leaving NE, SW and
explicit S unhandled — only 4 of the 6 hex wind directions feed the
gradient walk.  The gradient itself is a per-tile NE-NW corner pair
(the hex-port translation of the legacy `lookup(x+1, y) - lookup(x, y)`
square-axis read), so it's still computing the same horizontal slope
regardless of wind direction.  Both quirks land together in a
hex-aware rewrite of the climate generator.

## Slope readers that still look at four corners

`baum_t::calc_off` in `obj/baum.cc` interpolates a tree's vertical
offset bilinearly over `corner_sw/se/ne/nw` inside a 32x32 box.
On a slope whose only raised corner is E or W the offset comes out
0, so the tree stands on the tile base.
Replace the box with the hex tile: pick the in-tile sample point in
hex coordinates and interpolate over the three corners of the
triangle it falls in.
The sample-point choice is square-shaped too, so both move together,
ahead of the tree sprite offsets in the renderer port.

## W×H×D hexagon world

The playable world becomes the W×H×D hexagon of #280,
stored as its bounding rhombus with a validity predicate;
`documentation/map-shape.md` has the design and a six-step plan.
Step 1, `map_shape_t` in `dataobj/map_shape.h`, has landed;
only `array2d_tpl::rotate90` uses it so far.
Next move is step 2: `size_d` in `settings_t` and the save,
and the cut in `surface_t::is_within_limits`.

## Engine → pakset descriptor boundary

The simulation should speak full 6-way hex directions everywhere
(the 6-bit `ribi_t`), without narrowing for the benefit of legacy
4-direction art.  Sim-side code pattern-matching on square-era ribi
values (`leitung2.cc` magic 3/6 in the crossing-image picker) or
bound-checking with `ribi < 16` (`way_obj_desc.h` crossings) is
residual square-grid assumption to clean up, not a sim/art
compromise to ratify.

The current working hypothesis for where the seam between hex-aware
sim and 4-direction pakset art lives is the descriptor API
(`way_desc::get_image_id`, `vehicle_desc` sprite lookup, the
`ribi_t::_dir` enum and `get_dir()`): sim above the boundary always
passes a full 6-bit ribi in, descriptor below the boundary owns the
projection onto whatever sprite table its art actually has, with the
lossy mappings named and centralised.  This is a hypothesis to test
as the audit-surface entries below get cleaned up, not a committed
design — the seam may move once we see what stays clean.  The
principle (sim is fully hex) is firm; the location of the projection
layer is not.

Three writers prototype the new convention —
`way` (64-slot ribi protocol, `Image[se]` / `Image[se_nw]` / … keys),
`bridge` (3 hex-axis segments + 6 hex-edge starts/ramps,
hex-aware enum, tripwire on pre-port saves; commit `1e21a2a`),
and `tunnel` (6 hex-edge portals via `hex_keys::edge_names`,
low-edge naming matching bridge —
inverting the upstream high-edge convention so legacy paks render permuted,
same "loads without fatal but renders wrong" breakage as
the way ribi key change;
broad-portal `l/r/m` neighbours orthogonal to direction).
Shared key vocabulary lives in `descriptor/writer/hex_keys.h`
(`axis_names[3]`, `edge_names[6]`);
subsequent writers consume the constants on port rather than reinvent.

Four direction-keyed writers still embed their own square-era
tables.  Each is gated on its first hex pakset asset — mechanical
key renames without matching art are an invisible no-op, so
migrations land per-family with the asset that uses them.

**`way_obj_writer.cc`** still iterates the 4-bit ribi space
(`ribi_codes[26]` covers `0..15` plus 5 crossing variants);
the matching `way_obj_desc.h:56-60` `ribi_to_extra[16]` table is
gated on `ribi < 16`.
Widens to the 64-slot ribi-as-key protocol that `way_writer` uses;
no name vocabulary needed.
The 4-cardinal slope-up loop (`slope = 3, 6, 9, 12` for
`frontimageup` / `backimageup`) is the same square-era pattern
called out in "Way-object slope-up sprites" above;
both retire alongside the first hex wayobj asset.

**`crossing_writer.cc`** writes `openimage[ns]` / `openimage[ew]` in
a 2-axis loop.  Hex has 3 axes, so the keying widens to
`hex_keys::axis_names`; the harder question is gameplay — a
road-rail crossing on the NE-SW axis needs art that doesn't exist in
pak128 today.  Engine port and pakset port land together; flag for
design conversation before mechanical work.

**`roadsign_writer.cc`** has three hardcoded direction tables
(2/4/8 entries) and only 4 image slots for traffic lights
({SE-NW, NS} × {green, yellow}).  The engine-side FSM is now
3-axis adaptive; `calc_image`'s `state_to_direction[6]` table in
`roadsign.cc` projects NE-SW (states 2 / 5) onto the NS slots as
placeholder visuals, so 6-way intersections cycle correctly
under the hood but render with miscoloured arms until the
writer grows a third direction tier and pakset art ships.
Delete the placeholder projection in the same change that
widens the writer.

**`vehicle_writer.cc:178-180`** has `dir_codes[8]` for sprite-facing
direction; tied to the `ribi_t::_dir` widening tracked in "Vehicle
direction enum — compound 2-step displacements" below.  Holds with
the viewport / sprite port.

Cross-descriptor save-format situation is the wider "Save format
version bump" cluster below — bridge has a tripwire refusing pre-port
saves, others still load square keys into hex slots silently.  Bump
once the structural changes settle so the tripwires can land
together.

## ribi_t — audit surfaces

These are the shim / stub patterns spread across the caller port
that need a second sweep once their trigger condition lands.  Each
is named / tagged so a global grep surfaces all sites.

**Rotation cascade.**
#281 decided on real world rotation in 60° steps,
built on upstream's `karte_t::rotate90` cascade.
The primitives are in:
`hex_axial_rotate` (`dataobj/koord_hex.cc`) is the step about the origin,
and `koord::rotate90(y_size)`, `koord3d::rotate90` and
`array2d_tpl::rotate90()` take one step of a W×H rhombus map
(`map_shape_t::rotate_tile` is the same step for any W×H×D).
One step leaves the rhombus; only three steps (180°) map it onto itself.
`karte_t::rotate90` is a `dbg->fatal` tripwire,
because its body and every `obj_t::rotate90` override are still
upstream's 90° cascade,
and the 6 top-level callers (`fabrikbauer` retry loops ×4,
`karte_t::destroy` / `karte_t::save` retry loops, the `tool_rotate90` player tool)
are gated off.
Next move: give the cascade hex bodies, 180° first on today's rhombus
(`(q, r) -> (W-1-q, H-1-r)`: tiles, heights, buildings, offsets,
`settings_t::rotation` mod 6), then drop the tripwire and open the tool;
rename `rotate90` to `rotate_one_step` once upstream stage 5 lands;
single 60° steps wait for the W×H×D storage (see above),
when the `y_size` argument becomes the shape.
Sites that rotate an extent rather than a position
(`ai_goods.cc`'s `size1.rotate90(0)`) want the linear `hex_axial_rotate` alone.
The `ribi_t::rotate_for_map_rotate90` / `rotate_perpendicular` stubs
(currently `rotate60`) stay for the same reason.

**`ribi_t::rotate_perpendicular` / `_l` sweep.**  Square-era "90°
off this direction" sites — crossroads collision avoidance
(`road_vehicle.cc`, `pedestrian.cc`, `vehicle_base.cc`),
broad-tunnel side tiles (`tunnel.cc`, `tunnelbauer.cc`), canal
orthogonality (`wasser.cc`), parallel-way and shore-perpendicular
build heuristics (`simtool.cc`), and the roadsign map-rotate call
that mis-uses `rotate_perpendicular` where the comment intends
`rotate_for_map_rotate90` (`roadsign.cc`).  Also stubbed to
`rotate60` (the "one step over" axis).  Triggered by the
crossing-cluster port: per-site review, some may want both 60°
and 120° (test both adjacent axes), some may redesign the check
entirely for hex 3-axis geometry.

**Growing-chord overhang in 60°-pair parallel builds.**
`tool_build_way_t::calc_route` flips `prefer_parallel` when both
endpoints have a way among their 6 hex edge-neighbours, inverting
the cost so detours through existing ways become expensive.
Successive chord tips of an incrementally-longer parallel build
land at displacement `A+B` where A is the chord axis and B the
offset axis; under hex 6-fold symmetry the (A, B) pairs split
into two closed families: 120°-pairs where A+B is itself a hex
axis (1 edge-step, caught by the scan) and 60°-pairs where A+B
is between axes (2 edge-steps, outside the scan).  The 60°-pair
family silently disables detection and the router reuses earlier
rows as a cheap highway.  `test_way_road_build_parallel_routefinder`
covers the 120°-family; the 60°-family is a coverage gap, not an
axis bias — both families are closed under rotation and
reflection.  Visible cost: half of the (chord, offset) build
patterns zig-zag when built incrementally without ctrl.  Candidate
fixes: widen the proximity scan to also cover the 6 "wedge-tip"
2-step neighbours symmetrically across all three axis pairs (with
the caveat that wider detection may over-fire when the user wants
to join into an existing network 2 steps away), or restructure
the cost so prefer_parallel detection isn't load-bearing (e.g.
always treat fresh-with-way-neighbour as cheap and let curve cost
break ties).  Move when the 60°-family pattern actually surfaces
in gameplay or AI grids.

**Diagonal way-image selection.**  Hex has no out-of-axis diagonal
direction — every direction lies on an axis.  The `+2` offset for the
diagonal slot in `way_desc_t::image_list_base_index` is preserved for
save/load layout compatibility; `way_writer` emits an empty imagelist
into it.  `leitung2.cc`'s 4 surviving `get_diagonal_image_id` calls
on old 4-bit combo values (3, 6, 9, 12) are dead-end residue; delete
together with the "Powerline 3rd hex axis" cluster's gameplay
redesign.

**`is_straight_ns` last caller.**  `leitung2.cc` picks one of two
powerline diagonal sprites with the 2-axis predicate; NE-SW lands on
the wrong branch.  Bound to the "Powerline 3rd hex axis" sprite
cluster above; retires together with that cluster (along with
`is_straight_ns` itself).

**`ribi_t::is_perpendicular` 2-axis vs 3-axis.**  Under hex there
is no true 90° axis relation; the current predicate returns true
when x and y together span more than one hex axis (= "different
axes").  Callers in `route.cc`, `wegbauer.cc`, `vehicle_base.cc`
use this for collision avoidance and pathfinding.  Each needs
review: some want "different axis" (current semantic fits), some
may want "specific axis pair" for crossings that only care about
2 of 3 hex axes.  Not a silent bug today but the semantic shift
from 2-axis to 3-axis is a gameplay choice.

**Vehicle direction enum — compound 2-step displacements.**
`vehicle_base_t::calc_set_direction(start, end)` is called both with
adjacent (1-step) and 2-apart (2-step) position pairs —
the caller decides which based on what it wants,
e.g. `calc_set_direction(pos_next, pos_next_next)` for the
"what direction is the vehicle about to move" query
vs `calc_set_direction(get_pos(), pos_next_next)` for the
"what 2-tile diagonal is this visually".
Under the square grid that produced 8 displacement values
(4 cardinal 1-steps + 4 diagonal 2-steps),
which the old enum captured exactly.
Under hex the space explodes:
6 1-step hex neighbours,
6 "same-direction" 2-steps (2,0)/(2,-2)/...,
12 "turn" 2-steps like (1,1) from S+SE or (2,-1) from NE+SE,
and (0,0) for u-turns.
Around 18 distinct visual states in total,
none of which fit cleanly in the 8-value `ribi_t::_dir` sprite enum either.
The current port pattern-matches on the sign of (di, dj)
and maps to the closest of the 6 hex neighbour ribis;
the (1,1) and (-1,-1) cases
(produced only by S+SE, SE+S, N+NW, NW+N pairs)
fall into a defensive `ribi_t::none` fallback with dy=±2, dx=0.
Downstream consumers (collision avoidance, sprite offsets, disp_lane)
will be visually wrong in exactly the cases where 2-step compound paths arise.
Proper fix is a hex-aware vehicle direction model —
likely 12 states (6 hex edges × 2 magnitudes for 1-step vs 2-step)
or a distinct `path_t` that carries both entry and exit edge names —
and lands together with the viewport / sprite port.
The dir enum in ribi.h similarly needs widening;
`get_dir()` currently maps each of the 6 hex edges onto its
same-named slot of the 8-value enum
and returns `dir_invalid` for anything that is not a single edge.

Save-file format: `weg_t`'s in-memory ribi is now two full bytes
(was a packed 4-bit bitfield that silently truncated hex bits 4-5),
and the rdwr load path masks against `ribi_t::all` instead of `15`.
The on-disk byte layout is unchanged for now, but old saves load
their 4-bit values into the new low-4-bits slots — which under
hex mean SE/S/SW/NW, not the original N/E/S/W.  Needs a save
version bump + a one-shot converter (or clean rejection) before
any pre-port saved game survives a round-trip.  See also the
"Save format version bump" section below.

`ribi_t::_dir` sprite enum is still 4-direction;
`get_dir()` maps NE/SE/SW/NW onto the same-named slots
(`dir_northeast` etc.) and returns `dir_invalid` only for `none` and
multi-bit ribis.
Tied to sprite port.
See also the vehicle-direction compound-displacement note above —
18 distinct visual states under hex, 8 slots in the current dir enum.

**Aircraft landing-circle 4-of-6 runways.**
`air_vehicle_t::calc_route` in `air_vehicle.cc:418` switches on the
runway end's `get_weg_ribi` to pick an offset into the 16-step
`circle_koord` hold-pattern table — N=0, NW=4, S=8, SE=12.
NE and SW runways fall through to the implicit `offset=0` default
and put aircraft on those runways into the N-approach hold pattern.
They still take off and land through it
(`test_air_takeoff_and_landing_all_runway_axes`),
but the pattern reaches 4 tiles in +q from the landing start,
past the 5-tile margin `intern_calc_route_runways` checks:
on the 16×16 test map a NE-SW runway at (9,5)-(6,8) builds
and every landing fails with "airport too close to the edge".
The 16-step `circle_koord` table is itself hand-rolled for
4-direction airports —
widening the switch alone isn't enough;
the table needs new step sequences for the NE/SW approach geometries.
The AI airport builder lays only N-S and SE-NW
(see "Airport layout" below),
so the path is reachable from player-built NE/SW runways
and pre-port savegames, not from AI gameplay.
Lands together with a hex-aware hold-pattern geometry —
likely 6-direction symmetric circles rather than 4.

**Wind direction in the settings window.**
`settings_climates_stats_t` (`gui/settings_stats.cc`, inherited from upstream)
edits `wind_direction` as a number 0..3,
so the window can only set none, SE, S or SE|S;
anything but a single edge leaves planes with no takeoff end.
The new-map dialog in `climates.cc` already has a 6-entry combobox.
Next move: reuse that combobox there, or drop the field,
and clamp the value as `settings_t::rdwr` does.

`do_terraforming` in `wegbauer.cc` has an
`if (from_slope > slope_t::all_up_one  &&  is_axis_slope(from_slope -
slope_t::all_up_one))` branch (and its `to_slope` twin) that was
unreachable while slopes were stored in `uint8` (`all_up_one ==
1365 > 255`).  After the widening in this branch it can fire for
the first time under hex.  The semantic — "shifted-up axis slope
means raise tile by 1 and unshift" — is geometrically plausible
under the hex 6-corner encoding (axis slopes still subtract
cleanly when both layers' corners are at the same height), but
hasn't been exercised by a concrete trace.  Run one through and
either confirm or replace with a hex-aware lift; lands alongside
the first real-gameplay surfacing of city / AI terraforming.
While here, consider widening the predicate to `is_axis_ramp` —
shifted-up `*_double` slopes have corner heights up to 3 (still in
the base-4 encoding) and currently fall through the
single-only filter into the "leave alone" branch.

## Renderer port

The square renderer assumed a 2:1 iso "diamond" lattice everywhere
(forward + inverse projection, render-loop iteration, visible-tile
bbox, slope corners, ribi edges, sprite tables, minimap).  Phase A
(geometry — viewport projection, render-loop, bbox) has landed in
`display/hex_proj.h`, `display/viewport.cc`, `display/simview.cc`
with `tools/hex_proj_test/` as its standalone invariant suite.  The
remaining renderer work splits into:

**Phase B — per-tile detail.**
Base ground sprites, grid borders and cliff back walls
read directly from pakset-provided descriptors
(`LightTexture`, `Borders`, `Slopes` / `Basement`)
indexed by raw `slope_t` or `(wall, index)`;
the old engine-side `synth_overlay` family is gone,
the legacy 1D 22-cell `Slopes` / `Basement` packing is gone,
and the upstream Fabio Gonella rock-photo art for cliff faces
is preserved under `hextrans-pak128/landscape/grounds/back_wall/src/`
for future texture supervision.
Markers are the exception:
`ground_desc_t::get_marker_image` still derives the square-era
base-3 `% 27` key from a `uint8`-truncated slope
and reads `Marker` through `get_image(index)`,
while the pakset bakes `Image[<slope_t>][0 front / 1 back]`.
Key it like `get_border_image` (normalised raw `slope_t`),
with the back / front half as the second index.
Climate transitions use
`vertex_owners` plus `surface_t::vertex_corner_height` /
`climate_at_clamped` (const members; height uses `canonical_vertex`
then tile clamp like legacy neighbours) in both `recalc_transitions` and
`display_boden`; the six-bit mask lives on `planquadrat_t` (save
124.5: extra byte in `grund_t` for bits 4–5).  Shore alpha and
slope alpha (snowline / climate-corner mixing) both read pakset
`(slope, mask)` blocks directly — `ShoreTrans` and `SlopeTrans`
share `LightTexture`'s silhouette via `hex_synth.silhouette_mask`
so source and alpha RLE walks stay in lockstep, and
`init_ground_textures` tripwires both invariants once at startup.
The snowline path —
`get_alpha_tile(slope)` derives its mask from the slope's elevated
corners (`corner_*(slope) > 0`),
which is an isolated-bit mask on slopes
with non-edge-union elevated corners (single-corner-up,
opposite-corners-up, alternating-corners-up — ~41 of 141 valid
slopes).  `SlopeTrans`'s bake omits isolated-bit masks (they aren't
realisable for the climate-mixing path), so on those slopes the
lookup returns `IMG_EMPTY` and the snowline overlay is skipped —
visible regression vs. the legacy 15-cell square pakset.  Resolve
either by an engine-side mask derivation that always returns an
edge-union (e.g. expand each isolated corner by adding one
neighbour bit) or by baking the snow-only cells separately from
the climate-keyed table.
`simgraph16_draw_base_img_alpha` silently skips when either image
is `IMG_EMPTY`, matching the rezoomed sister and the contract
`f9b0a072` relied on.
Drift between the `(slope, water_corners)` combos
`grund.cc::display_boden` emits and the set `texture_shore/` ships
is therefore invisible — the overlay just disappears.  The
"lifted corners can't appear on real terrain" premise was hit in
practice (the ASAN SEGV that prompted the guard); enumerate the
combos `display_boden` actually produces vs. those the bake covers
before trusting the init-time silhouette tripwire as sufficient
coverage, or move the realisability check to a tripwire in
`grund.cc` where "we expected an overlay" is a checkable
condition.
Deep water still pairs with the pakset `Water` animation block as
below; on-slope water and snow now use LightTexture-derived shapes.
`rotate_transitions` still applies a 60° bit-rotate as a stand-in for
90° map rotate (same caveat as `karte_t::rotate90` elsewhere in this file).

On-slope water tiles now use the same required `LightTexture`
ground-lightmap path as climate ground.
Deep water still comes from the pakset `Water` animation block.

Cliff back-wall middle-slope indices 9 / 10 are baked as single-step
half-cliffs (one corner at 0, one at 1) — placeholder for the legacy
double-height notch shape that the pakset renderer at
`hextrans-pak128/landscape/grounds/back_wall/render.py` doesn't yet
model; revisit if stacked terraforming reads wrong.

Way-ground on bend / junction ribis returns early in
`display_way_ground` (any ribi where `straight_axis` is `none`),
so the natural-ground tile is drawn unmodified — the way sprite
then renders against unfilled corner triangles and the embankment
visually vanishes.  The atlas key would have to widen
(`(ribi, slope)` or layered single-axis cells), and the bend's
chord plane isn't a single straight strip so the
`way_ground/build_pakset.py` geometry needs a second case.
Triggers when a city builder places a junction on a slope, and now
also when a player builds a 2-edge bend or 3-edge Y junction on a
canonical edge ramp (plateau-chord rule in
`slope_admits_plateau_chord`).
`resolve_way_ground` skips save-state slope/ribi pairs rejected by
`slope_allows_ribi`, so legacy or transitional saves with forbidden
ways do not abort while drawing.  A load-time validator remains the
proper cleanup move when save migration gets a dedicated pass.

Fence sprite library is hex-sized but pakset-empty: engine
`FENCE_IMAGE_COUNT = 7` carves natural typ 0..6 / artificial typ
7..13 across the 7 non-zero back-wall combos, but paksets still
ship the 3 legacy wall-0/wall-1 combos.  Pak64's old artificial
sprites at indices 3..5 now address natural wall-2 combos and the
artificial slot is empty.  Ship 7 natural + 7 artificial fence
sprites in the hex pakset and pak64 to refill.

**Phase C — flow-on.**  Per-step vehicle interpolation offsets
(`vehicle_base_t::set_dxdy`, shared by hop time and save/load), label / halt
screen-anchor positions.  The label / halt group rides along on
`get_screen_coord` so positions become hex-correct automatically;
per-tile drawing under each anchor still assumes square geometry
until phase B lands.  `vehicle_base_t::calc_set_direction` now
enumerates 6 hex axes but the `dx,dy` magnitudes are still the
legacy square-iso pair (`(±2, ±1)` for the four edges that survive
the rename, `(±4, 0)` for NE/SW); under the hex lattice N/S
neighbours are at screen distance `2u` while NE/SW are at `u·√10`
≈ `3.16u`, so NE/SW vehicles visibly drift off-tile mid-hop on
straight segments.  Rebase the per-edge offsets on the lattice; every
edge crosses a tile in the same number of steps.

Two more per-direction tables still follow the old square model.
`driveleft_base_offsets` and `overtaking_base_offsets` (`vehicle_base.cc`)
are keyed by `get_dir()`,
so SE/NW traffic gets the square-diagonal lane rows
instead of the rows the old east/west edges used.
Next move: key them by ribi, the way `set_dxdy` does.

**Bend tile length.**
A vehicle crosses a tile on the chord from the middle of its entry edge
to the middle of its exit edge.
On the square grid every bend was a 90 degree corner,
and `diagonal_vehicle_steps_per_tile` (181 of 256) stands for its length.
A hex bend is one of two:
a 60 degree change of heading (edges 120 degrees apart, 0.87 of a straight
run, about 221 steps) or a 120 degree one (edges 60 degrees apart, 0.5,
128 steps).
`calc_chord_steps_next` instead tells bends from straight runs by the chord
from the tile before to the tile after:
the 60 degree bends are off-axis and take the 181 steps,
the 120 degree bends have an on-axis chord and take the full 256, like
straight travel (a bus crosses a SE-SW bend tile in 42 script steps, a
SE-S bend tile in 28).
Next move: derive `steps_next` from the turn between the two edge steps
(256, 221, 128), retire `diagonal_vehicle_steps_per_tile` and the pakset
`diagonal_multiplier` together with `is_cutting_corner()`, and add bends of
both kinds to `test_vehicle_tile_length_all_hex_directions`.

**Chord direction of vehicles.**
`vehicle_base_t::direction` holds `calc_chord_direction` of the tiles
before and after the vehicle's tile,
which sorts the off-axis chords of 60 degree bends into NE / SW or none.
`vehicle_base_t::calc_direction` is still `ribi_type`,
which gives none for every off-axis chord,
so `no_cars_blocking`, the overtaking checks and `road_vehicle_t`'s
`next_direction` compare chords classified two ways on a bend.
Next move: make `calc_direction` call `calc_chord_direction` and check the
bend cases of `no_cars_blocking` with a scenario of two buses meeting on a
60 degree bend.
Even then one bit names only one of the two edges an off-axis chord lies
between (SE then NE lies between SE and NE and is filed as NE),
so a "does this chord lead back over that edge" test can miss.
`can_go_alte_richtung`'s going-backwards test therefore uses
`calc_chord_direction` and adds the route's first step,
which is always an edge.
The other direction tests may need the same first-step term.

Minimap (`gui/minimap.cc`) now hex-projects in isometric mode:
`map_to_screen_coord` / `screen_to_map_coord` use the `(3u, u)` /
`(0, 2u)` lattice with `u = zoom_in`, and `set_map_color` paints
the `3u × 2u` screen-aligned cell that the floor-inverse picks for
a click in that cell — so paints and clicks stay in lockstep.  The
schedule-overlay tint walks the sheared parallelogram outline
scanline by scanline (slope 1/3, derived from `map_to_screen_coord`
on the four world corners).  Plan (non-iso) mode is unchanged —
still an axial-coordinate plot, lying about hex shape but compact.
Residual: at `zoom_out ≥ 2` multiple tiles still collapse to one
pixel as before — inherited from the legacy projection.  A regular
flat-top hex view (vs. the current 2:1 stretch that matches the
main viewport) would be a third mode, not a fix; not on the path
unless the main view's lattice changes.

**Sprite raster choice (pinned design decision).**  The lattice
the projection runs on is a *clean integer approximation* of hex
iso, not a regular hex tiling.  With unit `u = IMG_SIZE/4`:

* column step (axial `+q`): `(3·u,  u)`
* row step    (axial `+r`): `(  0, 2·u)`

Adjacent +q tiles are at screen distance `u·√10` while adjacent +r
tiles are at `2·u`, a ratio of √10/2 ≈ 1.58.
In world space the lattice is still a regular flat-top hex,
seen in orthographic projection from 35.26° elevation:
sin = 1/√3, so y compresses by 1/√3.
That is the elevation of true isometric, without its 45° yaw.
What the lattice *does* preserve: (1) the existing
`IMG_SIZE × IMG_SIZE/2` ground footprint, so legacy diamond sprites
overlay on the right footprint until pakset art lands; (2) the iso
2:1 row/column y-ratio, so vehicle motion and z-elevation keep the
angles they expect; (3) integer fractions of `IMG_SIZE`, so the
projection stays in fixed-point.  Two alternatives were rejected:
*hex-width-preserving with true √3 geometry* (irrational, breaks
fixed-point) and *square-row-spacing-preserving* with row step
`(0, u)` (halves the hex height, sprites overlap massively).
Revisit when sprite art enters scope.

**Square-art-as-placeholder strategy.**  Real hex pakset art —
sprite redraws, animator work, per-direction vehicle frames — is
out of scope.  Reusing the square-iso pakset on the hex lattice
breaks down into graded levels.  *Free*: existing diamond sprites
already overlay on the right footprint because the lattice was
picked to keep the W × W/2 bounding box (see "Sprite raster
choice" above) — ground textures, single-tile buildings, station
sprites land on roughly correct pixels with adjacency artefacts
where neighbouring diamonds overlap.  *Already landed*: base ground
tiles are now driven by pakset-provided `LightTexture` lightmaps,
so the synthetic colour-lightmap path no longer masks real hex art.
An alternative that wasn't taken — a flat-top hex alpha mask applied
at the blit (vertices at `(0, W/4)`, `(W/4, 0)`, `(3W/4, 0)`,
`(W, W/4)`, `(3W/4, W/2)`, `(W/4, W/2)`) — would have kept pakset
texture fidelity at the cost of still collapsing 6→4 slopes.  *Rough
but functional*: the 3rd hex axis (NE-SW) has no
square-pakset equivalent — way sprites, powerline crossings, and
several other tile decorations are missing 1/3 of the directions
outright.  60° pixel-art rotation of the N-S sprites is the
dumb-transform answer: visually jaggy because axis-aligned shading
breaks under rotation, but geometrically right.  For the 6 hex
bend variants vs. 4 square ones, ±60° rotation of two existing
bends fills the gap.  *Doesn't transform cleanly*: vehicles need
~18 distinct hex direction states (see "Vehicle direction enum"
above) vs. typically 4 sprite slots with ~8 rotation frames in
square pakset art — pixel rotation of vehicles breaks silhouette
and shading.
`get_dir()` already maps each hex edge onto the same-named slot
of the 8-direction sprite table,
so 8-direction vehicle art shows a sprite in every hex direction,
at an approximate angle.
Recommended order if a "playable but visibly stubby" demo is wanted before
real art lands: base ground now comes from LightTexture.
Skip the 60° way-bend rotations — that's where the cost-quality
curve gets bad and where pakset replacement most likely lands
first, so the transform code becomes pure dead weight.

**Out of scope.**  Real hex pakset art (see "Square-art-as-placeholder"
above for the placeholder roadmap).  Map rotation (currently fatal,
gated as unreachable) — orthogonal to the projection port.

## Depth-clip plane spec sits partially used

`hex_way_axis_t` + the "Depth-clip plane spec" docstring in
`display/hex_proj.h` define the per-axis vertical plane that splits
multi-layer way assets into Back / Front layers under hex projection;
pak-side mirror is in `hextrans-pak128/tools/threed/hex_synth.py`
(`HEX_DEPTH_CLIP_NORMAL`, `front_back_split`).
The bridge .dat contract
(`bridge_writer.cc::write_bridge_images`, `bridge_desc_t::img_t`)
keys segments and pillars by the 3 axes (`n_s`, `ne_sw`, `nw_se`)
and starts / ramps by the 6 hex edges, so the consumer side exists.
`rail_060_bridge_hex` now ships all 3 axes plus ramp / start cells
(pakset `93640ce`, `b01675d`),
so the NE-SW rule has a real bake to check against.
Re-evaluate the south-is-Front rule and the +x tie-break for N-S
against those NE-SW / NW-SE cells;
the rule was sketched against pak128's NS bridge convention only.

## Shore-water tile composition still square

`descriptor/ground_desc.cc::create_texture_from_tile`, reached via
`get_water_tile(slope, stage)`, splats `water_ani[0][stage]` with
hardcoded `±ref_w/2, ±ref_w/4` replication offsets to build a tileable
texture multiplied with the climate water texture.  Those offsets
tile a square silhouette; under hex the replication grid is wrong and
every shore tile drawn through this path produces wrong pixels.  Port
the replication geometry to hex alongside the rest of the wasser
display path.

## Save format version bump

Vertex topology, slope encoding and ribi width have shifted or will
shift further; the on-disk format is incompatible with upstream square
saves.  The `grund_t` slope field widened from byte to short in the
fork-reserved 124.900 save range so new six-corner slopes round-trip,
but this is only a narrow fix for post-port saves.  Once the
structural changes settle, either reject old upstream saves cleanly or
write a one-shot square→hex converter (hard because 4 corners do not
map cleanly to 6).

Nothing tests the terrain save/load round trip end to end,
including the map-edge and water branches
of the restore block in `grund_t::rdwr`.
Land one with the bump:
a headless check that generates a map with relief and water,
saves, reloads and compares every vertex height.

Save-format feature gates now have names in `simversion.h` rather
than floating against `SIM_SAVE_MINOR`.  Continue that convention for
future on-disk changes so later fork-version bumps do not silently
change the meaning of old readers.

## Multi-tile building footprint — still 2 of 6 hex axes

`building_desc_t::get_size(layout)` (`descriptor/building_desc.h`)
keeps the square-pak `(layout & 1) ? (size.y, size.x) : size` rule
under hex, so multi-tile cells only ever lay out along koord +x
(even layouts) or koord +y (odd layouts) — two of the six hex axis
directions.  The pak bakes 6 distinct rotations for asymmetric
multi-tile buildings, but only 2 of them align the model's long
axis with the cell axis the engine paints; the other 4 render the
building rotated relative to its footprint.  Next move: extend
`get_size` to recognise all 6 hex neighbour directions and lift
the consumers in `building_tile_desc_t::get_offset` and
`gebaeude_t::get_tile_list` to match.  Pak side then walks
all 6 directions in `iter_building_cells` /
`building_hex_viewpoint`'s slice generation instead of the binary
swap.  Soft trigger — only matters once residual misalignment is
the visible artefact in-engine.

Docks longer than one tile are refused on the NE-SW axis for now
(`dock_footprint_missing` in `tool/simtool.cc`, slope and flat docks alike),
because the footprint cannot follow the water there.
Next move: once `get_size` has the third axis,
delete that check and its message,
and make the NE and SW cases of `test_halt_build_harbour_all_directions`
and `test_halt_build_flat_dock_all_directions`
assert the same line of tiles as the other four directions.

## City growth anisotropy

City footprints come out elongated rather than round.
`tools/city_anisotropy.py` measures it: `--run` generates a flat empty
map (via the `-flatmap` headless hook), grid-seeds cities, grows each,
and dumps their building tiles; it maps the axial `(q, r)` through the
flat-top basis and reports the gyration tensor in both the axial and
physical frame plus the m=2/m=4/m=6 angular modes.  Reporting both
frames is the point — physical elongation with a round axial blob means
the basis shear, physical elongation with an elongated axial blob means
the growth algorithm itself biases a direction.  The tool's
`--self-test` validates the math (an axial-isotropic disk reads as
physical aspect sqrt3 at bearing 60 deg).

The gross elongation is fixed.  Pre-fix baseline (flat 192x192, 25
cities spaced 40 to stay clear of each other): physical aspect 1.68
along ~60°.  Two fixes landed: `bewerte_pos`/`bewerte_loc` widened from
4 to 6 rotations, and `build()`'s candidate-start sampling confined to
the physical disk inscribed in the `[lo,ur]` box-rhombus (radius^2 =
3*min(dx,dy)^2/16 via `koord.h`'s `physical_distance_sq`, the Euclidean
metric, not the hex-step `koord_distance`).  Physical aspect is now
~1.16 (round); axial rose to ~1.67, the footprint deliberately
axial-elongated to cancel the basis shear.  Two caveats when re-running:
the disk confinement packs cities denser (~100 tiles vs ~130 — revisit
only if a pakset looks cramped), and the aspect number is only honest at
spacing that keeps neighbours from colliding (spacing 30 reads ~2.3 from
competition for ground).

The per-city m=2 angular-density mode (not a hex symmetry) is the only
residual the gyration aspect can't see, and it is now understood, not
pending.  It used to be elevated in small cities (~0.3-0.4 below ~50
tiles) by the founding skeleton: pak64's 1-layout townhall always seeded
its road South (bearings {30°,90°} — a 2-fold set, exactly what m=2
reads).  Seeding the founding road isotropically over all 6 hex faces
(`check_bau_townhall`) fixed that — `--series` now reads m=2 ~0.18 for
small cities rising to ~0.32 for large (Pearson r = +0.26), i.e. the
remaining m=2 is intrinsic street-layout texture (houses lining up along
straight streets, read from building-tile coords) that grows with the
street network, not a removable bias.  Both tile-placement layers are
isotropic anyway (rule matching is 6-fold symmetric; `physical_distance_sq`
= q^2+qr+r^2 makes the `build()` sampling disk a true physical circle).
Don't chase it further: symmetrising the cityrules to full dihedral D6
was measured to do nothing, and the multi-tile-townhall NE/SW orientation
AGENTS.md flags is a footprint-model change, not an m=2 lever.

The flat map the baseline needs is produced by the `-flatmap N` test
hook in `simmain.cc`'s default-map path (size N, no mountains/rivers/
cities); it only fires under the headless none-backend build.  An empty
flat map needs no simulation, so the hook saves it directly and sets
`quit_simutrans` rather than running the loop to `-until` (which crawled
the quit month in NORMAL mode and hung in teardown).
`tools/city_anisotropy.py` generates and caches it automatically.

City road triangles at branch starts.
`stadt_t::build_road` connects a new road tile to every adjacent road tile.
On the hex grid a tile beside a straight road touches two of its tiles,
so the first tile of a side street closes a triangle
where the square grid gave a T junction.
Next move: count such triangles in grown cities
by extending the `tools/city_anisotropy.py` dump to road tiles,
and if they are common,
skip a neighbour that is adjacent to one already connected.
Limiting a city to two road axes (closed PR #122) is not the fix:
it stops rule-driven growth along the third axis.

## Building / crossing cluster

Buildings and intersections that bake the square 4-axis crossroads
into their gameplay logic, beyond the mechanical
`rotate90`→`rotate60` rename covered by the `rotate_perpendicular`
sweep in "ribi_t — audit surfaces".  Shared trigger is "hex
crossroads have a real design" — the design choice is **full
3-axis** (Y- and 6-way intersections are first-class, not
forbidden or projected onto 2 axes).  The traffic-light FSM has
landed under that policy; depot orientation, collision avoidance,
and airport layout still need porting against the same choice.

*Depot 3rd-axis layout.*
`tool_build_depot_t::tool_depot_aux` in `tool/simtool.cc`
maps a 4-layout depot's dead-end ribi through `ribi_t::ribi_to_layout`,
so all 6 directions get a layout;
`building_desc_t::adjust_layout` folds the NE and SW layouts
onto the images the pakset ships.
A 2-layout through depot, and every through stop,
picks its layout with `ribi_t::axis_to_layout`:
the S, SE and SW end give layouts 0, 1 and 5,
and `adjust_layout` folds 5 onto 1,
so NE-SW track shows the SE-NW image.
Real fix is a 3-axis depot and through-stop descriptor;
needs `depot_t` rotation handling
and pakset images for all three axes.

*Stop and depot facing is read from the way.*
`get_layout()` is folded for NE and SW,
so `hausbauer_t::get_facing_ribi` reads a stop's or depot's exit or axis
from its way and falls back to the layout.
A lone NE or SW terminal whose road was removed
can only be reconnected from its folded S or SE side.
Next move: store the real facing in `gebaeude_t` with the next save-format bump,
read it first in `get_facing_ribi`,
then drop the way-derived path and the fallback.

*Crossroads collision FSM.*
`vehicle_base_t::no_cars_blocking` (`vehicle_base.cc:491`)
and the related branches in `road_vehicle.cc::can_enter_tile`
were written against 4-way crossroads —
"across" = rotate90, "perpendicular" = rotate90,
and the right-of-way table assumes two axes meeting at 90°.
The `rotate_perpendicular` sweep makes them compile under hex
(one 60° step instead of one 90° step),
but a real 3- or 6-arm hex crossroads needs the FSM redesigned:
which arms have priority,
how is "across" defined when two arms meet at 60° vs. 120°,
what does the "exit same side" predicate mean with three axes.
Lights now signal Y-junctions correctly
while vehicles still resolve right-of-way against a 2-axis model,
so Y-junctions are a half-built case until this FSM is ported too —
visible as unpredictable collisions
when both axes go green-yellow on a real Y.

*Airport layout.*  `ai_passenger.cc:499` builds a hex diamond
using 4 of 6 hex edges (taxiway crosses on N-S and old-E-W = hex
SE-NW), which compiles and produces something playable but is not
a real hex airport.  Lands together with the other items here when
crossroads design lands.

## AI players — site selection still square

The C++ AIs (`player/ai*.cc`) run on hex engine pieces
(`koord_distance`, the way builder, hex-disc coverage),
but their hand-written site selection is still square.
Three sites matter most.
`ai_t::suche_platz` searches an axial ±cov rectangle around the factory,
which reaches 2·cov hex steps along ±(1,1),
so goods-AI rail stations miss the factory catchment
for about a third of directions;
reuse the `get_tile_list` + `hex_disc` dilation
that the road branch of `ai_goods_t::suche_platz1_platz2` already does.
`ai_passenger_t::walk_city` follows only SE/S/SW/NW roads;
loop over all 6.
Harbour→rail fails on NE/SW shores:
`suche_platz` takes the station length as `abs(size.x+size.y)`,
which is 0 along NE-SW,
`find_harbour` derives its second orientation by swapping x and y,
and the dock tool refuses a harbour longer than one tile there
(see "Multi-tile building footprint");
the AI should pick a one-tile harbour for such a shore.

The scripted AIs in `simutrans/ai/` are untouched,
but the script API's `dir` is already 6-way (`test_dir.nut`),
so `sqai` is a mechanical port:
its A* steps over `d = 1, 2, 4, 8` only
(SE, S, SW, NW, so targets to the north are unreachable),
its state flags 0x10/0x20 collide with the N/NE bits,
and its station search scans a square instead of the catchment disc.
`sqai_rail` needs a redesign:
its `check_station` reads the removed `dir.east` / `dir.west`,
and its double-track and station-extension code
is built on square offsets and the old straight ribis 5 / 10.
Hide it from the AI picker until then.
`test_ai_scripted.nut` (upstream #245/#246) attaches a fixture or shipped AI
to a `world.create_player(slot, 4)` player,
so the `sqai` port lands with a test there
that reaches a target north of the start.

The passenger AI's hub test and hub search
(`ai_passenger_t::get_our_hub` and `find_area_for_hub`)
share the axial `lo`/`ur` rectangle of `stadt_t`,
a sheared box rather than a hex disc,
and `find_airport_pos` / `find_harbour_pos` measure "out of town"
against it too.
The hub test has to use whatever the search uses,
or the AI stops finding the hubs it builds.
Next move: one hex-distance predicate on `stadt_t` for "tile is in the city",
used by all four together with the city growth geometry group below,
which decides what shape the city has.

Reusing a harbour or airport the AI already built reads the wrong tile.
`create_water_transport_vehicle` takes `get_basis_pos()` of a halt
that has a dock as the dock tile,
but that is the tile nearest the halt's centre
(`haltestelle_t::recalc_basis_pos`),
which can be its bus stop,
so the route test for the ship would start on land
and no second ferry from the town gets built.
The end-city branch of `create_water_transport_vehicle`
and `create_air_transport_vehicle` also assigns `start_hub = h`
where it means `end_hub`, as upstream does.
Next move: take the dock tile from the halt's tiles by station type,
assign `end_hub`,
and soak a map where one town gets a second route.

## Square-grid residue — audit backlog

A read-only audit (2026-10) of `tool/`, `builder/`, `vehicle/`, `obj/`,
`world/`, `dataobj/`, `script/`, `gui/` and `descriptor/`
found the sites below still reasoning in four directions or axial squares.
Each paragraph is one PR-sized fix group.
Groups touching code that an upstream 125 stage rewrites
wait for that stage (`documentation/upstream-125-merge.md`).

Route heuristics.
`way_builder_t::calc_distance` is axial Manhattan,
which overestimates NE/SW distances up to 2× and makes the A* heuristic
inadmissible there;
use the hex distance `(|dx| + |dy| + |dx + dy|) / 2`
on the out-of-box deltas.
`route_t::postprocess_water_route` can leave a non-adjacent gap
with its greedy substitution;
substitute only when the greedy path lands exactly on the end tile.
Next move: both fixes, with a NE-SW way-building test
that compares the built length with `koord_distance`.

Neighbourhood loops.
These "tiles around here" loops walk an axial 3×3 or `(-r..r)²` square,
which counts the non-adjacent `(1,1)` and `(-1,-1)` as neighbours:
`suche_nahe_haltestelle`, `tool_station_building_aux`
and `tool_make_stop_public_t::work` in `simtool.cc`;
`add_factory_to_fab_map` and
`factory_builder_t::find_random_construction_site`;
`tree_builder_t::create_forest` and `spawn_tree_near`;
the axial ellipse in `world/placefinder.cc`;
`fabrik_t::add_random_field` field rings;
and the nearby-name scan in `haltestelle_t::create_name`.
Next move: replace them with `koord_distance(a, b) <= 1`, `hex_disc(r)`
or `physical_distance_sq` as the shape needs
(AGENTS.md → "Axial coords vs physical hex symmetry"),
one PR, with a test per caller that a `(1,1)` offset is not adjacent.

City growth geometry.
`stadt_t::evaluate_size_res_com_ind` scores a 5×5 axial rhombus
and its street update uses an 8-neighbour table that paves `(1,1)`;
`stadt_t::build_road`'s corner check ignores the E and W corners;
`stadt_t::orient_city_building` only looks at S, SE, N and NW;
`stadt_t::new_month` spawns city cars in an axial square;
`stadt_t::generate_private_cars` is dead.
Next move: `koord_distance <= 2` for the scoring area,
`koord::neighbours` for the street update, six corners in `build_road`,
six neighbours plus `ribi_to_layout` in `orient_city_building`,
`hex_disc(3)` for car spawning, and delete `generate_private_cars`.
`tools/city_anisotropy.py --series` before and after
shows whether the footprint bias moves (see "City growth anisotropy").

Back-wall redraw sets.
After a height or building change, the tiles whose back walls must be
redrawn are the SE, S and SW neighbours
(`calc_back_image` reads NW, N and NE),
but the refresh loops in `stadt_t::build_city_house`
and `stadt_t::build_road`, `terraformer_t::raise_to` / `lower_to`,
`way_builder_t::do_terraforming`, `hausbauer_t::build` / `remove`
and the `simtool.cc` terraform tools still refresh the square set.
Next move: one helper that marks the three dependants dirty,
called from each site.

Map generation metrics.
`karte_t::calc_climate_map_region` smooths humidity with weights
that sum to 13 but divides by 16, so climates drift dry,
and its climate patches are axial ellipses;
`karte_t::distribute_cities` uses an axial cosine;
`karte_t::find_squares` looks for axial 5×5 squares;
`terraformer_t::lower_to` recalculates an axial ring;
`way_builder_t::build_river` has a square bend test,
as does the signal spacing in `tool_build_roadsign_t::mark_tiles`.
Next move: fix the humidity divisor first (a one-line change with a
measurable effect on climate shares), then port the shapes
to `hex_disc` / `physical_distance_sq`.

Descriptor visuals.
`tunnel_desc_t::slope_index` has no `*_double` slopes,
so tunnel portals vanish at the default `way_height_clearance` 2;
`objlist_t::intern_add_moving` orders NE movers like the square-era
NE diagonal;
the bridge preview in `tool_build_bridge_t::mark_tiles`
draws the delta-2 ramp with the single-height image
(`narrow_to_double` is missing).
Next move: one PR, with a scenario test that counts a portal's image ids
in the SDL build (headless CI registers no images,
so gate the assertion on an image being present).

GUI views.
`world_view_t::internal_draw` lays out the square lattice in every
info-window view;
the `gui_building_t` preview, the `minimap_t::draw` viewport-box
fallback, the station building selector (4 labels, 4 buttons),
`gui_rotation_item_t` in `extend_edit.cc` (4 rotation labels)
and the wind box in `settings_climates_stats_t` (0..3) are 4-way.
Next move: `world_view_t` through `hex_proj` first,
since every info window uses it.

Script-side constants.
`simutrans/script/hm_lib/hm_global.nut` keeps upstream's base-3 slope
constants (`UP=82` is a real hex slope; the hex values are 4097/4098,
and `N1`..`W2` come from `slope.*_narrow`);
the `SQAPI_DOC` lists in `api_simple.cc` and `api_settings.cc`
document removed direction constants;
`simtool-script-generator.cc` drops NE-SW segments in `write_command`
and `write_sign_at` assumes the old four-step sign cycle.
Next move: regenerate the constants from the C++ enums,
and a round-trip test that exports a NE-SW road with the script generator
and replays it.

Signals and signs on NE/SW.
`signal_t` and `roadsign_t` have four image slots and no NE/SW projection,
so signals, signs and traffic-light arms on NE-SW tracks draw nothing;
the tunnel-mouth masks clear single edges;
the private-way image index uses the SE bit (`1 - (dir & 1)`);
`state_to_direction[state & 7]` reads a 6-entry table.
Upstream's front and drive-on-left image lists (#223) are plumbed in
with the same four slots,
so a single-direction front image covers N, S, NW and SE only;
the diagonal lists stay unused (`calc_diagonal_index` is a tripwire).
Next move: a `state_to_direction`-style projection
(NE → N slot, SW → S slot) shared by `calc_image` and the front-image index,
image index by `straight_axis`,
and clamp the state on load.

## Pak reader child-count expectations are implicit

`obj_desc_t::get_child` now `dbg->fatal`s on out-of-bounds child
access, which catches the malformed-pak class in one place and
makes the fuzzer's recovery seam surface each offending reader by
name.  But the per-reader "I expect at least N children" contract
still lives only inside each reader's `register_obj` / desc
accessor code — there's no declarative way to read off how many
children a `bridge` / `vehicle` / `building` node needs.  A
fuzz_pak corpus that exercises one node per reader type would pin
each contract as a regression seed; opportunistic and low priority
until the next pak-loader bug or a CI gate motivates it.

## fuzz_pak CMake source-list inheritance fragility

`src/fuzz/CMakeLists.txt` builds `fuzz_pak`
(and now `fuzz_command` and `fuzz_command_preauth`,
which reuse the same `_SIM_*` variables)
by copying the simutrans target's `SOURCES` / `COMPILE_DEFINITIONS` /
`COMPILE_OPTIONS` / `LINK_LIBRARIES` via `get_target_property`,
because the readers must all link
to populate the `obj_reader_t::registered_readers` table.
This works today
but silently misses anything added through INTERFACE properties,
target-level `$<...>` generator-expression sources,
or new backend-specific quirks;
a future regression would appear as fuzz_pak missing a translation unit
rather than a build error.
Cleaner fix: restructure `simutrans` as `add_library(... OBJECT ...)`
plus a thin executable,
and have `fuzz_pak` link the OBJECT target
via `$<TARGET_OBJECTS:simutrans_objects>`.
Requires extracting the per-backend `main()`
(currently in `simsys_posix.cc` / `simsys_s2.cc` / `simsys_w.cc`)
into its own file so it can be added to the executable target only —
the `SIMUTRANS_NO_MAIN` guard in `simsys_posix.cc` is an interim shim
that survives only as long as the fuzzers are linux-only.
Two more consumers (`fuzz_command`, `fuzz_command_preauth`) now exist,
so the dedup is no longer hypothetical;
do it when a propagation gap actually bites
or when touching this CMake file for another reason.

## Command-handling fuzzer: go fork-based for the real campaign

`src/fuzz/fuzz_command.cc` fuzzes network command *handling* (the
post-parse surface `fuzz_nettool` / `fuzz_command_preauth` don't reach): it stands up one
real world and applies a fuzzed sequence of `nwc_tool_t` commands per
input, driving `do_command` → `tool->init`/`work`.  Input is a
hand-rolled byte reader (no libprotobuf-mutator / FuzzTest dependency
yet).

Throwaway benchmarks during bring-up settled the engine question, so
the numbers live here rather than as code.  Against pak64 (124-4) on a
32x32 map, in-process per-input world reset runs **~2 execs/sec** at
full world-gen, **~5** with cities/factories/tourist attractions off
(`karte_t::init` terrain/climate/grid alloc is the floor), and RSS
grows ~1.36 MB **per world rebuilt** and never returns.  That growth
is reset churn, not per-command, so a client streaming commands is not
a slow-DoS vector; and LeakSanitizer attributes only ~13 KB of it to
genuine lost pointers (one-time standup + a freed-only-at-shutdown
translator list) — the rest is reachable freelist/pool retention LSan
does not flag, so it is not a pluggable leak.  Standing the world up
once and `fork()`ing per test (kernel COW snapshot) ran **~94
execs/sec even under ASAN** with parent RSS flat (~20-45× in-process,
retention gone because each child reclaims its pages on exit).

So a real campaign wants **fork-after-init** (AFL++ fork mode or
Centipede); the in-process libFuzzer build is the structured-input dev
surface.  It is *not* wired into CI yet (unlike `fuzz_nettool` / `fuzz_pak`
/ `fuzz_command_preauth`): a `-runs=0` replay gate is feasible and would
lock in the handler fixes below, but it needs more plumbing than the
others — the `fuzz_command` target added to the "Replay fuzz corpora"
step, pak64 + base files on disk in that job (the parser-only fuzzers
need neither), `detect_leaks=0` (per the incomplete per-world teardown
above, vs the others' `detect_leaks=1`), an absolute `SIMUTRANS_FUZZ_BASE`,
and a committed seed corpus under `src/fuzz/corpus/command/` (a generator
hitting each fixed tool + the minimized repros).  Concrete next steps:
(1) wire the harness to AFL++'s `aflpp_driver` with a deferred forkserver
(`__AFL_INIT()` after standup — the harness already pins
`env_t::num_threads=1` so the fork is safe) and re-measure without the
ASAN fork tax; (2) deepen the structured input layer.  Tool *selection*
now folds the fuzz bytes onto the dense valid id space (`fuzz_tool_id`),
so iterations reach a real handler instead of bouncing off `create_tool`
returning NULL; the remaining gap is the per-tool payload — `default_param`
is a raw fuzzed string and `custom_data` is not fuzzed at all, so
`two_click_tool_t` start-pos / click-pair state and the int-parsed params
of the change_* tools stay shallow.  A grammar (libprotobuf-mutator keeps
the existing CMake+CI infra; FuzzTest if the in-C++ domain ergonomics are
worth the build integration) would model the click-pair sequence and the
custom_data blob per tool.  Seed corpus:
the `tests/*.nut` scenario scripts already drive real tool sequences.
Run with `ASAN_OPTIONS=detect_leaks=0`.  The pre-auth surface
(`init_tool` / `rdwr_custom_data` with attacker `tool_id` before any
auth check) is covered by `fuzz_command_preauth` and lives in CI.

Harness fidelity caveats worth keeping in mind when reading its findings.
`SIMUTRANS_FUZZ_BASE` must be an *absolute* path for `-fork` mode: standup
does a relative `dr_chdir`, so a re-exec'd child (already inside the data
dir from the parent's chdir) would chdir to a non-existent nested dir and
fatal — every fork child then aborts during standup and the run reports
bogus crashes.  The harness sets `env_t::networkmode = true` *after* each
`welt->init()` (init calls `network_core_shutdown()` which clears it) to
model the server `do_command` environment — without it the
`if(!networkmode)` GUI-window branches a real server never runs (e.g.
`dialog_enlarge_map_t::init`'s map re-gen) fire and dangle a settings
window across worlds, which surfaces as a `settings_general_stats_t::read`
use-after-free that is a harness artifact, not a server bug.  Tools that
re-enter `karte_t::init`/`load`/`stop`/`switch_server` synchronously
(`TOOL_QUIT`, `TOOL_WORK_WORLD`, `TOOL_LOAD_SCENARIO`) are skipped — see
`is_world_lifecycle_tool`.

## Network tool handlers: attacker default_param / pos hardening

With `fuzz_command` now reaching real handlers (`fuzz_tool_id` folds the
fuzz bytes onto the dense valid id space), it enumerates two classes of
pre-existing crash in the network-broadcast tool handlers, where the
single-player invariant (GUI supplies a well-formed `default_param` and an
on-map `pos`) does not hold for an attacker-controlled `nwc_tool_t`.  The
NULL-`default_param` derefs found so far are fixed (the `*::init` guards in
`simtool.cc` and the NULL guards in `way_builder_t::get_desc` /
`roadsign_t::find_desc` / `hausbauer_t::find_tile`, matching the existing
bridge/tunnel ones), plus `tool_change_line_t::init`'s terminator over-read,
the `default_param` length over-reads in the industry build tools
(`has_factory_param`) and `tool_work_world_t::init`,
and `tool_transformer_t::work` / `tool_build_depot_t::get_waytype`'s
unchecked tile lookups.  Two sub-classes remain, each a per-site missing
guard rather than one central fix (a `do_command` bounds gate would wrongly
reject the non-positional tools that legitimately carry `koord3d::invalid`):

*Off-map `pos` → NULL ground deref.*  `welt->lookup_kartenboden(pos)` /
`welt->lookup(pos)` derefed without an `if(gr)` guard in tool `work()`s;
~13 of 64 lookup sites in `simtool.cc` already guard, the rest are
candidates.  Next move: continue the campaign, add `if(!gr) return "";`
per offending site (the pattern the guarded sites already use).

*`default_param` content that is not a length problem.*
Three per-site crashes of the same attacker-input shape remain,
each reproduced under ASAN with one `command_x` call.
`tool_build_factory_t` and `tool_build_land_chain_t` turn the rotation byte
into `(c - '0') % layouts` unchecked
(`tool_build_house_t` has the same expression, not reproduced),
so `"1!1024,TANKE"` gives a negative rotation
and `building_tile_desc_t::get_tile` aborts;
`tool_build_station_t::get_desc` already range-checks its rotation.
`tool_plant_groundobj_t::work` dereferences `desc` before checking it,
so `"00,nonexistent"` is a NULL dereference.
`tool_add_city_t::work` writes through the NULL result of its second `strchr`,
so a parameter with exactly one comma (`"5,nonexistent"`) writes to address 0.
Next move: make the three `init()`s reject a rotation byte that is neither
a digit nor `#` (`A` too for houses),
add `if (!desc) return "";` after the groundobj lookup,
move the stray `*p = 0;` into the `if` that assigns `p`,
and add an invalid-parameter test for each.

*World-lifecycle tools over the wire (unverified, read from the code).*
`tool_quit_t`, `tool_load_scenario_t` and `tool_work_world_t`
set `WFL_LOCAL | WFL_NO_CHK`,
and `simmenu.h` says such calls never go to the server.
But `nwc_tool_t::clone` clears `WFL_NO_CHK` only for non-server senders,
skips the unlock check for `player_nr >= PLAYER_UNOWNED`,
and `do_command` then calls `init()`,
so a connected client may make the server quit, load or save.
`tools/nwc_protocol_test` sends tools with `exec=True`,
which the server rejects before `clone()`, so it never covers this.
Next move: a wire test that joins and sends `TOOL_QUIT`
with `exec=False` and `player_nr=15`;
if the server quits, have `clone()` drop `WFL_LOCAL` tools from non-server senders.

## Descriptor-driven allocation OOM in pak readers

Several readers turn a decoded count/length field into an
allocation (`desc->alloc(decode_*(p))`, `new[count]`) without first
checking the count against the bytes remaining — a hostile value
drives a multi-GB `new[]`, and in headless builds the copy-site
bounds check that would otherwise catch the short read is skipped,
so the alloc is the only guard point.
`image_reader` versions 0–2 are fixed and show the shape:
cap `len * 2` against `node.size` before the alloc.
Version 3 still calls `desc->alloc((node.size-10)/2)`
before it reads the node's tenth byte,
so a 9-byte node wraps the unsigned subtraction into a ~4 GiB request.
Next move: reject `node.size < 10` before that alloc.
No other reader allocates from a decoded count today.
Trigger: fuzz_pak active mutation surfaces the next one on top of
the seeded corpus.

## fuzz_pak UBSAN-class reader findings (post-OOB-read batch)

An active fuzz_pak campaign (dictionary of the obj_type tags from
`objversion.h`, ~5M execs) on top of the seeded corpus cleared the two
spatial OOB *reads* — the xref name fed unterminated into
`xref_to_resolve`'s strcmp, and `vehicle_reader` indexing the 8-entry
legacy `convert_from_old` table with an unchecked uint8 (both fixed,
seeds `xref_unterminated_name` / `vehicle_legacy_waytype_oob`).  ~700
inputs remain that trip UBSAN but not ASAN; none corrupt memory, but
each aborts under CI's `abort_on_error`, so they gate any move to fuzz
under UBSAN with a non-empty mutation budget.

The largest actionable cluster is a null `get_name()`: a desc whose
name TEXT child is absent returns a null name, which `register_obj`
then hands to strcmp / a hashtable.  Verified at
`tree_builder_t::register_desc` (hashtable `remove`) and
`tunnel_reader_t::convert_old_tunnel` (`strcmp(...,"RoadTunnel")`);
the same `get_name()`-is-non-null assumption is spread across the
register paths, so the fix wants to be one guard, not one per site —
reject a desc with a null name in `read_nodes` after `read_node`
returns (the descs that legitimately lack a name need enumerating
first), or make the name accessor never return null.  A sibling of this
bug is a present-but-wrong-type name child: `get_name()` does
`get_child<text_desc_t>(0)` and downcasts unconditionally, so a node
whose child 0 is some other desc is read as a `text_desc_t`.  The
`TRACK_DESCRIPTORS` virtual dtor makes that downcast visible to UBSAN's
`-fsanitize=vptr` (disabled in `fuzz_pak` to keep the teardown vtable
scoped to destruction — re-enabling it is a ready detector once the
guard above lands).

The rest are narrower per-reader decodes: invalid `waytype_t` enum
loads in `crossing_reader` (value is sound, only used in a
`PAKSET_INFO` print — validate against the known set on read);
invalid `building_desc_t::flag_t` / `btype` / `old_btyp::typ` loads
in `building_reader` (validate on decode); and a signed-int overflow
in `building_reader`'s `2 + size.x*size.y*layouts` child-count test
(widen the operands before multiplying).  Work them smallest-risk
first behind the null-name guard, a minimized seed per fix as above.
The crash artifacts are not committed — rerun the campaign to
resurface them.

## TRACK_DESCRIPTORS engine teardown: verify and extend to reload

`free_all_descriptors()` is wired into `simu_main`'s shutdown under
`TRACK_DESCRIPTORS` (after `gfx->exit()`) and is compile-verified, but
its leak-cleanliness on a real engine run is not — this env can't run
the GUI.  The risk is a static descriptor-holder still dereferencing a
freed node *after* the free call (`skinverwaltung_t`, the tool lists,
anything registered at static init), which would surface as an ASAN
use-after-free at exit.  Next move: build `-DSIMUTRANS_TRACK_DESCRIPTORS=ON`
with ASAN, run a headless load-then-quit, and resolve any post-free
deref by reordering the call or clearing the offending holder before it.

`fuzz_pak` given a complete pak64 file
(`bridge.ClassicRoad.pak`, `ground_obj.flowers.pak`)
hits a heap-use-after-free when libFuzzer runs the input a second time:
`free_all_descriptors()` frees the descriptors,
but the per-type tables they were registered into
(`bridge_builder_t::desc_table`, the ground-object list, …)
keep their name pointers,
and the next `register_desc` calls `remove(name)` on them.
The committed corpus holds no complete descriptor, so CI does not see it.
Next move: clear those tables in `free_all_descriptors()`
(one `clear` per `register_desc` owner),
then commit a minimized real-pak seed under `src/fuzz/corpus/pak/`.

The production payoff that motivated the engine-side option is freeing
on *pakset reload* (GUI pakset switch, server map reload), where the
loading registries are overwritten without freeing the previous
descriptors — a real in-process leak / RSS growth, not just an exit-time
diagnostic.  Once the shutdown path is confirmed clean, call
`free_all_descriptors()` at the reload site, guarded by the same
no-live-world precondition (descriptors must not be referenced by an
existing `karte_t` when freed).

## libcurl rollback gate retirement

Legacy in-house HTTP socket code in `network_file_transfer.cc`
(the `network_http_get` / `_post` / `_get_file` bodies)
and the desktop fan-out in `pakset_downloader.cc`
(urlmon / PowerShell / system curl)
live under `#else USE_CURL` as a downstream fallback
(`-DSIMUTRANS_USE_CURL=OFF`).
`parse_http_url` is compiled unconditionally,
but its only caller is the legacy fan-out in `pakset_downloader.cc`,
so it retires with it.
Delete the legacy branch together with the cmake / autoconf gate
after the next release has shipped
without a regression report against the libcurl path.
The three `network_http_get_file` bugs called out
in `documentation/libcurl-port.md`
(relative-redirect dropout, `:80`-double-append, mis-tagged error string)
remain present in the legacy branch —
fixing them there is wasted work ahead of retirement.

## Pakset download hardening leftovers

The `#if 0` block at the bottom of `pakset_downloader.cc` (the
"curl code broken for now" libcurl+libzip variant) carries its own
`extract_pak_from_zip` that builds the output path as
`sprintf("%s%s", env_t::install_dir, target_filename)` with no `..`
check — the same Zip Slip the live miniz path was just fixed for
(`pak_entry_name_is_safe`). It is dead today, but if anyone revives
it they reintroduce the hole. Next move: delete the block (the live
`USE_CURL` path supersedes it) or route it through
`pak_entry_name_is_safe` before the first write. Delete is
preferred — nothing references it.

Defense-in-depth: the `src/paksetinfo.h` catalogue ships almost
entirely `http://` URLs, so a passive MITM can swap the downloaded
archive. The traversal guard now contains the blast radius to "a
malicious-but-in-tree pakset" (same trust level as any downloaded
pak), but switching the generator (`get_pak.sh`) to emit `https://`
where the mirror supports it closes the swap itself. Deferred:
needs a per-mirror TLS check (sourceforge / github / codeberg do;
some of the smaller hosts may not) so it can't be a blind
find-replace.
