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

**`ASSERT_WAY_PATTERN` family.**  `ASSERT_WAY_PATTERN` takes an
array-of-arrays of 6-bit ribi integers (`-1` = don't care, `0` = no
way).  Each disabled test still needs its patterns rewritten and its
actual hex-pathfinder route reasoned out — the original patterns
assume 4-bit square-axis paths, and at least for `wt_road` the
builder routes around the NE-SW axis (no sprite support there yet)
into a 2-step path through `(q+1, r)`-style intermediate tiles.
Affected: `test_way_bridge_build_{ground, above_way,
at_slope, at_slope_stacked, above_runway}`,
`test_way_road_build_straight / _parallel / _below_powerline /
_crossing / _upgrade_crossing / _upgrade_downgrade /
_upgrade_downgrade_across_bridge / _cityroad_{build,
upgrade_with_cityroad, downgrade_with_cityroad,
replace_by_normal_road, replace_keep_existing}`,
`test_way_tram_build_{parallel, on_road, across_road_bridge,
in_tunel}`,
`test_way_tunnel_build_{straight, up_down, above_tunnel_slope,
across_tunnel_slope}`, `test_way_tunnel_make_public`, and the two
`test_scenario_rules_allow_forbid_tool_stacked_{rect,cube}` entries.
Several crossing cases additionally need a hex-axis pair to replace
the square-perpendicular setup.

**Legacy `slope.east` / `slope.west` in tests.**  The square-era 2-corner
diagonals are still admitted as way slopes by the post-slope-way
predicate (side-chord branch with samples both at 1), but the names
project poorly onto the hex axes and read as historical baggage.  Four
call sites (`test_building_rotate_harbour`, the "east-west direction"
block in `test_depot_build_on_bridge_end` and the matching block in
`test_halt_build_on_bridge_end`, `test_powerline_remove_powerbridge`)
migrated to `slope.se_edge` / `slope.nw_edge`, the hex axis they were
already projecting onto.  `test_scenario_rules_allow_forbid_way_tool_cube`
still uses `2*slope.east`; left untouched because the function is
already HEX-PORT PENDING and needs a holistic rewrite (square ribi
values in `ASSERT_WAY_PATTERN` throughout, plus 2× way slopes are
gone).  `test_terraform_climate_from_water` observes terrain restoring
to `2*slope.east` after a water-height round trip; that is terrain /
water-table fallout, not a direction-conversion contract, and should
move to a planar hex double slope when that path is ported.

**Powerline 3rd hex axis.**  `test_powerline_connect /
_build_transformer_multiple / _ways` each expect crossings /
powerlines on the 2 square-era axes (N-S and old E-W).  Under hex
there are 3 axes and the 3rd (NE-SW) has no powerline crossing sprite
or connection FSM support (`leitung2.cc` diagonal-image table is
keyed on 4 old-combo values).  Restore after the crossing-cluster /
3rd-axis work lands.  `_transformer_multiple` additionally depends on
`leitung_t::suche_fab_neighbour`'s adjacency order — see
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
`test_terraform_raise_lower_land_at_water_center`,
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
`tool_change_water_height_t` in `simtool.cc` is hex-aware (6-neighbour
flood, shared-edge corner heights on the *current* tile per
`vertex_owners`, six-corner apply + `set_grid_hgt_nocheck`).  Scenario
`test_terraform_raise_lower_water_level` stays commented out: it still
uses a rectangular `terraform_volcano` scaffold and square-shaped
flood expectations — restore after a hex-shaped scaffold and
assertion rewrite.

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

## Lower_to water-tile NW-only gate

`terraformer_t::lower_to` short-circuits water tiles unless the NW corner
is being lowered (`terraformer.cc:472-476`); `raise_to` has no symmetric
gate.  The NW pick is the legacy "tile reference height" corner from the
square era — under hex no single corner has that role.  In practice the
gate decides "did the corner the water table is keyed off of drop", and
the answer probably wants to be "did `min_corner` drop" or "did any
corner that touches a neighbour-with-higher-water drop".  Real semantic
choice, not a mechanical refactor; lands together with the wider
hex-aware water-table propagation pass when that gets scheduled.

## `tool_set_slope_work` doesn't gate on `is_way`

The way-slope tightening (12 axis edges, no 2×, no east/west) is
enforced by `slope_t::is_way` at way-build time and by
`way_desc::has_double_slopes()` returning false on the all_up/down
progression in `tool_set_slope_work`.  Direct `setslope(tile,
2*slope.south)` under an existing way still succeeds — the
terraformer's only way-direction check at `simtool.cc:1316` is the
`backward(ribi_type) == ribis` opposite-axis check, which 2×
satisfies.  Result: a way ends up sitting on a non-way-buildable
slope, reachable from script as `command_x.set_slope(pl, pos,
2*slope.south)`.  Either add an `is_way(new_slope)` gate when the
tile carries a way, or accept that ground slopes are unrestricted
and document the asymmetry.  Test
`test_terraform_raise_lower_land_below_way` exercises this path
already and currently passes — it sets 2*slope.south directly,
then walks back via two all_down steps.

## `slope_allows_ribi` not applied to bridges and tunnels

The post-pathfinding slope-vs-ribi check landed in `way_builder_t`
only — `validate_route_slopes` walks the ground route and rejects
configurations the per-step `check_slope` lets through (half-chord
stubs that mix with a ramp, axis-mixed bends, chord heights that
don't reconcile across axes).  `bridge_builder_t` (`brueckenbauer.cc`)
and `tunnel_builder_t` (`tunnelbauer.cc`) have their own slope
admissibility checks that pre-date the rule and don't call it,
so a bridge end or tunnel mound can land in a configuration the
rule would reject on a ground way.  Wire `slope_allows_ribi` into
those builders next time either is touched — pulling out a small
"check this single tile's final ribi against its slope" helper from
`validate_route_slopes` would let all three sites share one entry
point.

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

The disabled `test_way_bridge_build_at_slope` (HEX-PORT PENDING)
asserts `err == ""` (build refused) for its "planar double height
slope" subcase — that was true under the art-presence policy but is
not under this one; restoring the test needs to flip that subcase
to expect `null` and add a `find_object(mo_bridge)` check, the way
`test_way_bridge_build_at_planar_double_slope` does.

## Flat-way chord altitude is hardcoded to the bottom of the interval

`slope_t::chord_h_axis` returns the lowest height in the overlap of
the two edges' height intervals.  A saddle / single-corner-on-axis-edge
slope often admits a chord at z=1 as well as z=0, but `get_vmove`
always reports the z=0 chord, so a path threading the slope tile
between two raised-flat (z=1) neighbours fails to chain and the build
is refused.  Surfaces if somebody tries to route through a saddle
between higher terrain.  Generalise to "pick the chord matching the
requested altitude" (per-neighbour, plumbed through `get_vmove` /
`is_allowed_step`), or expose the full interval and let the pathfinder
pick — when somebody actually hits the refusal.

## Way-object slope-up sprites — still 4 of 6 hex edges

`way_obj_writer.cc` iterates `slope = 3, 6, 9, 12` for
`frontimageup` / `backimageup` under the square-era pattern: 4 of
6 hex edges, missing the two third-axis edges (raised E+SE and
raised NE+E pairs, see `koord.cc` neighbour case table).  Needs
the same widening that `way_writer.cc` got — 12 slots covering
all 6 hex edges × {narrow, wide}, indexed by
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
NW).  Extended switch sprites (3-way junction art) are gone with
the same change: `way_desc::has_switch_image` is hardcoded to
`false`, `image_switch` in `weg_t::set_images` now routes through
`get_image_id`, and the upstream 5-entry `ribi_to_extra` table is
deleted.  `schiene_t::reserve` still toggles a `HAS_SWITCHED` flag
that no longer affects rendering — vestigial, retire alongside
the hex-aware vehicle-direction work in "ribi_t — audit
surfaces".

River ways have a narrow engine-side visibility fallback:
`way_desc_t::get_image_id` maps missing `type_river` multi-ribi
slots to an available single-edge river sprite, and `weg_t::calc_image`
falls back from missing river slope sprites to the flat-ribi path.
This is not a general way-art shim; it exists because natural map
generation can create rivers before pak128 has complete 64-slot river
art.  Delete the fallback when `landscape.rivers` ships real hex flat
and slope river sprites.

The on-disk `way_desc` save version did not bump.  Pre-built paks
(pak64 from `tools/get_pak.sh` in CI; any cached pak128 build) load
without fatal but their 16-entry imagelist is now indexed by 6-bit
hex ribi: `ribi >= 16` returns IMG_EMPTY, and `ribi 0..15` returns
the upstream sprite at that slot drawn for a different hex direction
than the index now means.  Rendering is wrong but graceful;
gameplay-only scenario tests should still pass because they don't
assert on visuals.  Bump the version field once the migration is
worth a hard cutover, or rebuild the CI pak from source against the
new makeobj when one becomes available.

The same migration story applies to slope-up keys.  `way_writer.cc`
now expects 12 keys (`ImageUp[n]`, `ImageUp[ne]`, ..., `ImageUp[nw]`,
plus `_wide` variants) routed by `way_desc_t::get_slope_image_id`.
The legacy `ImageUp[3/6/9/12]` and `imageup2[*]` keys are gone.
Only `rail_060_tracks` is migrated; every other way .dat that
declared slope sprites under the old keys now silently drops them
on rebake (slope rendering falls through to IMG_EMPTY).  Migrate
per family alongside the flat-image migration above.

## Per-vertex height storage — remaining writer-side ports

Storage is per-hex-vertex (see `documentation/hex-vertex-storage.md`,
`surface_t::grid_hgts`): two canonical slots (E, SE) per tile, plus
boundary padding.  `perlin_hoehe` now samples at world-vertex
positions and writes both canonical slots, so freshly generated
terrain is self-consistent across shared vertices — the three owners
of any shared vertex all resolve to the same slot and get the same
noise value by construction.

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
bound-checking with `ribi < 16` (`way_desc.h` switch sprites,
`way_obj_desc.h` crossings) is residual square-grid assumption to
clean up, not a sim/art compromise to ratify.

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
(2/4/8 entries); the underlying 2-axis FSM is a gameplay design
choice covered separately in "Sign / traffic-light 2-axis FSM"
above.  Key vocabulary updates fall out of that work, not this
cluster.

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
orthogonality (`wasser.cc`).  Also stubbed to `rotate60` (the
"one step over" axis).  Triggered by the crossing-cluster port:
per-site review, some may want both 60° and 120° (test both
adjacent axes), some may redesign the check entirely for hex
3-axis geometry.

**Diagonal way-image selection.**  Square-era code detected a
smooth 45° diagonal way (a bend whose CW-endpoint neighbour
carried the reverse bend) and switched to a `_diagonal` sprite
variant.  Flat-top hex has 3 axes (N-S, NE-SW, NW-SE) — every
direction lies on an axis, so there is no out-of-axis diagonal
to detect.  Engine-side bookkeeping, the placement-preview
diagonal branches in `simtool.cc`, and the unused
`way_obj_desc_t::*_diagonal_image_id` accessors are deleted.
`way_writer` now emits the diagonal imagelist node always-empty
(the `+2` offset in `image_list_base_index` keeps it for layout
compatibility) and `way_desc_t::has_diagonal_image` is hardcoded
to `false`; `get_diagonal_image_id` consequently returns
IMG_EMPTY for every input.  `leitung2.cc`'s old square-era combo
casts into this lookup are now dead code waiting to be deleted
in the "Powerline 3rd hex axis" cluster.

**Old-east→hex-SE, old-west→hex-NW rename convention.**  ~30+ sites
in rendering, signs, and leaf files mechanically renamed
`ribi_t::east`→`ribi_t::southeast` and `ribi_t::west`→`ribi_t::northwest`.
The rename is legitimate *under the current 2:1 isometric viewport*
(both names refer to the same axial displacement vector).  When the
viewport port lands and the projection changes — or if NE/SW ever
need sprite representations — every rename site needs re-audit.
Grep: `HEX-PORT.*east\|HEX-PORT.*west\|\b(southeast|northwest)\b`
inside rendering-cluster files.

**`is_straight_ns` last caller.**  `leitung2.cc:298` picks one of two
powerline diagonal sprites with the 2-axis predicate; NE-SW lands on
the wrong branch.  Bound to the "Powerline 3rd hex axis" sprite
cluster above; retires together with that cluster (along with
`is_straight_ns` itself).

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

Slope-edge naming asymmetry: the 6 hex-edge slope constants split
into 2 bare names (`slope_t::north`, `::south`) and 4 suffixed
(`::ne_edge`, `::se_edge`, `::sw_edge`, `::nw_edge`).  The suffix
disambiguates from the `southeast = raised_SE` etc. single-corner
aliases above and from squirrel's `slope.northeast = 1024` (single
corner on the script side).  Cosmetic; collapse the asymmetry by
either retiring the C++ single-corner aliases (no in-tree use
beyond the enum declaration itself) or by suffixing all 6 edges
(`n_edge`/`s_edge` etc.).  Either move ripples through ~50 call
sites of `::north` / `::south`.

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

`way_builder_t::check_terraforming` (`wegbauer.cc:1004-1010` and the
symmetric write block at 1064-1078) handles the 4 cardinal step
directions (N, SE, S, NW) explicitly with per-corner height arithmetic.
NE and SW (the third hex axis) fall through the if-chain un-handled —
the in-code `HEX-PORT` comment names this — and `*new_from_slope` /
`*new_to_slope` are left at their default values, so a way-builder
terraform step on the NE-SW axis is a silent no-op.  The fix needs
the corner indices for the NE / SW shared edge under hex (which two
corners of tile A correspond to which corners of tile B across the
NE/SW edge); pure mechanical extension once those indices are pinned.
Tied to the wider hex-aware terraform / vertex-sharing cluster.

`schiene_t::reserve` (`obj/way/schiene.cc:91`) computes
`set_switched(dir == ribi_t::northeast || dir == ribi_t::southwest)`.
The square upstream this was ported from used the old
2-bit `northeast` / `southwest` *bend* constants (N|E and S|W); the
hex rename made those single-bit values, but `reserve` is gated on
`is_bend(dir)` which only fires for 2-bit ribis, so the equality
test can never match and `set_switched` is always false — every
3-way rail switch shows the unswitched sprite regardless of which
leg the convoy takes.  Visual-only, but tied to the
"Vehicle direction enum — compound 2-step displacements" cluster
above: `dir` arrives via `ribi_type(prev_pos, next_pos)` whose
2-step compound semantics are the same surface the rest of that
cluster waits on.  Lands together with the hex-aware vehicle
direction model.

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
so source RLE and alpha walks stay in lockstep, and
`init_ground_textures` tripwires the silhouette match before
privatising each alpha cell to a 1bpp bitmask.
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

**Back-wall and bridge / tunnel hide-test residuals.**  The hide-test
loop in `calc_back_image` is still the square 3-corner sweep with
`testdir` including the hex-invalid `(-1,-1)`; it samples W, NW, NE
and ignores E + wall 2.  The `> 11` magic check in
`brueckenboden_t::calc_image_internal` and `tunnelboden_t` was "is
wall 1 non-trivial" under the old base-11 2-digit encoding; under
the 3-digit encoding (`w0 + 11*w1 + 121*w2`) the threshold no longer
maps cleanly, so bridge / tunnel "draw as obj" logic can misfire on
hex-only edge slopes.  Both retire together with the hex-aware
brueckenboden / tunnelboden rewrite.  `grund_t::get_back_image(leftback)`
exposes only walls 0 and 1 (the two callers in brueckenboden /
tunnelboden); replace with a `back_imageid`-direct accessor or widen
the API when those bridge / tunnel sites get ported.

Cliff back-wall middle-slope indices 9 / 10 are baked as single-step
half-cliffs (one corner at 0, one at 1) — placeholder for the legacy
double-height notch shape that the pakset renderer at
`hextrans-pak128/landscape/grounds/back_wall/render.py` doesn't yet
model; revisit if stacked terraforming reads wrong.

Fence sprites (`back_imageid > BIID_ENCODE_FENCE_OFFSET`, drawn from
`ground_desc_t::fences`) still use `tile_raster_scale_y` for the
`corner_nw` offset and have only 3 pakset combos for walls 0+1; the 4
new wall-2-involving combos return IMG_EMPTY at draw time.  Move the
offset to hex z and ship hex-aware combos when fences come back into
scope.

`grund.cc::display_border` is square-grid logic (`pos.y/x ==
welt->size().y/x - 1` edge detection, `lookup_hgt[2+diff]`
corner-delta arithmetic that can run OOB under base-4 slope
encoding, reads into the legacy 1D `Slopes` 22-cell packing).  The
new pak128 cliff bake repurposed `Slopes` / `Basement` to a 2D
`Image[<wall>][<index>]` layout, so the legacy reads silently miss
on hex paksets; legacy paksets like pak64 still ship the 1D layout
but the OOB risk applies under base-4 slope encoding.  Body is a
no-op early-return + one-time `dbg->warning` -- not a fatal, since
the function is purely visual (no save-state effect, no
propagation) and a fatal regresses pak64 + hex-engine boot for what
is otherwise a benign missing visual.  Reactivation needs either a
hex-aware `MapEdgeCliff` family or a rewrite of this function onto
the new (wall, index) layout.

**Phase C — flow-on.**  Minimap (`gui/minimap.cc`, square pixels
per tile), per-step vehicle interpolation offsets
(`vehicle_base_t::calc_set_direction` and friends, square-iso
baked), label / halt screen-anchor positions.  The last group
rides along on `get_screen_coord` so positions become hex-correct
automatically; per-tile drawing under each anchor still assumes
square geometry until phase B lands.

**Half-tile centring nudge missing.**  The square renderer applied a
`disp_w/IMG_SIZE & 1` half-row nudge so the world centre lined up
with the screen centre at all viewport widths; the hex equivalent
`disp_w/(3·IMG_SIZE/4) & 1` is not applied anywhere in
`display/simview.cc`'s draw loop.  At specific window widths the
world centre therefore sits half a tile off the screen centre.
Apply the parity nudge to the `i_off` / `j_off` setup around
`simview.cc:138`; mechanical fix.

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

## other

Save-format feature gates now have names in `simversion.h` rather
than floating against `SIM_SAVE_MINOR`.  Continue that convention for
future on-disk changes so later fork-version bumps do not silently
change the meaning of old readers.

`simgraph16_convert_to_bitmask`'s RLE-to-1bpp walk has no unit test.
The conversion is a non-trivial pure transform (RLE walk plus a
binary-alpha tripwire that mirrors `draw_alpha`'s `alpha_value > 30`
threshold under arbitrary alpha-flag combinations), but the body
sits in `simgraph16.cc` next to the renderer globals and the
`TRANSPARENT_RUN` macro so a standalone test would need the pure
walk lifted into a header first.  Lift + test together when next
touching this code; same standalone-binary shape as
`tools/hex_proj_test/`.

`SIMUTRANS_DISABLE_BITMASK_ALPHA` and `SIMUTRANS_PROFILE_REZOOM`
are profiling knobs introduced with the shore/slope alpha bitmask
path.  Useful for the A/B perf characterisation that motivated the
bitmask in the first place; both retire (env-var read + the rezoom
profiler block in `simgraph16.cc`) once we have data confirming the
bitmask path is the right default and don't expect to flip back.
