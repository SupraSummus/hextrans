/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GROUND_BACK_IMAGE_DECODE_H
#define GROUND_BACK_IMAGE_DECODE_H

#include "../dataobj/ribi.h"


// Per-wall floors a back neighbour contributes to the hide-test of the
// current tile.  Default `2` is the legacy "no clamp" sentinel — under
// square geometry it matched the max single-tile corner height, so
// `min(corner_xx, 2)` was a no-op.  Under hex base-4 a corner can reach
// 3, so the sentinel mildly under-clamps the rare case of an
// absent-wall neighbour with a corner at 3.  Latent, inherited from
// upstream.
struct back_image_wall_floors_t {
	sint8 lh, mh, rh;
};


// Per-neighbour projection onto the three adjacent screen-up corners of
// the current tile.  `left → test[i-1]`, `mid → test[i]`,
// `right → test[i+1]`; the caller skips `left` at i=0 and `right` at
// i = corner_count - 1.
struct hide_test_contribution_t {
	sint16 left, mid, right;
};


// Decode the three base-`wall_image_count` digits of `bb` into per-wall
// floor heights against slope `s`.  `bb` is `abs(neighbour->back_imageid)`;
// values ≥ `wall_image_count^3` are fence encodings (no clamp).  Pure.
static inline back_image_wall_floors_t decode_back_image_wall_floors(sint16 bb, slope_t::type s, uint16 wall_image_count)
{
	back_image_wall_floors_t out = { 2, 2, 2 };
	const sint32 fence_offset = (sint32)wall_image_count * wall_image_count * wall_image_count;
	if (bb <= 0 || bb >= fence_offset) {
		return out;
	}
	if (bb % wall_image_count) {
		out.lh = min(corner_sw(s), corner_nw(s));
	}
	if ((bb / wall_image_count) % wall_image_count) {
		out.mh = min(corner_ne(s), corner_nw(s));
	}
	if (bb / (wall_image_count * wall_image_count)) {
		out.rh = min(corner_ne(s), corner_nw(s));
	}
	return out;
}


// Project one back neighbour's south corners onto the three adjacent
// hide-test slots.  Formulas mirror the legacy square projection;
// `back_off` and `fore_off` carry the legacy `(2-i)` / `i` y-step shape
// generalised so the back offset hits 0 at the rightmost corner and
// the forward offset hits 0 at the leftmost.  Pure.
static inline hide_test_contribution_t compute_hide_test_contribution(
	sint16 h, slope_t::type s, back_image_wall_floors_t floors,
	uint i, uint corner_count,
	sint16 scale_z, sint16 scale_y, sint16 step)
{
	const sint8 wall_floor = min(min(floors.lh, floors.mh), floors.rh);
	const sint16 back_off  = (sint16)(corner_count - 1 - i) * scale_y;
	const sint16 fore_off  = (sint16)i * scale_y;
	const sint16 step_off  = step * scale_y;

	hide_test_contribution_t out;
	out.left  = h + min(corner_sw(s), floors.lh) * scale_z + back_off + step_off;
	out.mid   = h + min(corner_se(s) * scale_z, min(corner_nw(s), wall_floor) * scale_z + scale_y) + step_off;
	out.right = h + min(corner_ne(s), floors.rh) * scale_z + fore_off + step_off;
	return out;
}


#endif
