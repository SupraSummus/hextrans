# Viewport-only map rotation

Design doc for the 60° map-rotate feature.  This describes live
behaviour; the residual cleanup work is tracked in `TODO.md` under
"Rotation cascade".

## Two coordinate frames

The renderer reads tile data through a `view_rotation` (0..5, stored
on `viewport_t`).  Each step is 60° CCW around the viewport centre
tile (`ij_off`).  World data is not mutated — the rotation re-labels
which world tile lands at each screen position, exactly like the
upstream 90° square rotation does.

Two axial frames are in scope:

  - **world-axial** `(q, r)`: as stored in `planquadrat_t* plan[]`,
    immutable across view rotations.
  - **view-axial** `(vq, vr)`: the screen projection's input frame.
    The 6 standard view-axial neighbour vectors
    `(1,0), (0,1), (-1,1), (-1,0), (0,-1), (1,-1)` always project to
    the same 6 fixed screen positions in `hex_proj.h`'s iso 2:1
    lattice; only their world-axial identity rotates.

Bridge between the frames:

    view  = hex_axial_rotate_inv(world - ij_off, view_rotation)
    world = hex_axial_rotate    (view,           view_rotation) + ij_off

`hex_axial_rotate` (in `display/hex_proj.h`) applies the integer hex
rotation `60° · steps CCW`.  The 6-cycle is unrolled to a switch.

`viewport_t::view_axial_to_world(koord)` is the world-frame public
helper — same as `hex_axial_rotate(delta, view_rotation)` but reads
better at call sites.  `viewport_t::get_world_view_ij_off()` returns
the world-axial offset from the centre tile to the screen top-left,
i.e. `view_axial_to_world(view_ij_off)`.

## What is and isn't a screen rotation

The iso 2:1 hex lattice is *not* 6-fold rotationally symmetric in
screen pixels.  The 6 hex neighbours of a tile project to screen
positions sitting at angles 0°, 71.6°, 108.4°, 180°, 251.6°, 288.4°
relative to "12 o'clock" — three pairs symmetric about the vertical,
with alternating 71.6°/36.8° spacing.  The lattice has 2-fold (180°)
symmetry, no more.

So a view rotation step does *not* visually rotate the framebuffer by
60°.  It permutes which world tile sits at each of those 6 fixed
screen positions.  Cycling rotation 0→1→…→5→0 walks every world tile
through all 6 neighbour screen positions and back; an N-S road on
the world map appears as one of the 3 way axes (NS, NE-SW, NW-SE)
depending on rotation, with 2 of the 6 rotations sharing each axis.

This matches upstream's 90° square rotation in spirit: there too
"rotation" permuted the 4 neighbour-screen-positions rather than
visually rotating the screen.  The illusion of visual rotation in
upstream came from the `obj_t::rotate90` cascade re-orienting every
building's sprite alongside the world reshuffle; the hex viewport-
only equivalent of that (pakset layout selection at draw time) is
stage 2 — see "Rotation cascade" in `TODO.md`.

## Integration sites

`get_screen_coord` (world tile → screen pixel), the
`get_ground_on_screen_coordinate` / `get_new_cursor_position`
inverse, the `main_view_t::display` and
`main_view_t::display_region` render loops, the bbox computation
for `prepare_tiles`, the panning-step normalisation in
`change_world_position(koord, sint16, sint16)`, the screen-anchored
variant `change_world_position(pos, off, sc)`, and the z-elevation
2D compensation in `get_map2d_coord` all consume the rotation.

The mouse picker `pick_nearest_hex_vertex_global` iterates over the
7 hex-neighbours-plus-centre candidate set in view-axial (so the
projection-distance math is rotation-invariant), and rotates each
candidate to world-axial for the tile lookup.  The hex_corner enum
returned to the terraform tool is the world corner; the cursor
sprite draw offset rotates to the screen position of that corner
under the current view.

`tool_rotate90_t::init` (`tool/simtool.cc`) calls
`viewport_t::rotate_view_step()` and returns `false` — false here
means "no persistent tool to keep selected", consistent with the
upstream rotate-button behaviour.

## What still reads `settings.rotation`

The compass widget, the minimap projection, the pakset building
layout selector, the road-vehicle direction sprite slot and the
schedule-rotation helpers all still read
`settings_t::get_rotation()` — which under viewport-only is frozen
at 0.  These are stage-2 migration sites; until they're moved over,
the compass shows N upright regardless of `view_rotation`, the
minimap doesn't rotate, and buildings/vehicles keep their world-
frame sprite orientation (so a building visually faces the same
screen direction across all 6 rotations, instead of cycling through
the available pakset layouts).  Tracked under "Rotation cascade" in
`TODO.md`.

## Save format

`view_rotation` is *not* persisted.  Loading a save resets the view
to rotation 0; the world data is unchanged.  This is the right
default — view rotation is a session-local preference, not part of
the world state — but if a per-player view preference becomes
desirable it would slot into the next save-version bump alongside
the wider save-format cluster.
