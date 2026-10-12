# Playable map shape: the W×H×D hexagon

Design for issue #280: the playable world is a centrally symmetric hexagon
whose opposite side pairs are W, H and D tiles long (W×W×W is regular).
Steps not yet landed are under "Plan"; the rest describes the target.

## Shape

Sides are counted in tiles, corner tiles included.
The W sides run along the SE–NW axis (`+q`),
the H sides along the N–S axis (`+r`),
the D sides along the NE–SW axis (`(-1, +1)`).
The hexagon is the W'×H' axial rhombus, with `W' = W+D-1` and `H' = H+D-1`,
with its two 60° corners cut off along the NE–SW axis:

    0 <= q < W'    0 <= r < H'    t <= q + r <= W' + H' - 2 - t,    t = D - 1

    tiles = W'·H' - t·(t+1) = WH + HD + DW - W - H - D + 1

For W = H = D = n this is `3n² - 3n + 1`, the hex disc of radius n-1.
**D = 1 gives t = 0, the plain W×H rhombus**:
today's maps are W×H×1 hexagons and need no conversion.
The cut corners are `(0, 0)` and `(W'-1, H'-1)`;
the 120° corners `(W'-1, 0)` and `(0, H'-1)` stay.
The near cut faces NW and N, where no walls are drawn;
the far cut faces SE and S, like today's far rhombus edges.

`map_shape_t` (`dataobj/map_shape.h`) holds `(W, H, D)`
and answers `contains(q, r)`, `tile_count()`, the extents, and rotation.
It is header-only over `koord.h`,
so `tools/hex_map_shape_test` compiles it as it is.

## Storage: bounding rhombus plus a validity predicate

`plan`, the vertex height arrays, `water_hgts`, `climate_map`, `humidity_map`,
`marker_t` and the minimap buffers keep their rectangular `W'×H'` layout.
`surface_t::cached_grid_size` is `(W', H')`, so `get_size()` returns the extents
and `plan[q + r*W']` stays.
`is_within_limits` adds the two cut half-planes to its branchless test:

    (x | y | (cached_size.x-x) | (cached_size.y-y)
       | (x+y-cut_lo) | (cut_hi-x-y)) >= 0

`access`, `lookup` and `lookup_kartenboden` go through it,
so callers that tolerate a NULL tile at the map edge need no change:
the renderer draws the outside tile, routes do not expand, tools refuse.

The vertex arrays need no change.
Every vertex of an on-map tile has its canonical owner
(`documentation/hex-vertex-storage.md`) inside `[-1, W'-1] × [-1, H']`,
the range the slots already cover.
A vertex is on-map when any of its three `vertex_owners` is an on-map tile;
`map_shape_t::contains_vertex` is that predicate,
for the grid-point gates of the terraforming tools.

**Waste.** The cut corners hold `t·(t+1)` cells:
about 25% for any regular hexagon, 6.6% for 512×256×128, 1.2% for 512×512×64,
64% for the extreme 128×128×512.
An off-map cell costs about 30 bytes and holds no `grund_t`;
a regular 256-hexagon wastes about 2 MB.

**Rejected: compact row-offset storage**
(`plan[row_start[r] + q - q_min(r)]`, no waste).
Besides `access_nocheck` and the ~15 raw `plan[]` sites in `simworld.cc`,
it changes every other array that shares the `x + y*W` stride:
the vertex slot layout and its standalone test,
`water_hgts`, the two `array2d_tpl` climate maps,
`marker_t`, `fabrikbauer`'s `fab_map`, and the minimap.
Memory is the only gain, small next to the `grund_t` of each on-map tile,
and upstream's code paths would not survive it.
PR #33's inscribed radius `min(W, H)/2` is rejected too:
it cannot express W×H×D and leaves populated terrain in the corners.

## Off-map cells

Off-map cells are `planquadrat_t` with `ground_size == 0`.
`access_nocheck(k)->get_kartenboden()` is NULL there,
so a loop over the full `W'×H'` rectangle that dereferences the ground
crashes at the first off-map cell:
the tripwire for loops that have not learnt the shape.
Each such loop gets `if (!is_within_limits(k)) continue;`,
or the per-row range `shape.q_range(r)` in hot loops.

Step 2 fixed the loops that a flat empty hexagon runs through
on generation, the first game months, save, load and display:
`cleanup_grounds_loop` (off-map cells get no ground),
the height-bound loop in `enlarge_map`,
`create_lakes` and `can_flood_to_depth`
(an off-map cell is never a lake seed and counts as the map edge),
the three `create_beaches` passes, `calc_humidity_map_region`,
`distribute_groundobjs`, `distribute_movingobjs`,
the rainfall pass of `distribute_trees_region`,
`tree_builder_t::fill_trees`,
`square_is_free` and `disc_is_free`
(so `find_free_discs` and city placement skip the cut),
`plans_finish_rd`,
and `planquadrat_t::correct_water` and `update_underground`,
which return early on a cell without ground.
`gameinfo_t` draws off-map cells of its minimap black.
`perlin_hoehe_loop` and the climate generators
write heights and climates for off-map cells too;
nothing reads them, so they stay.

Left for step 3: `create_rivers`
and the city, factory and attraction builders on a populated map,
the season, image and sync loops in `simworld.cc`
that dereference the ground,
`minimap.cc`, `obj_t` and `vehicle_base_t` rotation walks, `simplay.cc:532`,
`simtool.cc:2328`, `fabrikbauer.cc:77`, the AI scan windows,
`lookup_kartenboden_gridcoords`,
and `climate_at_clamped` / `vertex_corner_height`,
whose rectangle clamp lands on an off-map cell next to a cut
and reads it as water at groundwater height.
The `enlarge_map` strips (step 5) and `rotate90_plans` (step 6)
are tripwires instead:
`enlarge_map` on an existing world and `rotate90` call `dbg->fatal`
when D > 1.

`koord_random(W', H')` callers sample by rejection
(acceptance at least 36%, even at 128×128×512);
`get_closest_coordinate` also clamps onto the cut lines;
`display_border`, `zeiger_t` and `viewport_t` test
`!is_within_limits(pos + neighbour)` instead of `pos.x == size.x-1`;
edge margins (airports, AI) use `map_shape_t::edge_distance(k)`,
the minimum over the six half-planes.

## Save and load

`settings_t` keeps `size_x`/`size_y` as W and H and adds `size_d`, default 1;
`get_map_shape()` returns the `map_shape_t`.
`settings_t::rdwr` writes `size_d` behind a named gate in `simversion.h`,
`SAVE_VERSION_HEX_MAP_SHAPE_MAJOR/MINOR` (124, 902),
with `SIM_SAVE_MINOR` and `SIM_SERVER_MINOR` at 902;
older saves read D = 1.
`rdwr_gamestate` sets `cached_grid_size` from the shape before the tiles load,
through `surface_t::set_cached_shape`,
and refuses a shape that is invalid or wider than 32766 cells.

`gameinfo_t` carries W, H (the sides, from the settings,
where it used to copy `get_size()`: equal at D = 1)
and D under the same gate.
The file holding it travels with its own version header
(`SERVER_SAVEGAME_VER_NR` for `NWC_GAMEINFO`),
so a reader of either version parses it;
`nwc_gameinfo_t` itself is unchanged.
The server list prints `W×H×D` when D > 1.
The listserver announcement keeps `&size=` at the bounding extents
and adds nothing:
its query fields are the external listserver's schema.

The tile and climate streams skip off-map cells on save and on load,
with no version check:
a pre-gate save has D = 1, so every cell is on-map, as before.
`planquadrat_t::rdwr` keeps its "no kartenboden" fatal.
Vertex heights travel with `grund_t::rdwr`; off-map ones are not saved.
This does not wait for the #285 version bump;
the bump's terrain round-trip test should also run on a hexagon with D > 1.

## World creation

The new-world dialog (`gui/welt.cc`) has three fields, W, H and D,
and an "Equal sides" checkbox:
while it is ticked, editing any field sets all three.
City, factory, attraction and river densities scale with `sqrt(tile_count())`
instead of `sqrt(size_x·size_y)`.

**Image coordinates.**
The map preview and an imported heightfield share one frame,
the physical plane in offset coordinates:
column `i = q`, row `j = r + (q - t)/2`.
Columns are exact, because a tile centre sits at `x = 1.5·q`;
odd columns fall half a row down, as on screen.
The hexagon needs an image of `W'` columns by `H + ⌈(W'-1)/2⌉` rows,
a square image for a regular hexagon.
The pixel aspect is `1.5 : √3`, about 0.87, so a source with square pixels
reads about 13% narrower than tall.
Pixels outside the hexagon are ignored.

The preview draws in that frame and blanks the cut corners.
Loading a relief fixes the image size:
D stays editable, W follows from the columns and H from the rows,
and the dialog refuses a D that leaves either below 1.
`height_field_vertex_height` (branch `claude/heightfield-import-hex`)
samples at `hex_vertex_pos(v)` and maps back to axial,
pixel `(i, j)` = tile `(i, j)`, which shears a picture drawn on the plane.
Only its back-conversion changes: `i = p.x / 1.5`, `j = p.y / √3 - t/2`.
At D = 1 preview and import too move from axial identity to this frame.

**Generation.** `perlin_hoehe` already samples at `hex_vertex_pos`,
so only its loop skips off-map cells.
`init_perlin_map` sizes its cache from the hexagon's physical bounding box.
Cities, factories, rivers and trees pick candidates through `koord_random`
and edge margins, so they follow the predicate.
New worlds default to 150×150×150 (67 051 tiles, about the old 256×256).

**Enlarging.** Growing W and H with D fixed keeps the old hexagon
inside the new one at the same coordinates,
since `cut_lo` stays and `cut_hi` only grows.
The new tiles are the new hexagon minus the old one,
which replaces the L-shaped strips of `enlarge_map`'s `old_x`/`old_y` loops.
Growing D would move the old map by `(a, b)` with `a + b = ΔD`,
a translation of every saved koord.
`enlarge_map_frame` shows D read-only until 60° rotation lands;
then rotate D into W's place, enlarge, rotate back.
Shrinking stays unsupported, as upstream.

## Rotation

One 60° step clockwise on screen takes SE to S and S to SW.
It permutes the sides and moves each tile into the new rhombus:

    (W, H, D) -> (D, W, H)
    (q, r)    -> (H + D - 2 - r,  q + r - (D - 1))
    (tile, corner c) -> (rotated tile, c + 1 mod 6)

The new extents are `(H+D-1, W+H-1)`.
`karte_t::rotate90`, renamed `rotate_one_step` by the #281 plan,
reallocates `plan` and the vertex arrays at the new extents
and moves each on-map cell to its rotated index,
as upstream's `rotate90_plans` swaps W and H.
Three steps map the shape onto itself with `(q, r) -> (W'-1-q, H'-1-r)`,
so 180° needs no reallocation and works on today's rhombus.

The linear part of a step, `(q, r) -> (-r, q + r)`,
is `hex_axial_rotate` in `dataobj/koord_hex.cc`;
`rotate_tile` and `rotate_vertex` add the shift `(H + D - 2, -(D - 1))`.
`koord::rotate90(y_size)`, `koord3d::rotate90` and `array2d_tpl::rotate90()`
are the D = 1 forms, with `y_size = H - 1`:
upstream's signatures, one step of a W×H rhombus.
The rotated hexagon (1, W, H) is not a rhombus,
so `array2d_tpl::rotate90()` grows the array to its bounding rhombus.
`tools/hex_map_shape_test` pins all of them against `rotate_tile`.

## Upstream divergence

`size_x`/`size_y` (equal to the extents at D = 1), `get_size()`,
`is_within_limits` and `plan[x + y*W]` keep upstream's meaning;
new are one field, one predicate term, one header,
and small `contains` hunks inside upstream loops.

## Plan

1. **`map_shape_t` and its self-test.**
   `dataobj/map_shape.h` with `contains`, `contains_vertex`, `tile_count`,
   the extents, `rotated` and `rotate_tile`/`rotate_vertex`.
   Landed.
   Test: `tools/hex_map_shape_test` in CI:
   tile count against enumeration, D = 1 is the full rhombus,
   central symmetry, rotation a bijection with period six
   that keeps neighbours and the three names of a vertex together.
2. **Shape in settings, save and `surface_t`.**
   `size_d` and its save gate, `gameinfo_t`,
   the cached cut in `is_within_limits`,
   off-map skips in the tile and climate streams and in world creation
   (off-map cells stay empty), and `-flatmap W,H,D` in `simmain.cc`.
   Landed.
   Scripts read the sides through `settings.get_map_side_w/h/d`.
   Test: the suite stays green (its fixture is W×H×1);
   the `test_map_shape` tests run in it and,
   through `tools/test.py --map` and a CI step,
   on `tests/empty-hex-9x7x5.sve`, written by `-flatmap 9,7,5`:
   the extents, ground on exactly the on-map cells and none off them
   (`square_x` and `tile_x` invalid), the tile count,
   and a road from each cut line into the cut refused.
   The fixture went through the save and load paths,
   so the shape assertions also cover the round trip.
   `tools/hex_map_shape_test` checks the cached branchless predicate
   `map_shape_t::contains_cached` against `contains`.
3. **Full-map loops and edges.**
   The tripwire sweep listed under "Off-map cells", `edge_distance`,
   border walls, cursor and viewport.
   Test: a `-mapshape W,H,D` hook generates a full world
   (relief, cities, factories, an AI player) and runs a game year;
   a CI step runs it headless under ASAN at 48×40×32.
4. **World-creation dialog and image frame.**
   Three fields, "Equal sides", densities by tile count,
   preview and heightfield import in offset coordinates, the new default.
   Test: the heightfield sampler self-test pins the offset frame;
   a scenario imports a relief into a 9×7×5 world, checks corner heights.
5. **Enlarge.** W and H grow with D fixed;
   the new-region loops use the predicate.
   Test: enlarge the fixture to a larger W and H;
   old tiles are unchanged, every new on-map tile has ground,
   and the tile count matches the formula.
6. **Rotation.** #281's W×H×D and 60° steps, built on `rotate_tile`.
   Test: the rotation round trip on a D > 1 fixture.
