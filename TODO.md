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
(`-1` = don't care, `0` = no way) and tripwires on string rows because
Squirrel indexes `row[x]` to ASCII codes (so the legacy `"..5....."`
shorthand silently produced wrong expected values), and 6-bit hex
ribis don't fit a single digit besides.  When migrating a pending test,
the original patterns assume 4-bit square-axis paths, so the actual
hex-pathfinder route has to be reasoned out (or read off a temporary
dump) — at least for `wt_road` the builder hugs allowed/buildable tiles
into a hex staircase rather than the square L-path.  Worked examples:
`test_way_road_build_{straight, parallel, crossing, below_powerline}`,
`test_scenario_rules_allow_forbid_tool_stacked_{rect, cube}`,
`test_way_road_upgrade_{downgrade, crossing}`,
`test_way_tram_build_{on_road, in_tunel}`,
`test_way_road_cityroad_*`,
`test_way_tunnel_build_{straight, up_down, make_public}`; the tunnel
ones demonstrate the 6-vertex hex-hill scaffold from the AGENTS.md
primer, and `test_way_tram_build_in_tunel` additionally shows a 2-axis
tunnel cross (S-axis road + SE-axis tram) threading one buried hex tile
with ribi 16|2|8|1=27.

**Legacy `slope.east` / `slope.west` in tests.**  The square-era 2-corner
diagonals are still admitted as way slopes by the post-slope-way
predicate (side-chord branch with samples both at 1), but the names
project poorly onto the hex axes and read as historical baggage.  Four
call sites (`test_building_rotate_harbour`, the "east-west direction"
block in `test_depot_build_on_bridge_end` and the matching block in
`test_halt_build_on_bridge_end`, `test_powerline_remove_powerbridge`)
migrated to `slope.southeast_narrow` / `slope.northwest_narrow`, the hex axis they were
already projecting onto.  `test_scenario_rules_allow_forbid_way_tool_cube`
and `test_terraform_climate_from_water` both still use `2*slope.east`;
left untouched because both functions are already HEX-PORT PENDING
and need holistic rewrites.  Note the literal `2*slope.east` no
longer denotes a real terrain slope under hex's base-4 encoding —
square-era `2 * narrow == double` is gone, replaced by named
`*_double` constants and the `slope_t::narrow_to_double()` helper —
so when these tests come back, the `2*slope.east` literals migrate
to `slope.southeast_double` / `slope.northwest_double` (whichever
the hex axis intent is).

**Powerline 3rd hex axis.**  `test_powerline_connect` and
`test_powerline_ways` each expect crossings / powerlines on the 2
square-era axes (N-S and old E-W).  Under hex there are 3 axes and
the 3rd (NE-SW) has no powerline crossing sprite or connection FSM
support (`leitung2.cc` diagonal-image table is keyed on 4 old-combo
values).  Restore after the crossing-cluster / 3rd-axis work lands.

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

**Flood-fill / region walkers.**
`tool_change_water_height_t` in `simtool.cc` is hex-aware (6-neighbour
flood, shared-edge corner heights on the *current* tile per
`vertex_owners`, six-corner apply + `set_grid_hgt_nocheck`).  Scenario
`test_terraform_raise_lower_water_level` stays commented out: it still
uses a rectangular `terraform_volcano` scaffold and square-shaped
flood expectations — restore after a hex-shaped scaffold (a hex ring
of 6R cells at axial distance R from the centre) replaces the 4-side
square ring, and the per-subcase coord and flood-pattern assertions
get rewritten.

**Missing `terraform=` in `command_x.build_way`.**
`command_x.build_way` in the Squirrel API has no `terraform=` flag,
so the `terraform_flag` paths in `way_builder_t` (set by city / AI
builders via `simcity.cc` and `ai*.cc`) have no scenario-test
coverage.  Add a `terraform` bool mirroring `straight` /
`keep_city_roads`; wire through the `TOOL_BUILD_WAY` param
encoding in `script/api/api_command.cc:308-316`.  Lands alongside
the first scenario test that exercises AI / city-shape
terraforming — including a regression test for the
`check_terraforming` widening + corner-index fixes that currently
have no end-to-end coverage.

**`ASSERT_THROWS` helper for scenario tests.**  47 sites across
`tests/tests/*.nut` hand-roll the same `local caught = false; try { ...
} catch (e) { ASSERT_EQUAL(e, "<msg>"); caught = true } ASSERT_TRUE(caught)`
block to assert a tool call throws an expected message.  Add
`ASSERT_THROWS(expected_msg, callable)` to `test_helpers.nut` and
collapse the sites to one line each.  Soft trigger: fold in next time
a test file in this set is being rewritten anyway (the hex migration
of the remaining HEX-PORT PENDING tests touches several of them).

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
satisfied by `max_height == 0 || max_height >= 2` — the desc's
declared height cap, not whether the pakset ships matching `*_Start2`
/ `*_Ramp2` art.  Under pak64 that art is never shipped; the engine
still builds planar double-slope and 2× flat-tile bridges and renders
empty or with the single-edge sprite at the wrong height.  AGENTS.md
"reuse existing square-tile sprites where they still fit" — crooked
graphics rather than refusing to build.  `bridge_img_valid` remains
the one img-side gate (rejects the `(img_t)-1` "no slot for this
slope" sentinel that catches `all_up_slope` and other shapes the
get_start switch doesn't cover).  Restore art-presence probes (or
replace them with explicit per-slot pakset declarations) once Start2
/ Ramp2 art is shipped.

## build_bridge auto-extension misses delta-2 ramps

`bridge_builder_t::build_bridge` (`brueckenbauer.cc:905`) walks
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
`frontimageup` / `backimageup` under the square-era pattern: 4 of
6 hex edges, missing the two third-axis edges (raised E+SE and
raised NE+E pairs, see `koord.cc` neighbour case table).  Needs
the same widening that `way_writer.cc` got — 18 slots covering
all 6 hex edges × {narrow, wide, double}, indexed by
`way_obj_desc::get_slope_image_id`.  Land alongside the first hex
sloped way-object asset.  `ground_writer.cc` was already widened
to scan `slope_t::max_slopes` so sparse hex slope indices
round-trip into `.pak`.

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
`way_desc_t::get_image_id` maps missing `type_river` multi-ribi
slots to an available single-edge river sprite, and `weg_t::calc_image`
falls back from missing river slope sprites to the flat-ribi path.
This is not a general way-art shim; it exists because natural map
generation can create rivers before pak128 has complete 64-slot river
art.  Delete the fallback when `landscape.rivers` ships real hex flat
and slope river sprites.

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

`karte_t::rotate90`'s heightmap-rotation loop is still the legacy
90° square formula — 90° is not a hex symmetry, so the result is
deterministic but geometrically wrong.  `perlin_hoehe`'s own
rotation code shares the bug.  Both land together with a real hex
rotation (or a `dbg->fatal` refusal), tied to the viewport port.

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

## Map storage shape — open architectural choice

Tiles are stored as a `W × H` rhombus in axial `(q, r)`, indexed
`plan[q + r*W]`.  Under the flat-top hex projection that rhombus
becomes a strongly-skewed parallelogram in world space — y-extent
~2.6× x-extent at `W = H`, NE / SW corners far apart,
look-around-from-centre asymmetric.  A regular hex region — all
tiles with axial distance ≤ d from the centre, `3d² + 3d + 1` tiles
— would be spatially compact, give roughly equal sightlines in
every direction, and reduce the world-create UI to a single radius.

Not in flight, flagged so it doesn't get closed off by side-effect.
It touches every `plan[q + r*W]` indexing site, every map-iteration
loop, the save format (or leaves corner cells empty for round-trip
compatibility), the world-create / `enlarge_map_frame` UI which
currently asks for `(W, H)`, and the `init_perlin_map` cache —
sized today against rhombus-in-world-space extents and would want
recomputing for whatever shape we pick.  The
`koord_random` / `clip_min` / `clip_max` "rhombus in world space"
caveat under "ribi_t — audit surfaces" is the same question viewed
from a different direction.  Pick a direction before any of those
adjacent items gets done in a way that bakes in rhombus
assumptions.

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

Three writers prototype the new convention — `way` (64-slot ribi
protocol, `Image[se]` / `Image[se_nw]` / … keys; slope-up subset is
still 4-cardinal, see "Way slope-up sprites" above), `bridge` (3
hex-axis segments + 6 hex-edge starts/ramps, hex-aware enum, tripwire
on pre-port saves; commit `1e21a2a`), and `tunnel` (6 hex-edge
portals via `hex_keys::edge_names`, low-edge naming matching bridge
— inverting the upstream high-edge convention so legacy paks render
permuted, same "loads without fatal but renders wrong" breakage as
the way ribi key change; broad-portal `l/r/m` neighbours orthogonal
to direction).  Shared key vocabulary lives in
`descriptor/writer/hex_keys.h` (`axis_names[3]`, `edge_names[6]`);
subsequent writers consume the constants on port rather than
reinvent.

Four direction-keyed writers still embed their own square-era
tables.  Each is gated on its first hex pakset asset — mechanical
key renames without matching art are an invisible no-op, so
migrations land per-family with the asset that uses them.

**`way_obj_writer.cc`** still iterates the 4-bit ribi space
(`ribi_codes[26]` covers `0..15` plus 5 crossing variants); the
matching `way_obj_desc.h:56-60` `ribi_to_extra[16]` table is gated on
`ribi < 16`.  Widens to the 64-slot ribi-as-key protocol that
`way_writer` uses; no name vocabulary needed.  The 4-cardinal
slope-up loop (`slope = 3, 6, 9, 12` for `frontimageup` /
`backimageup`) is the same square-era pattern called out in "Way
slope-up sprites" above; both retire alongside the first hex wayobj
asset.

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

**`vehicle_writer.cc:179-181`** has `dir_codes[8]` for sprite-facing
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

**Aircraft landing-circle 4-of-6 runways.**  `aircraft_t::calc_route`
in `air_vehicle.cc:415` switches on the runway end's `get_weg_ribi`
to pick an offset into the 16-step `circle_koord` hold-pattern
table — N=0, NW=4, S=8, SE=12.  NE and SW runways fall through to
the implicit `offset=0` default and put aircraft on those runways
into the N-approach hold pattern.  The 16-step `circle_koord` table
is itself hand-rolled for 4-direction airports — widening the
switch alone isn't enough; the table needs new step sequences for
the NE/SW approach geometries.  The AI airport builder lays only
N-S and SE-NW (see "Runway layout" above), so the path is
reachable from player-built NE/SW runways and pre-port savegames,
not from AI gameplay.  Lands together with a hex-aware hold-pattern
geometry — likely 6-direction symmetric circles rather than 4.

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

**Phase B — per-tile detail.**  Base ground sprites, grid borders,
markers and cliff back walls all read directly from pakset-provided
descriptors (`LightTexture`, `Borders`, `Marker`, `Slopes` /
`Basement`) indexed by raw `slope_t` or `(wall, index)`; the old
engine-side `synth_overlay` family is gone, the legacy 1D 22-cell
`Slopes` / `Basement` packing is gone, and the upstream Fabio Gonella
rock-photo art for cliff faces is preserved under
`hextrans-pak128/landscape/grounds/back_wall/src/` for future texture
supervision.
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
The snowline path — `get_alpha_tile(slope)` derives mask =
`high_corners_of(slope)`, which is an isolated-bit mask on slopes
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
`f9b0a072` relied on.  Drift between the `(slope, water_corners)`
combos `grund.cc::display` emits and the set `texture-shore/`
ships is therefore invisible — the overlay just disappears.  The
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
ground-lightmap path as climate ground.  Deep water still comes from
the pakset `Water` animation block.  6-edge way / wall / ribi-keyed
sprite tables remain 4-edge with `rotate60` stubs.

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
(`vehicle_base_t::calc_set_direction` and friends), label / halt
screen-anchor positions.  The label / halt group rides along on
`get_screen_coord` so positions become hex-correct automatically;
per-tile drawing under each anchor still assumes square geometry
until phase B lands.  `vehicle_base_t::calc_set_direction` now
enumerates 6 hex axes but the `dx,dy` magnitudes are still the
legacy square-iso pair (`(±2, ±1)` for the four edges that survive
the rename, `(±4, 0)` for NE/SW); under the hex lattice N/S
neighbours are at screen distance `2u` while NE/SW are at `u·√10`
≈ `3.16u`, so NE/SW vehicles visibly drift off-tile mid-hop on
straight segments.  Rebase the per-edge offsets on the lattice and
match the diagonal-step substep count to the longer hex chord.

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
tiles are at `2·u` — the +q axis is ~14% off true regular hex.
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
and shading; the realistic plan is `get_dir()` projecting 6 hex
edges onto whichever 4-slot table the current sprite has, accepting
visibly wrong direction in roughly 1/3 of cases.  Recommended
order if a "playable but visibly stubby" demo is wanted before
real art lands: base ground now comes from LightTexture, so next is
`get_dir()` 6→4 projection for vehicles (has to land anyway).
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
pak-side mirror is in `hextrans-pak128/tools/3d/hex_synth.py`
(`HEX_DEPTH_CLIP_NORMAL`, `front_back_split`).  The bridge .dat
contract (`bridge_writer.cc::names`, `bridge_desc_t::img_t`) now
keys segments and pillars by the 3 axes (`ns`, `ne_sw`, `nw_se`)
and starts / ramps by the 6 hex edges, so the consumer side
exists.  Coverage is still partial: `rail_060_bridge_hex` only
ships 2 of 3 axes (NS and NW-SE) and no ramp / start cells, so
the NE-SW depth-clip rule is not yet exercised by a real bake.
Re-evaluate the south-is-Front rule and the +x tie-break for N-S
when the third-axis bake lands — the rule was sketched against
pak128's NS bridge convention only, not stress-tested against
NE-SW / NW-SE bakes that don't exist yet.

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

*Depot 3rd-axis layout.*  `tool_build_depot_t::tool_depot_aux` in
`tool/simtool.cc` picks `layout = 0` (N-S axis) or `layout = 1`
(SE-NW = old E-W) from the way's ribi.  The 3rd hex axis (NE-SW)
falls through to layout 0, so depots on NE-SW track render
mis-oriented.  Real fix is a 3-layout depot descriptor; needs
`depot_t` rotation handling + pakset depot images for all three
axes.

*Crossroads collision FSM.*  `vehicle_base.cc::is_blocking`
(`vehicle_base.cc:533`) and the related branches in
`road_vehicle.cc::can_enter_tile` were written against 4-way
crossroads — "across" = rotate90, "perpendicular" = rotate90, and
the right-of-way table assumes two axes meeting at 90°.  The
`rotate_perpendicular` sweep makes them compile under hex (one
60° step instead of one 90° step), but a real 3- or 6-arm hex
crossroads needs the FSM redesigned: which arms have priority,
how is "across" defined when two arms meet at 60° vs. 120°, what
does the "exit same side" predicate mean with three axes.
Lights now signal Y-junctions correctly while vehicles still
resolve right-of-way against a 2-axis model, so Y-junctions are
a half-built case until this FSM is ported too — visible as
unpredictable collisions when both axes go green-yellow on a
real Y.

*Airport layout.*  Tracked separately under `Runway layout` in
the test section above — `ai_passenger.cc:499` builds a hex
diamond using 4 of 6 hex edges, which compiles and produces
something playable but is not a real hex airport.  Lands together
with the other items here when crossroads design lands.

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

`src/fuzz/CMakeLists.txt` builds `fuzz_pak` by copying the
simutrans target's `SOURCES` / `COMPILE_DEFINITIONS` /
`COMPILE_OPTIONS` / `LINK_LIBRARIES` via `get_target_property`,
because the readers must all link to populate the
`obj_reader_t::registered_readers` table.  This works today but
silently misses anything added through INTERFACE properties,
target-level `$<...>` generator-expression sources, or new
backend-specific quirks; a future regression would appear as
fuzz_pak missing a translation unit rather than a build error.
Cleaner fix: restructure `simutrans` as `add_library(... OBJECT
...)` plus a thin executable, and have `fuzz_pak` link the OBJECT
target via `$<TARGET_OBJECTS:simutrans_objects>`.  Requires
extracting the per-backend `main()` (currently in
`simsys_posix.cc` / `simsys_s2.cc` / `simsys_w.cc`) into its own
file so it can be added to the executable target only — the
`SIMUTRANS_NO_MAIN` guard in `simsys_posix.cc` is an interim shim
that survives only as long as fuzz_pak is linux-only.  Trigger:
when a propagation gap actually bites, or when another tool grows
a similar "link the whole simutrans source set" need.

## Pak-fuzzer per-iteration teardown

`fuzz_pak` (`src/fuzz/fuzz_pak.cc`) leaks every descriptor it
constructs because there's no per-iteration teardown of
`pakset_manager_t::loaded` / `unresolved` / the desc trees
themselves, so the CI replay path needs `ASAN_OPTIONS=detect_leaks=0`
and active fuzz RSS grows linearly.  Fix is a
`pakset_manager_t::reset()` that drops the loaded registry and
deletes the desc trees; tricky because readers share image pointers
via `images_adlers` deduplication, so a naive delete double-frees.
The same boundary would also wire `longjmp`-driven `dbg->fatal`
recovery in `fuzz_pak.cc` to a destructor-safe path — today the
longjmp out of `log_t::set_fatal_hook` skips RAII unwinding (UB in
strict C++, harmless in practice but reinforces the leak).
Trigger: when active-fuzz RSS growth or strict leak checking
becomes a blocker.

## image_reader dedup-table UAF on failed-sibling delete

`image_reader_t::read_node` registers each unique image desc in a
static `images_adlers<adler, image_t*>` for content-deduplication.
A malformed pak can hand an IMG node a non-zero `nchildren` even
though IMG nodes don't normally take children; when one of those
synthetic children fails to read, `pakset_manager_t::read_nodes` at
`pakset_manager.cc:369` `delete data`s the IMG desc that
`read_node` just registered.  `images_adlers` keeps the freed
pointer; a later image with a matching adler hits the dangling
entry at `image_reader.cc:218` (`a.x != b.x`) and reads freed
memory.  Fix candidates: (a) clear `images_adlers` on
`load_pak_from_fp` entry, losing cross-load dedup but trivially
correct; or (b) extend `~image_t()` (or override
`image_t::operator delete`) to drop the entry from
`images_adlers`, so any `delete` on a registered image cleans the
table transparently.  Lands well alongside the
`pakset_manager_t::reset()` work in "Pak-fuzzer per-iteration
teardown" above — both need a clean ownership story for image
descs.  No small seed pins this one — minimisation collapses the
UAF inputs onto simpler bug classes (the OOMs and the
`obj_named_desc_t::get_name` NULL deref below); the deeper-run
crash artifacts are reproducible locally but too large to commit.

## Descriptor-driven allocation OOM in image_reader (and likely others)

`image_reader_t::read_node` calls `desc->alloc(decode_uint32(p))`
where the uint32 is a `len` field read straight from the pak.  The
spike's `decode_*` bounds-check guards the buffer read but not the
*value* — a hostile `len` close to 2³² drives `image_t::alloc` into
a multi-GB `operator new[]`.  Same shape exists wherever a reader
turns a decoded count/length field into an allocation (vehicle
pixel arrays, building tile lists, …).  Fix: cap each
allocation-driving count against the buffer's remaining bytes
before alloc — for image specifically, `len * sizeof(uint16) <=
buf_end_ - p`.  `node_body` already has the primitive: its private
`require(n)` tests that `n` bytes remain, and the bulk pixel read
now computes `len * 2` at the copy site — the fix is to call that
check on `len * 2` (exposed as a public method) just above
`desc->alloc`.  The same shape elsewhere (vehicle pixel arrays,
building tile lists, …) wants a shared `obj_reader_t` helper to
avoid per-reader whack-a-mole.  Trigger: fuzz_pak active mutation
surfaces this immediately on top of the seeded corpus; harden once
a structural fix lands.

## libcurl rollback gate retirement

Legacy in-house HTTP socket code in `network_file_transfer.cc`
(the `network_http_get` / `_post` / `_get_file` bodies and the
`parse_http_url` helper, only reachable from those bodies now),
and the desktop fan-out in `pakset_downloader.cc` (urlmon /
PowerShell / system curl), all live under `#else USE_CURL` as a
downstream fallback (`-DSIMUTRANS_USE_CURL=OFF`).  Delete the
legacy branch together with the cmake / autoconf gate after the
next release has shipped without a regression report against the
libcurl path.  The three `network_http_get_file` bugs called out
in `documentation/libcurl-port.md` (relative-redirect dropout,
`:80`-double-append, mis-tagged error string) remain present in
the legacy branch — fixing them there is wasted work ahead of
retirement.

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
