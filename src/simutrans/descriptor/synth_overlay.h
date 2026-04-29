/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_OVERLAY_H
#define DESCRIPTOR_SYNTH_OVERLAY_H


#include "../display/simimg.h"
#include "../dataobj/ribi.h"


/**
 * Code-generated overlay sprites — an "algorithmic pakset" for the
 * tile-cursor, grid-line, and cliff-face overlays.
 *
 * This module synthesises hex-shaped `image_t`s at startup and
 * registers them through the same `gfx->register_image` path the
 * pakset reader uses.  The active families today:
 *
 *   - `get_marker(slope, half)` — outline-only markers for the
 *     cursor and grid-line overlays (front + back halves drawn
 *     bracketing tile content).
 *   - `get_border(slope)` — grid-line border overlays.
 *   - `get_back_wall(wall, index, artificial)` — placeholder cliff
 *     faces for the hex-only back-wall geometry.
 *
 * Ground tiles are pakset-owned again: hex-aware `HexLightTexture`
 * images are read through `ground_desc_t::init_ground_textures`.
 */
namespace synth_overlay {

/**
 * Precedence flag between synth and pakset overlay sprites.
 *
 *   true  — synth wins when it has an answer; pakset is the fallback
 *           floor.  Default — fitting today, where the only available
 *           pakset art is square-projected and visibly mismatches the
 *           hex tile geometry.
 *   false — pakset wins when it has an answer; synth fills in for
 *           slopes the pakset doesn't cover.  Flip when a pakset
 *           ships hex-aware overlay art that should take over.
 *
 * Single knob covers marker, border, and cliff overlays.  Live —
 * the lookup functions read it on every call, so flipping at
 * runtime takes effect on the next frame draw.  Wire to env_t /
 * simuconf.tab when settings UI lands; for now flip from the
 * debugger / a code patch.
 */
extern bool prefer_over_pakset;

/**
 * Generate hex-shaped marker sprites for every slope and register
 * them with the graphics system.  Call from
 * `ground_desc_t::init_ground_textures` after pakset load and after
 * `image_offset` is set, so synth images are tracked alongside the
 * other runtime-generated ground textures and freed together on the
 * next world (re)load.
 *
 * Idempotent — a second call frees the previously-synthesised
 * image_t's and rebuilds them.  Bails out (logs a warning, leaves
 * `get_marker` returning IMG_EMPTY) if no template marker is
 * available in the pakset; in that case the legacy square-projection
 * path stays in charge.
 */
void init();

/**
 * Marker image for @p slope.  @p background = true returns the rear
 * half of the outline, drawn behind tile content (vehicles, buildings)
 * so they render on top; false returns the front half drawn over.
 *
 * Returns IMG_EMPTY when synth has not been initialised, or when
 * @p slope is out of range (< 0 or >= slope_t::max_slopes).  Callers
 * should fall back to the legacy lookup in that case.
 */
image_id get_marker(slope_t::type slope, bool background);


/**
 * Grid-line border image for @p slope — a hex-shaped 6-edge outline,
 * the synth equivalent of the pakset `Borders` block consulted by
 * `ground_desc_t::get_border_image`.  One image per slope (no
 * front/back split): the grid overlay is drawn once over the tile,
 * not bracketed around it like the cursor marker.
 *
 * Geometry mirrors `get_marker` (same `4u × 2u` inscribed bbox,
 * same per-corner lift) so the synth grid lines and the synth
 * cursor share the same pixel-level outline; toggling between them
 * doesn't shift the visible hex perimeter.
 *
 * Returns IMG_EMPTY when synth has not been initialised, or when
 * @p slope is out of range.  Callers should fall back to the legacy
 * pakset lookup in that case.
 */
image_id get_border(slope_t::type slope);


/// Number of "back walls" (cliff faces against screen-up neighbours)
/// the synth covers.  Mirrors `grund_t::BACK_WALL_COUNT`.  Wall 0 is
/// the NW-neighbour cliff (along this hex's NW edge), wall 1 is the
/// N-neighbour cliff (along this hex's N edge), wall 2 is the
/// NE-neighbour cliff (along this hex's NE edge).
static const uint8 back_wall_count = 3;

/// Number of distinct cliff-face sprites per wall (matches the
/// `(h1, h2)` encoding produced by `get_back_image_from_diff` in
/// `grund.cc`: 9 single-step shapes plus 2 middle-slope shapes).
/// Mirrors `grund_t::WALL_IMAGE_COUNT`.
static const uint8 back_wall_image_count = 11;


/**
 * Cliff-face sprite for back-wall @p wall (0 = NW edge, 1 = N edge,
 * 2 = NE edge) with image index @p index (0..10) under the encoding
 * produced by `get_back_image_from_diff`: index 0 = no cliff, 1..8 =
 * `(h1, h2)` for `h1, h2 ∈ {0, 1, 2}` with `index = h1 + 3*h2`, 9..10 =
 * middle slopes of double-height stacks.  @p artificial picks the
 * fundament (man-made platform) palette; false picks the natural-cliff
 * palette, matching the sign of `back_imageid` (`< 0` → fundament).
 *
 * Geometry is anchored against `synth_hex_geometry` so the cliff
 * face attaches along this tile's NW, N or NE hex edge rather than the
 * legacy diamond silhouette.  Vertical lift uses the same `geom.lift`
 * (= `hex_height_raster_scale_y(TILE_HEIGHT_STEP, W)`) as the shared
 * hex per-corner lift and as `simview.cc`'s tile world-z translation, so
 * callers must pair this with the matching `hex_height_raster_scale_y`
 * for the `back_height` shift in the cliff yoff.
 *
 * Returns IMG_EMPTY when synth has not been initialised, the wall
 * index is out of range, the image index is out of range, or the
 * decoded cliff has no visible face (index 0).  Callers should fall
 * back to the pakset `fundament` / `slopes` lookup in that case.
 */
image_id get_back_wall(uint8 wall, uint8 index, bool artificial);

} // namespace synth_overlay

#endif
