/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_OVERLAY_H
#define DESCRIPTOR_SYNTH_OVERLAY_H


#include "../display/simimg.h"
#include "../dataobj/ribi.h"


/**
 * Code-generated overlay sprites — an "algorithmic pakset" for ground
 * tiles and the tile-cursor / grid-line overlays.
 *
 * The hex port reuses the legacy square pakset for ground art via
 * `ground_desc_t::project_to_square_sprite`, which collapses 6-corner
 * slopes onto the 15 square sprites.  That collapse happens inside
 * `doubleslope_to_imgnr[]`, so by the time `get_ground_tile` reads
 * `climate_image[cl] + doubleslope_to_imgnr[slope]` the 6→4 hex info
 * is already gone and every hex slope draws as the same diamond as
 * its square projection — visibly mismatched against the hex
 * lattice and unable to distinguish the 4 hex-only edge slopes (NE,
 * SE, SW, NW edges) from each other.
 *
 * This module synthesises hex-shaped `image_t`s at startup and
 * registers them through the same `gfx->register_image` path the
 * pakset reader uses.  Two families today:
 *
 *   - `get_marker(slope, half)` — outline-only markers for the
 *     cursor and grid-line overlays (front + back halves drawn
 *     bracketing tile content).
 *   - `get_ground(slope, climate_idx)` — filled hex ground tiles
 *     per climate, the synth equivalent of pakset's 15-slope ×
 *     7-climate base ground sprite block.
 *
 * The lookup functions in `ground_desc_t` consult synth first by
 * default (`prefer_over_pakset == true`) and pass the full
 * `slope_t::type` in — the 6→4 projection only happens on the
 * pakset fallback path.  When a future hex-aware pakset arrives, the
 * flag can be flipped so pakset wins and synth idles as a fallback
 * floor.
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
 * Single knob covers all overlays this module synthesises.  Live —
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


/// Number of climate slots `get_ground` accepts.  Indexing matches
/// the `climate_image[]` block the pakset path uses: 0..6 = climate-1
/// (desert..arctic non-snow), 7 = snow.
static const uint8 ground_climate_slots = 8;


/**
 * Filled hex ground tile for @p slope at climate index @p climate_idx
 * (0..7; 7 = snow).  The tile bounding box and offsets match the
 * pakset's diamond ground sprite, so the synth tile drops in at the
 * same `(xpos, ypos)` callers already pass to `gfx->draw_normal`.
 *
 * The 6 hex vertices are lifted by their per-corner height; the
 * 6-triangle interior is shaded by face normal so slopes read
 * visually instead of looking like a flat coloured hex.  This is the
 * synth equivalent of `climate_image[cl] + doubleslope_to_imgnr[slope]`,
 * but consulted *before* `doubleslope_to_imgnr` flattens hex-only
 * slopes onto their square projection.
 *
 * Returns IMG_EMPTY when synth has not been initialised, when
 * @p climate_idx is out of range, or when @p slope is out of range
 * (< 0 or >= slope_t::max_slopes).  Callers should fall back to the
 * legacy `climate_image[] + doubleslope_to_imgnr[]` lookup in that
 * case.
 */
image_id get_ground(slope_t::type slope, uint8 climate_idx);


/**
 * Hex-shaped alpha mask for @p slope, RLE-shape-identical to the
 * synth ground tile from `get_ground`.  Used by the climate and
 * snowline overlay blits in `display_img_alpha_wc`, which walks the
 * source and alpha pointers in lockstep driven by the source's RLE
 * — pairing a synth source with a synth-shaped alpha keeps the
 * walk inside both allocations.  Beach overlays still pair a pakset
 * water source with the legacy `alpha_water_image[]`; they take
 * their cue from synth only when `synth_overlay` grows a per-stage
 * water family.
 *
 * Pixel values are full opaque PIXVAL (all R/G/B at max), so the
 * blit applies the overlay uniformly across the hex.  Per-corner
 * gradient — needed for legacy-style smooth climate transitions —
 * is a future enhancement once corner masking is wired through;
 * today the corner mask is unused and transitions appear at tile
 * granularity rather than per-corner.
 *
 * Returns IMG_EMPTY when synth has not been initialised or when
 * @p slope is out of range.  Callers should fall back to the legacy
 * `alpha_image[] / alpha_corners_image[]` lookup in that case.
 */
image_id get_alpha(slope_t::type slope);


/// Number of "back walls" (cliff faces against screen-up neighbours)
/// the synth covers.  Mirrors `grund_t::BACK_WALL_COUNT`.  Wall 0 is
/// the NW-neighbour cliff (along this hex's NW edge), wall 1 is the
/// N-neighbour cliff (along this hex's N edge).  See TODO.md for the
/// missing third hex back-wall (NE neighbour) and the legacy corner-pair
/// mismatch in `calc_back_image`.
static const uint8 back_wall_count = 2;

/// Number of distinct cliff-face sprites per wall (matches the
/// `(h1, h2)` encoding produced by `get_back_image_from_diff` in
/// `grund.cc`: 9 single-step shapes plus 2 middle-slope shapes).
/// Mirrors `grund_t::WALL_IMAGE_COUNT`.
static const uint8 back_wall_image_count = 11;


/**
 * Cliff-face sprite for back-wall @p wall (0 = NW edge, 1 = N edge)
 * with image index @p index (0..10) under the encoding produced by
 * `get_back_image_from_diff`: index 0 = no cliff, 1..8 = `(h1, h2)`
 * for `h1, h2 ∈ {0, 1, 2}` with `index = h1 + 3*h2`, 9..10 = middle
 * slopes of double-height stacks.  @p artificial picks the fundament
 * (man-made platform) palette; false picks the natural-cliff palette,
 * matching the sign of `back_imageid` (`< 0` → fundament).
 *
 * Geometry is anchored against `synth_hex_geometry` so the cliff
 * face attaches along this tile's NW or N hex edge rather than the
 * legacy diamond silhouette.  Vertical lift uses the same `geom.lift`
 * (= `hex_height_raster_scale_y(TILE_HEIGHT_STEP, W)`) as synth ground
 * per-corner lift and as `simview.cc`'s tile world-z translation, so
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
