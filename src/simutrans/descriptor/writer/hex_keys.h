/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_WRITER_HEX_KEYS_H
#define DESCRIPTOR_WRITER_HEX_KEYS_H

/*
 * Shared tabfile-key vocabulary for direction-keyed writer entries
 * (`image[ns]`, `start[n]`, `ramp[ne]`, …).
 *
 * Pakset .dat files use parameter expansion for these keys.
 * `tabfile_t::find_parameter_expansion` treats `,` and `-` inside
 * `[…]` as parameter-list mode, so axis keys use `_` as separator.
 *
 * Remaining direction-keyed writers should consume these constants
 * on port rather than reinvent the convention.  See TODO.md
 * "Engine → pakset descriptor boundary" for the per-writer status.
 */
namespace hex_keys {
	/// 3 hex way-axes for segment / pillar-style entries.
	extern const char* const axis_names[3];

	/// 6 hex edges for start / ramp / portal-style entries, in the
	/// order N, S, NE, SE, SW, NW.  Matches `bridge_desc_t::img_t`'s
	/// `*_Start` / `*_Ramp` enum order.
	extern const char* const edge_names[6];
}

#endif
