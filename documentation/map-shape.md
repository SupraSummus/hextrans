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
A loop over the whole map, or over a clipped rectangle of it,
goes through `surface_t::for_each_tile(f)`
or `for_each_tile(x_min, x_max, y_min, y_max, f)`,
which call `f(koord)` for the on-map tiles only, row by row.
A loop that breaks out early walks the per-row range instead,
`get_row_begin(y) <= x < get_row_end(y)`
(`map_shape_t::get_q_begin/get_q_end`).
A sparse probe or a neighbour step uses `lookup_kartenboden`
and skips the NULL it returns off the map.

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
The climate generators write climates for off-map cells too;
nothing reads them, so they stay.
`perlin_hoehe_loop` skips the middle of the cut corners (step 4).

Step 3 fixed the loops a populated world runs through
in generation, a game year of AI play and teardown:
`create_rivers` (its source grid skips the cut),
the destructor fallbacks of `obj_t` and `vehicle_base_t`
that search the whole map for a lost object,
`player_t`'s bankruptcy sweep (`simplay.cc`),
the haltlist cleanup in `~haltestelle_t`,
the AI's site scans (`ai_t::suche_platz`, `find_harbour`,
and `ai_passenger_t`'s harbour and airport windows),
the neighbour image refresh in `terraformer_t` and the remove tool,
the city-info and listserver minimaps,
and the raise and lower tools,
which refuse a grid point whose tile is in a cut
(`lookup_kartenboden_gridcoords` returns NULL there).
`climate_at_clamped` and `vertex_corner_height` clamp
through `get_closest_coordinate`,
which now lands on the cut lines too,
so a vertex next to a cut reads a real tile, not water.
The season and image loops (`check_season_snowline`, `update_map_intern`)
and `fabrikbauer`'s `fab_map` stay rectangular:
they go through `get_boden_count()`, which is 0 off the map,
or touch no ground.
The `enlarge_map` strips (step 5) are a tripwire instead:
`enlarge_map` on an existing world calls `dbg->fatal` when D > 1.
`karte_t::rotate90` walks the on-map cells only (see "Rotation").

`koord_random(W', H')` callers sample by rejection
through `surface_t::get_random_tile()`
(acceptance at least 36%, even at 128×128×512);
`get_closest_coordinate` also clamps onto the cut lines,
which keeps the viewport over the hexagon.
`surface_t::is_front_edge(k)` is an on-map tile
whose SE, S or SW neighbour is off the map;
`grund_t::display_border` draws a wall towards each such neighbour,
along the far rhombus sides, the far cut and the `q = 0` side,
and `zeiger_t` and `viewport_t` test it
instead of `pos.x == size.x-1`.
Edge margins use `map_shape_t::edge_distance(k)`
(`surface_t::get_edge_distance`),
the minimum over the six half-planes,
which is the hex distance to the nearest off-map cell minus one;
a footprint's is the minimum at its two extreme corners.
The AI's harbour and airport sites keep 6 tiles from every side,
a city does not grow on an edge tile,
and the lake tool does not flood one.
The minimap draws the hexagon on black.

Still rectangular after step 3:
the light tint over the isometric minimap
while a schedule is shown,
and the grid-point gates of the terraforming tools,
which go through the tile, not `map_shape_t::contains_vertex`:
a vertex on a cut line is raised from its on-map tile only.

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
while it is ticked, editing any field sets all three,
and ticking it sets all three to W.
W and H are at least 8 as before, D at least 1,
and the extents `W'` and `H'` stay within 32766
(an equal side is at most 16383).
The checkbox starts ticked when the settings have W = H = D.
City, factory, attraction and river densities scale with
`map_shape_t::get_side_equivalent()`, `sqrt(tile_count())`,
instead of `sqrt(size_x·size_y)`: the same number at D = 1.
The memory estimate counts a `planquadrat_t` per cell of the bounding rhombus
and the ground of each tile.

**Image coordinates.**
The map preview and an imported heightfield share one frame,
the physical plane in offset coordinates:
column `i = q`, row `j = r + ⌊(q - t)/2⌋`.
Columns are exact, because a tile centre sits at `x = 1.5·q`;
the columns with `q - t` odd fall half a row down, as on screen.
The hexagon needs an image of `W'` columns by `H + ⌈(W'-1)/2⌉` rows,
a square image for a regular hexagon
(`map_shape_t::get_image_cols/rows`, `get_image_row`, `get_tile_row`).
The pixel aspect is `1.5 : √3`, about 0.87, so a source with square pixels
reads about 13% narrower than tall.
A pixel whose tile is off the hexagon belongs to no tile.
The vertices of the tiles along the edge still interpolate
between the pixels around them, outside ones included,
so an image should not carry content there that it does not want in the edge.

The preview draws in that frame:
preview pixel `(x, y)` shows image pixel `(X, Y)` at the centre of its cell,
the tile `(X, get_tile_row(X, Y))`, in the colour of its height,
and pixels off the hexagon are black.
At D = 1 that is the sheared rhombus of the screen, not the axial square.
The preview has the aspect of the image.
Loading a relief fixes the image size:
D stays editable, W follows from the columns and H from the rows
(`map_shape_t::from_image_size`),
and the dialog refuses a D that leaves W below 1 and keeps the old one.
H does not depend on D, so a relief with `rows <= columns/2` fits no D:
it is refused with a message and the generated map stays.
A relief that does not fit the D of the settings when it is loaded
sets D to `(columns + 1)/2`, the depth of the regular hexagon.
W, H and the equal-sides box are fixed while a relief is loaded.
`karte_t::load_heightfield` derives the shape from the image the same way,
and the `-heightfield FILE [D]` hook takes D (default 1).
`height_field_vertex_height` samples at `hex_vertex_pos(v)`
and takes the position to the image frame,
`i = p.x / 1.5`, `j = p.y / √3 - t/2`.
Values between pixels are multiples of 1/6,
so a tie at .5 is exact:
the rounding adds `1e-9` so that ties round up on every platform.

**Generation.** `perlin_hoehe` already samples at `hex_vertex_pos`,
so only its loop skips cells:
`perlin_hoehe_loop`, `fill_south_phantom_heights`
and the relief import fill the canonical vertex slots
that `map_shape_t::slot_near_map` accepts,
those within one vertex step of an on-map vertex,
which is everything at D = 1 and leaves the middle of the cut corners at the
water level.
The smoothing in `cleanup_karte` only raises vertices,
so the low off-map ones never touch the map.
`init_perlin_map` sizes its cache from the physical bounding box of those slots,
`map_shape_t::get_vertex_pos_bound()`:
the old `(W'+2) × (H'+2)`-based size at D = 1,
smaller in y by `t/2` tiles otherwise.
Cities, factories, rivers and trees pick candidates through `koord_random`
and edge margins, so they follow the predicate.
New worlds default to 150×150×150 (67 051 tiles, about the old 256×256);
every other place that builds `settings_t` sets D = 1 with its size
(`simmain.cc` test hooks, `fuzz_command`),
and a saved `default.sve` keeps its own shape.

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
Three steps map the shape onto itself with `(q, r) -> (W'-1-q, H'-1-r)`.

The linear part of a step, `(q, r) -> (-r, q + r)`,
is `hex_axial_rotate` in `dataobj/koord_hex.cc`;
`rotate_tile` and `rotate_vertex` add the shift `(H + D - 2, -(D - 1))`,
and their `steps` forms chain single steps through the rotated shapes.
`koord::rotate90(shape)`, `koord3d::rotate90(shape)` and
`array2d_tpl::rotate90(shape)` take one step from `shape`;
the hooks of the cascade take that shape where upstream took `y_size`,
and `obj_t` hooks read it from `surface_t::get_map_shape()`,
which still holds the old shape while the tiles move.
`tools/hex_map_shape_test` pins them against `rotate_tile`.

`karte_t::rotate90(steps)` runs the cascade once per step.
Each step moves every on-map cell of `plan` and `water_hgts`
into arrays at the rotated extents,
maps each canonical height slot `(tile, E|SE)` to
`canonical_vertex(shape.rotate_vertex(v))`,
turns the climate maps, sets the rotated shape in the settings and
`surface_t`, and then rotates cities, factories, halts, convoys, players,
labels, the viewport, messages, windows, the scenario and the script
transform.
Ribis and slopes take exact 60° steps.
Building layouts and footprints, factory origins, pixel offsets
and axial rectangles (the city area, scenario rects) have no 60° form
before the 6-orientation footprint:
they turn by 180° on the step that completes a half turn
(`settings_t::is_half_turn_rotation`),
which is exact because positions take exact steps in between.
So `karte_t::can_rotate90` allows whole half turns from 0° or 180°,
on any W×H×D,
and the rotate tool turns by three steps, offline only, as upstream.

The script API sees the world at its own rotation 0
(`script_api::coordinate_transform_t`):
tiles through `rotate_tile` back over the map shape,
directions, slopes, corners and building layouts with them.
`debug.rotate_map()` turns the map from a script;
`tests/tests/test_world_rotate.nut` snapshots the world through the API
across half turns,
and `tools/test.py --rotate` runs the suite on a turned map.

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
   The sweep under "Off-map cells", `for_each_tile`, `edge_distance`,
   `get_random_tile`, border walls, cursor and viewport.
   Landed.
   Test: `-ai_soak W,H,D` generates a full world
   (relief, rivers, cities, factories, two AIs) on the hexagon
   and plays it; `tools/ai_soak.py --shape 48,40,32`
   runs seeds 1–3 for a game year headless under ASAN in CI.
   `tools/hex_map_shape_test` pins the row range and `edge_distance`
   against enumeration;
   `test_map_shape_no_terraform_in_cut` checks the raise and lower tools
   on the 9×7×5 fixture.
4. **World-creation dialog and image frame.**
   Three fields, "Equal sides", densities by tile count,
   preview and heightfield import in offset coordinates, the new default.
   Landed.
   Test: `tools/hex_heightfield_test` pins the offset frame
   (corner offsets in thirds and halves, ties rounded up);
   `tools/hex_map_shape_test` pins the image frame against enumeration,
   the vertex slot band and the noise cache bound,
   and the density scale at D = 1 and for 150×150×150;
   `tools/heightfield_import_test.py` imports a relief at D = 1,
   into a 9×7×5 world (13×13 pixels, the six corner tiles must differ)
   and a regular hexagon, and compares every tile and the off-map cells
   with an exact model.
   The dialog itself has no headless test.
5. **Enlarge.** W and H grow with D fixed;
   the new-region loops use the predicate.
   Test: enlarge the fixture to a larger W and H;
   old tiles are unchanged, every new on-map tile has ground,
   and the tile count matches the formula.
6. **Rotation.** #281's 60° steps, built on `rotate_tile`.
   Half turns landed on any W×H×D;
   the single step waits for this plan and the 6-orientation footprint
   (`TODO.md`, "Rotation cascade").
   Test: the rotation round trip on a D > 1 fixture.
