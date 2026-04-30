/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_OVERLAY_H
#define DESCRIPTOR_SYNTH_OVERLAY_H


#include "../display/simimg.h"
#include "../dataobj/ribi.h"


/**
 * Code-generated overlay sprites — an "algorithmic pakset" for
 * cliff-face overlays.
 *
 * This module synthesises hex-shaped `image_t`s at startup and
 * registers them through the same `gfx->register_image` path the
 * pakset reader uses.  The active family today:
 *
 *   - `get_back_wall(wall, index, artificial)` — placeholder cliff
 *     faces for the hex-only back-wall geometry.
 *
 * Marker and ground tiles are pakset-owned: markers through the
 * ordinary Marker descriptor, ground through hex-aware `LightTexture`
 * images read by `ground_desc_t::init_ground_textures`.
 */
namespace synth_overlay {

/**
 * Precedence flag between synth and pakset cliff sprites.
 *
 *   true  — synth wins when it has an answer; pakset is the fallback
 *           floor.
 *   false — pakset wins when it has an answer; synth fills in for
 *           slots the pakset doesn't cover.
 *
 * Back walls stay synth-first: paksets still only cover the two
 * square-era wall families, while wall 2 is hex-only.
 *
 * Live — the lookup functions read this on every call, so flipping
 * at runtime takes effect on the next frame draw.  Wire to env_t /
 * simuconf.tab when settings UI lands; for now flip from the debugger
 * / a code patch.
 */
extern bool prefer_back_wall_over_pakset;

/**
 * Generate hex-shaped cliff sprites and register them with the
 * graphics system.  Call from
 * `ground_desc_t::init_ground_textures` after pakset load and after
 * `image_offset` is set, so synth images are tracked alongside the
 * other runtime-generated ground textures and freed together on the
 * next world (re)load.
 *
 * Idempotent — a second call frees the previously-synthesised
 * image_t's and rebuilds them.
 */
void init();

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
