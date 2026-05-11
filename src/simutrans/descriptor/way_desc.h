/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_WAY_DESC_H
#define DESCRIPTOR_WAY_DESC_H


#include "image_list.h"
#include "obj_base_desc.h"
#include "skin_desc.h"
#include "../dataobj/ribi.h"

class checksum_t;
class tool_t;

/**
 * Way type description. Contains all needed values to describe a
 * way type in Simutrans.
 *
 * Child nodes:
 *  0   Name
 *  1   Copyright
 *  2   Images for flat ways (indexed by ribi)
 *  3   Images for slopes
 *  4   Images for straight diagonal ways
 *  5   Skin (cursor and icon)
 * if number_of_seasons == 0  (no winter images)
 *  6-8  front images of image lists 2-4
 * else
 *  6-8  winter images of image lists 2-4
 *  9-11 front images of image lists 2-4
 *  12-14 front winter images of image lists 2-4
 */
class way_desc_t : public obj_desc_transport_infrastructure_t {
	friend class way_reader_t;

private:

	/**
	 * Max weight
	 */
	uint32 max_weight;

	/**
	 * Way system type: i.e. for wtyp == track this
	 * can be used to select track system type (tramlike=7, elevated=1, ignore=255)
	 */
	uint8 styp;

	/* true, if a tile with this way should be always drawn as a thing
	*/
	uint8 draw_as_obj;

	/* number of seasons (0 = none, 1 = no snow/snow
	*/
	sint8 number_of_seasons;

	/// if true front_images lists exists as nodes
	bool front_images;

	bool clip_below; // only relevant for elevated ways

	/// .dat opt-in for double-height slope builds.  Read by
	/// `way_reader_t` from version 9 nodes; older paks default false.
	bool double_slopes;

	/**
	 * calculates index of image list for flat ways
	 * for winter and/or front images
	 * add +1 and +2 to get slope and straight diagonal images, respectively
	 */
	uint16 image_list_base_index(bool snow, bool front) const
	{
		if (number_of_seasons == 0  ||  !snow) {
			if (front  &&  front_images) {
				return (number_of_seasons == 0) ? 6 : 9;
			}
			else {
				return 2;
			}
		}
		else { // winter images
			if (front  &&  front_images) {
				return 12;
			}
			else {
				return 6;
			}
		}
	}

	image_id get_river_fallback_image_id(const image_list_t *imglist, ribi_t::ribi ribi) const
	{
		// River paks are still square-art placeholders in practice:
		// the natural generator can build two-ended or bent hex ribis,
		// while the current pak128 river imagelist only has single-edge
		// slots.  Prefer the axis representative for straight river
		// runs, then any present single-edge sprite, so generated rivers
		// remain visible until real 64-slot river art lands.
		const ribi_t::ribi axis = ribi_t::straight_axis(ribi);
		if(  axis != ribi_t::none  ) {
			image_id image = imglist->get_image_id(axis);
			if(  image != IMG_EMPTY  ) {
				return image;
			}
			image = imglist->get_image_id(ribi_t::backward(axis));
			if(  image != IMG_EMPTY  ) {
				return image;
			}
		}

		for(  uint8 i = 0;  i < 6;  i++  ) {
			const ribi_t::ribi single = ribi_t::nesw[i];
			if(  (ribi & single) == 0  ) {
				continue;
			}
			const image_id image = imglist->get_image_id(single);
			if(  image != IMG_EMPTY  ) {
				return image;
			}
		}

		return imglist->get_image_id(ribi_t::none);
	}
public:

	/**
	* @return waytype used in finance stats (needed to distinguish \
	* between train track and tram track
	*/
	waytype_t get_finance_waytype() const;

	/**
	* returns the system type of this way (mostly used with rails)
	* @see systemtype_t
	*/
	systemtype_t get_styp() const { return (systemtype_t)styp; }

	bool is_tram() const { return wtyp == track_wt  &&  styp == type_tram; }

	bool is_clip_below() const { return clip_below; }

	/// Pakset imagelist is sized to `ribi_t::all + 1` slots, indexed
	/// by the full 6-bit hex ribi.  Missing slots return IMG_EMPTY
	/// and render blank.  3-way junctions reuse the same per-ribi
	/// slot as every other connectivity (the upstream extended
	/// 5-entry switch-image table didn't span the 20 hex three-way
	/// patterns and is gone).
	image_id get_image_id(ribi_t::ribi ribi, uint8 season, bool front = false) const
	{
		if (front  &&  !front_images) {
			return IMG_EMPTY;
		}
		const uint16 n = image_list_base_index(season, front);
		const image_list_t *imglist = get_child<image_list_t>(n);
		const image_id image = imglist->get_image_id(ribi);
		if(  image != IMG_EMPTY  ||  front  ||  wtyp != water_wt  ||  styp != type_river  ) {
			return image;
		}
		return get_river_fallback_image_id(imglist, ribi);
	}

	/// Slope-up sprite lookup.  Pakset imagelist holds 18 slots, one
	/// per way-buildable hex slope: 6 narrow (2-corner) edge slopes
	/// 0..5 in clockwise order from north, then their 6 wide (4-corner)
	/// variants 6..11 in the same order, then their 6 double-height
	/// (012210) variants 12..17 in the same order.  Anything else
	/// (flat, all_up sentinels, non-buildable values) returns IMG_EMPTY.
	/// Pre-double-slope paksets ship a 12-entry imagelist; slots
	/// 12..17 fall through to image_list_t::get_image_id's
	/// out-of-range IMG_EMPTY return, same as a pak that ships the
	/// keys but leaves them blank.
	image_id get_slope_image_id(slope_t::type slope, uint8 season, bool front = false) const
	{
		if (front  &&  !front_images) {
			return IMG_EMPTY;
		}
		const uint16 n = image_list_base_index(season, front) + 1;
		uint16 nr;
		switch(slope) {
			case slope_t::north_narrow:     nr = 0;  break;
			case slope_t::northeast_narrow: nr = 1;  break;
			case slope_t::southeast_narrow: nr = 2;  break;
			case slope_t::south_narrow:     nr = 3;  break;
			case slope_t::southwest_narrow: nr = 4;  break;
			case slope_t::northwest_narrow: nr = 5;  break;
			case slope_t::north_wide:     nr = 6;  break;
			case slope_t::northeast_wide: nr = 7;  break;
			case slope_t::southeast_wide: nr = 8;  break;
			case slope_t::south_wide:     nr = 9;  break;
			case slope_t::southwest_wide: nr = 10; break;
			case slope_t::northwest_wide: nr = 11; break;
			case slope_t::north_double:     nr = 12; break;
			case slope_t::northeast_double: nr = 13; break;
			case slope_t::southeast_double: nr = 14; break;
			case slope_t::south_double:     nr = 15; break;
			case slope_t::southwest_double: nr = 16; break;
			case slope_t::northwest_double: nr = 17; break;
			default:
				return IMG_EMPTY;
		}
		return get_child<image_list_t>(n)->get_image_id(nr);
	}

	image_id get_diagonal_image_id(ribi_t::ribi ribi, uint8 season, bool front = false) const
	{
		if (front  &&  !front_images) {
			return IMG_EMPTY;
		}
		const uint16 n = image_list_base_index(season, front) + 2;
		return get_child<image_list_t>(n)->get_image_id(ribi / 3 - 1);
	}

	/// True iff the .dat declares `has_double_slopes=1`.  Lets a way
	/// opt into double-height slope builds independently of which
	/// slope sprites the pakset shipped.  Stored on-disk from
	/// `way_desc` save version 9; older nodes default to `false`.
	bool has_double_slopes() const { return double_slopes; }

	/* true, if this tile is to be drawn as a normal thing */
	bool is_draw_as_obj() const { return draw_as_obj; }

	/**
	* Skin: cursor (index 0) and icon (index 1)
	*/
	const skin_desc_t * get_cursor() const
	{
		return get_child<skin_desc_t>(5);
	}

	void calc_checksum(checksum_t *chk) const;
};

#endif
