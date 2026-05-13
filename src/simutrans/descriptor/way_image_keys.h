/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_WAY_IMAGE_KEYS_H
#define DESCRIPTOR_WAY_IMAGE_KEYS_H

#include <string>

#include "../dataobj/ribi.h"


/**
 * Canonical .dat key vocabulary for way image-table slots.  Shared by
 * `way_writer_t` (emits `image[<key>]` / `imageup[<key>]` entries
 * when packing a .dat) and by `way_image_slot_t::to_label` (formats
 * the engine's runtime slot decision in the same notation).
 *
 * Engine intent and .dat content thus speak the same names: a script
 * test asserting `way.get_image_slot_id() == "imageup[s]"` is naming
 * the very slot the pakset .dat fills with `imageup[s]=...`, and the
 * writer's iteration order over flat / slope slots cannot drift from
 * what the engine reads.
 *
 * "_" is the only safe separator for multi-bit / variant keys:
 * `tabfile_t::find_parameter_expansion` treats `,` and `-` inside
 * `[…]` as parameter-list mode.
 */
namespace way_image_keys {

	/// Total number of slope-up image slots: 18 full-axis crossings
	/// (6 narrow + 6 wide + 6 double) followed by 18 low-half stubs
	/// and 18 high-half stubs in the same axis order — see
	/// `slope_slot_keys` below.
	enum : uint16 {
		SLOPE_SLOTS_FULL  = 18,
		SLOPE_SLOTS_HALF  = 36,
		SLOPE_SLOTS_TOTAL = SLOPE_SLOTS_FULL + SLOPE_SLOTS_HALF,
	};

	/// Key fragment for a flat-image slot keyed on a hex ribi value.
	/// "-" for ribi == 0, otherwise the bit names joined low-to-high
	/// with "_" — e.g. ribi=18 ({S, N}) -> "s_n".
	std::string ribi_key(uint8 ribi);

	/// Key fragment for a slope-up image slot, indexed by slot number
	/// 0..53.  Slots 0..17 are full axis crossings; slots 18..35 are
	/// low-half stubs (the way occupies the half of the tile closest
	/// to the slope's low edge); slots 36..53 are high-half stubs.
	/// Each half block mirrors the full block's axis order so
	/// half_slot = full_slot + 18 (low) or + 36 (high).
	extern const char* const slope_slot_keys[SLOPE_SLOTS_TOTAL];

	/// Key fragment for a slope-up image slot, by slope value.  Returns
	/// NULL when @p slope is not one of the 18 named ramp slopes
	/// (flat / single-corner / transient mid-terraform shapes have no
	/// slope-image slot).
	const char* slope_key(slope_t::type slope);

	/// Half-slope slot index — full slope's index (0..17) plus 18 for
	/// the low-half block or 36 for the high-half block.  Returns -1
	/// if @p slope has no slope-image slot.
	int slope_half_slot(slope_t::type slope, bool high_half);
}

#endif
