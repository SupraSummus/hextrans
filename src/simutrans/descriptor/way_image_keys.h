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

	/// Key fragment for a flat-image slot keyed on a hex ribi value.
	/// "-" for ribi == 0, otherwise the bit names joined low-to-high
	/// with "_" — e.g. ribi=18 ({S, N}) -> "s_n".
	std::string ribi_key(uint8 ribi);

	/// Key fragment for a slope-up image slot, indexed by slot number
	/// 0..17.  Order matches `way_desc_t::get_slope_image_id`'s
	/// slope -> nr mapping and the writer's iteration order, so a
	/// single change to either array shifts both in lockstep.
	extern const char* const slope_slot_keys[18];

	/// Key fragment for a slope-up image slot, by slope value.  Returns
	/// NULL when @p slope is not one of the 18 named ramp slopes
	/// (flat / single-corner / transient mid-terraform shapes have no
	/// slope-image slot).
	const char* slope_key(slope_t::type slope);
}

#endif
