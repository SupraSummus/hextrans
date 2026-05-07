/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_GROUND_DESC_H
#define DESCRIPTOR_GROUND_DESC_H


#include "obj_base_desc.h"
#include "image_array.h"
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
	static const ground_desc_t *fundament;  // man-made fundament cliff faces, keyed (wall, index)
	static const ground_desc_t *slopes;     // natural cliff faces, keyed (wall, index)
	static const ground_desc_t *fences;
	static const ground_desc_t *marker;
	static const ground_desc_t *borders;
	static const ground_desc_t *sidewalk; // city-road pavement, keyed by raw slope_t
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
			return image != NULL && image->get_id() != IMG_EMPTY ? image : NULL;
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
	/// under the encoding produced by `get_back_image_from_diff` in
	/// `grund.cc`: index 0 = no cliff, 1..8 = `(h1, h2)` for
	/// `h1, h2 ∈ {0, 1, 2}` with `index = h1 + 3*h2`, 9..10 = the
	/// middle slopes of double-height stacks.  @p artificial picks
	/// the fundament (man-made platform) palette; false picks the
	/// natural-cliff palette.  Pakset-owned: hex paksets ship the
	/// cliff faces in the legacy `Slopes` / `Basement` descriptors,
	/// rebaked under the hex `Image[<wall>][<index>]` layout (the
	/// upstream pak128 1D layout is gone — `display_border` was the
	/// last consumer and is itself square-grid logic, tripwire'd
	/// pending hex port).
	static image_id get_back_wall_image(uint16 index, bool artificial, uint8 wall)
	{
		return (artificial ? fundament : slopes)->get_image(wall, index);
	}

	/// Multi-step extension cliff segment for back-wall @p wall.
	/// Draws a uniform vertical face one or two height-steps tall
	/// (chosen by @p two_step), used by `grund_t::display_boden` to
	/// stack cliff segments below the final cliff face when a
	/// neighbour is more than two steps higher.  Reuses the per-step
	/// back-wall atlas: index 4 = `(h1=1, h2=1)` is the single-step
	/// uniform cliff, index 8 = `(h1=2, h2=2)` is the double-step
	/// uniform cliff.
	static image_id get_back_wall_extension_image(uint8 wall, bool two_step, bool artificial)
	{
		return get_back_wall_image(two_step ? 8 : 4, artificial, wall);
	}

	static image_id get_border_image(slope_t::type slope_in)
	{
		return borders->get_image(slope_t::lower_min_corner(slope_in));
	}

	/// City-road pavement under a `weg_t` whose `hat_gehweg()` is true,
	/// and the building footpath sprite via `gebaeude.cc` (slope=flat).
	/// `stage` is 0 = base, 1 = snow, 2 = transition; missing stages
	/// (or a missing descriptor — sidewalk is optional) return
	/// IMG_EMPTY and the caller falls back, see
	/// `boden_t::calc_image_internal`.
	static image_id get_sidewalk_image(slope_t::type slope_in, uint8 stage = 0)
	{
		return sidewalk ? sidewalk->get_image(slope_t::lower_min_corner(slope_in), stage)
		                : IMG_EMPTY;
	}
};

#endif
