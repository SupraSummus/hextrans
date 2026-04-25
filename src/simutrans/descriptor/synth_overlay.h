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
 * tile-cursor and grid-line overlays.
 *
 * The hex port reuses the legacy square pakset for ground art via
 * `ground_desc_t::project_to_square_sprite`, which collapses 6-corner
 * slopes onto the 15 square sprites.  That works for textured ground
 * but produces a square outline for the cursor and the grid overlay,
 * which then visibly disagrees with the hex tile geometry.
 *
 * This module synthesises hex-shaped outline `image_t`s at startup
 * and registers them through the same `gfx->register_image` path the
 * pakset reader uses.  The lookup functions in `ground_desc_t`
 * consult synth first; when a future hex-aware pakset arrives, the
 * lookup can prefer pakset art and let synth idle as a fallback floor.
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

} // namespace synth_overlay

#endif
