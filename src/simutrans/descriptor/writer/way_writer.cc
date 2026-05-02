/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string>
#include "../../dataobj/tabfile.h"
#include "../../utils/simstring.h"
#include "obj_node.h"
#include "obj_pak_exception.h"
#include "../way_desc.h"
#include "text_writer.h"
#include "imagelist_writer.h"
#include "skin_writer.h"
#include "get_waytype.h"
#include "way_writer.h"

using std::string;

/**
 * Write a waytype description node
 */
void way_writer_t::write_obj(FILE* outfp, obj_node_t& parent, tabfileobj_t& obj)
{
	// Hex-ribi flat imagelist: one slot per hex ribi value, indexed
	// directly by the 6-bit ribi.  Bit positions match
	// `ribi_t::_ribi`: SE=1, S=2, SW=4, NW=8, N=16, NE=32.  Each
	// slot's .dat key joins the bit names low-to-high with `_` —
	// e.g. ribi=18 → bits {S, N} → `s_n`.  Slot 0 (no connection)
	// is keyed as `-`.  `_` is mandatory because `,` and `-`
	// inside `[…]` trigger
	// `tabfile_t::find_parameter_expansion`'s parameter-list mode.
	static const char* const hex_dir_name[6] = {
		"se", "s", "sw", "nw", "n", "ne"
	};
	auto hex_ribi_code = [&](uint8 r) -> std::string {
		if (r == 0) return "-";
		std::string s;
		for (uint8 b = 0; b < 6; b++) {
			if (!(r & (1 << b))) continue;
			if (!s.empty()) s += '_';
			s += hex_dir_name[b];
		}
		return s;
	};

	// Slope-up image slot order.  6 narrow (2-corner) hex edge slopes
	// clockwise from north, then their 6 wide (4-corner) variants in
	// the same order.  Order matches way_desc_t::get_slope_image_id;
	// double-height edges and square diagonals are no longer
	// way-buildable.  `_` separates suffix because `,` and `-` inside
	// `[…]` trigger tabfile_t::find_parameter_expansion's parameter-
	// list mode.
	static const char* const slope_keys[12] = {
		"n", "ne", "se", "s", "sw", "nw",
		"n_wide", "ne_wide", "se_wide", "s_wide", "sw_wide", "nw_wide"
	};
	static const char* const image_type[] = { "", "front" };

	const sint64 price       = obj.get_int64("cost",        100);
	const sint64 maintenance = obj.get_int64("maintenance", 100);
	const sint32 topspeed    = obj.get_int("topspeed",    999);
	const uint32 max_weight  = obj.get_int("max_weight",  999);
	const uint16 axle_load   = obj.get_int("axle_load",  9999);
	const uint8  clip_below  = obj.get_int_clamped("clip_below", 1, 0, 1);         // clip ground below (if elevated way)


	uint16 intro = obj.get_int("intro_year", DEFAULT_INTRO_YEAR) * 12;
	intro += obj.get_int("intro_month", 1) - 1;

	uint16 retire = obj.get_int("retire_year", DEFAULT_RETIRE_YEAR) * 12;
	retire += obj.get_int("retire_month", 1) - 1;

	uint8 wtyp = get_waytype(obj.get("waytype"));
	uint8 styp = obj.get_int("system_type", 0);

	// compatibility conversions
	if (wtyp == track_wt && styp == 5) {
		wtyp = monorail_wt;
	}
	else if (wtyp == track_wt && styp == 7) {
		wtyp = tram_wt;
	}

	// true to draw as foregrund and not much earlier (default)
	uint8 draw_as_ding = (obj.get_int("draw_as_ding", 0) == 1);
	sint8 number_of_seasons = 0;

	obj_node_t node(this, 37, &parent);

	node.write_version(outfp, 8);
	node.write_sint64(outfp, price);
	node.write_sint64(outfp, maintenance);
	node.write_sint32(outfp, topspeed);
	node.write_uint32(outfp, max_weight);
	node.write_uint16(outfp, intro);
	node.write_uint16(outfp, retire);
	node.write_uint16(outfp, axle_load);
	node.write_uint8 (outfp, wtyp);
	node.write_uint8 (outfp, styp);
	node.write_uint8 (outfp, draw_as_ding);
	node.write_uint8 (outfp, clip_below);


	// Try `<key>[<season>]` first; for season 0, fall back to bare
	// `<key>` so paksets without a season axis still work.
	auto get_keyed = [&](const std::string& key, int season) -> std::string {
		const std::string suffixed = key + "[" + std::to_string(season) + "]";
		std::string s = obj.get(suffixed.c_str());
		if (s.empty() && season == 0) s = obj.get(key.c_str());
		return s;
	};

	// `image[-]` (slot 0) is mandatory; winter is signalled by a
	// non-empty `image[-][1]`.
	if (get_keyed("image[-]", 0).empty()) {
		dbg->fatal("way_writer_t::write_obj", "image with label image[-] missing");
	}
	number_of_seasons = get_keyed("image[-]", 1).empty() ? 0 : 1;

	node.write_sint8(outfp, number_of_seasons);
	write_name_and_copyright(outfp, node, obj);

	slist_tpl<std::string> keys;
	for (size_t backtofront = 0; backtofront < lengthof(image_type); backtofront++) {
		const std::string prefix = image_type[backtofront];
		for (uint8 season = 0; season <= number_of_seasons; season++) {
			// Flat per-ribi sprites — one slot per hex ribi value.
			for (uint8 r = 0; r < ribi_t::all + 1; r++) {
				keys.append(get_keyed(prefix + "image[" + hex_ribi_code(r) + "]", season));
			}
			imagelist_writer_t::instance()->write_obj(outfp, node, keys);
			keys.clear();

			// Slope-up sprites — 12 fixed slots, indexed by slope value
			// in way_desc_t::get_slope_image_id.  Empty entries become
			// IMG_EMPTY so indices stay aligned.
			for (uint32 i = 0; i < lengthof(slope_keys); ++i) {
				keys.append(get_keyed(prefix + "imageup[" + slope_keys[i] + "]", season));
			}
			imagelist_writer_t::instance()->write_obj(outfp, node, keys);
			keys.clear();

			// Diagonal sprites — upstream's `_diagonal` smooth-bend
			// variant.  Hex has no out-of-axis diagonal direction
			// (every direction lies on an axis), so this block is
			// always empty under hex; the imagelist node is still
			// emitted because `way_desc::image_list_base_index`
			// keeps a `+2` offset for it.
			imagelist_writer_t::instance()->write_obj(outfp, node, keys);

			if (season == 0 && backtofront == 0) {
				slist_tpl<string> cursorkeys;
				cursorkeys.append(string(obj.get("cursor")));
				cursorkeys.append(string(obj.get("icon")));
				cursorskin_writer_t::instance()->write_obj(outfp, node, obj, cursorkeys);
			}
		}
	}

	node.check_and_write_header(outfp);
}
