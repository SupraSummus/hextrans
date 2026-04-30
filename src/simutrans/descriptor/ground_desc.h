/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_GROUND_DESC_H
#define DESCRIPTOR_GROUND_DESC_H


#include "obj_base_desc.h"
#include "image_array.h"
#include "synth_overlay.h"
#include "../simtypes.h"
#include "../dataobj/ribi.h"


class grund_t;
class karte_t;


/**
 * Images of all possible surface tiles: slopes, climates, transitions, etc.
 *
 * Child nodes:
 *  0   Name
 *  1   Copyright
 *  2   Image-array
 */
class ground_desc_t : public obj_named_desc_t {
private:
	static karte_t *world;

	static image_id image_offset;

public:
	static uint16 water_animation_stages;
	static sint16 water_depth_levels;

	// only these textures need external access
	static const ground_desc_t *shore; // nicer shore graphics, optional
	static const ground_desc_t *fundament;
	static const ground_desc_t *slopes;
	static const ground_desc_t *fences;
	static const ground_desc_t *marker;
	static const ground_desc_t *borders;
	static const ground_desc_t *sea;     // different water depth
	static const ground_desc_t *outside;

	static char const* get_climate_name_from_bit(climate n);

	static bool double_grounds;

	/// Project any 6-corner slope onto one of the 15 single-height
	/// pakset sprites.  Always returns a valid 0..14 — the lossy
	/// projection absorbs the 2 hex-only corners (E, W) into their
	/// adjacent square corners, drops them, and clamps to single
	/// height.  Centralises the 6→4 sprite lookup until real hex art
	/// lands.
	static uint8 project_to_square_sprite(slope_t::type slope);

	// returns the pointer to an image structure
	const image_t *get_image_ptr(uint16 typ, uint16 stage=0) const
	{
		image_array_t const* const imgarray   = get_child<image_array_t>(2);
		image_list_t   const* const list = imgarray->get_list(typ);
		if(list && list->get_count() > 0) {
			image_t const* const image = imgarray->get_image(typ, stage);
			return image;
		}
		return NULL;
	}

	// image for all non-climate stuff like foundations ...
	image_id get_image(uint16 typ, uint16 stage=0) const
	{
		image_t const* const image = get_image_ptr(typ, stage);
		return image ? image->get_id() : IMG_EMPTY;
	}

	// image for all ground tiles
	static image_id get_ground_tile(grund_t *gr);

	static image_id get_water_tile(slope_t::type slope, int stage);
	static image_id get_climate_tile(climate cl, slope_t::type slope);
	static image_id get_snow_tile(slope_t::type slope);
	static image_id get_beach_tile(slope_t::type slope, uint8 corners);
	static image_id get_alpha_tile(slope_t::type slope);
	static image_id get_alpha_tile(slope_t::type slope, uint8 corners);

	static bool register_desc(const ground_desc_t *desc);

	static bool successfully_loaded();

	/**
	 * Generates ground texture images, transition maps, etc.
	 */
	static void init_ground_textures(karte_t *world);

	static image_id get_marker_image(slope_t::type slope_in, bool background)
	{
		// Pakset-owned.  Hex paksets supply marker sprites; older paksets
		// still use the legacy 4-corner projection.
		uint8 slope = double_grounds ? slope_in : project_to_square_sprite(slope_in);
		uint8 index = background ? (double_grounds ? (slope % 3) + 3 * ((uint8)(slope / 9)) + 27
		                                           : ((slope & 1) + ((slope >> 1) & 6) + 8))
		                         : (double_grounds ?  slope % 27
		                                           : (slope & 7 ));
		return marker->get_image(index);
	}

	/// Cliff-face sprite for back-wall @p wall (0 = NW edge,
	/// 1 = N edge, 2 = NE edge) with per-wall image @p index (0..10)
	/// and the pakset's `fundament` (artificial=true) vs `slopes`
	/// (artificial=false) split — the same encoding the legacy
	/// `grund_t::get_back_image(leftback)` returned, hoisted to a
	/// static so `synth_overlay` can take precedence the same way
	/// `get_marker_image` does.  Legacy pakset
	/// stacks the 2 square-era walls in one sprite list with wall 1
	/// offset by `back_wall_image_count` (= legacy `WALL_IMAGE_COUNT` =
	/// 11); applied here so callers pass the per-wall index unmodified.
	/// Wall 2 (NE neighbour) is hex-only — pakset has no sprites for
	/// it, so the pakset path returns IMG_EMPTY there and the synth
	/// fallback kicks in.
	static image_id get_back_wall_image(uint16 index, bool artificial, uint8 wall)
	{
		const uint16 pakset_offset = (uint16)(wall * synth_overlay::back_wall_image_count);
		// pakset's `fundament` / `slopes` only carry wall 0 + wall 1 sprites
		const image_id pakset_id = (wall < 2)
			? (artificial ? fundament : slopes)->get_image(index + pakset_offset)
			: IMG_EMPTY;
		const image_id synth_id = synth_overlay::get_back_wall(wall, (uint8)index, artificial);

		if(  synth_overlay::prefer_back_wall_over_pakset  ) {
			return synth_id != IMG_EMPTY ? synth_id : pakset_id;
		}
		return pakset_id != IMG_EMPTY ? pakset_id : synth_id;
	}

	/// Multi-step extension cliff segment for back-wall @p wall.  Draws a
	/// uniform vertical face one or two height-steps tall (chosen by
	/// @p two_step), used by `grund_t::display_boden` to stack cliff
	/// segments below the final cliff face when a neighbour is more than
	/// two steps higher.  The pakset paths use legacy hard-coded indices
	/// (`WALL_IMAGE_COUNT*2 + (two_step?1:0) + 2*wall`, falling back to
	/// `4 + 4*(two_step?1:0) + WALL_IMAGE_COUNT*wall` for older pakset
	/// layouts); both lists only cover wall 0 + 1 so wall 2 always lands
	/// on the synth fallback.  The synth fallback reuses the uniform
	/// single- and double-height back-wall sprites (indices 4 and 8 of
	/// `back_wall`), which are already shaped as full-height rectangular
	/// cliffs by `build_back_wall`.
	static image_id get_back_wall_extension_image(uint8 wall, bool two_step, bool artificial)
	{
		const ground_desc_t *sl_draw = artificial ? fundament : slopes;
		image_id pakset_id = IMG_EMPTY;
		if(  wall < 2  ) {
			const uint16 idx_primary = (uint16)(synth_overlay::back_wall_image_count * 2 + (two_step ? 1 : 0) + 2 * wall);
			pakset_id = sl_draw->get_image(idx_primary);
			if(  pakset_id == IMG_EMPTY  ) {
				const uint16 idx_fallback = (uint16)(4 + (two_step ? 4 : 0) + synth_overlay::back_wall_image_count * wall);
				pakset_id = sl_draw->get_image(idx_fallback);
			}
		}
		const image_id synth_id = synth_overlay::get_back_wall(wall, two_step ? 8 : 4, artificial);

		if(  synth_overlay::prefer_back_wall_over_pakset  ) {
			return synth_id != IMG_EMPTY ? synth_id : pakset_id;
		}
		return pakset_id != IMG_EMPTY ? pakset_id : synth_id;
	}

	static image_id get_border_image(slope_t::type slope_in)
	{
		return borders->get_image(slope_t::lower_min_corner(slope_in));
	}
};

#endif
