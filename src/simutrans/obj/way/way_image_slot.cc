/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "way_image_slot.h"

#include "../../descriptor/way_desc.h"
#include "../../descriptor/way_image_keys.h"
#include "../../utils/cbuffer.h"


image_id way_image_slot_t::resolve(const way_desc_t* desc, bool front) const
{
	if (kind_ == kind_t::none || desc == NULL) {
		return IMG_EMPTY;
	}
	switch (kind_) {
		case kind_t::flat:  return desc->get_image_id      ((ribi_t::ribi)(uint8)key_, snow_, front);
		case kind_t::slope: return desc->get_slope_image_id((slope_t::type)     key_, snow_, front);
		case kind_t::none:  break;
	}
	return IMG_EMPTY;
}


// Label format mirrors the .dat slot keys exactly: `image[<ribi_key>]`
// for the flat table, `imageup[<slope_key>]` for the slope table.
// Snow rows get `@snow`.  Slope values that fall outside the 18-slot
// table (transient mid-terraform shapes) are spelled with `raw_<int>`
// inside the brackets so they remain distinguishable instead of
// collapsing to a generic "other".
void way_image_slot_t::to_label(cbuffer_t& buf) const
{
	switch (kind_) {
		case kind_t::none:
			buf.append("none");
			return;
		case kind_t::flat:
			buf.printf("image[%s]", way_image_keys::ribi_key((uint8)key_).c_str());
			break;
		case kind_t::slope: {
			const slope_t::type sl = (slope_t::type)key_;
			if (const char* k = way_image_keys::slope_key(sl)) {
				buf.printf("imageup[%s]", k);
			}
			else {
				buf.printf("imageup[raw_%d]", (int)sl);
			}
			break;
		}
	}
	if (snow_) buf.append("@snow");
}
