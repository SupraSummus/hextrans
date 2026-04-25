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

} // namespace synth_overlay

#endif
