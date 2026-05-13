/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef OBJ_WAY_WAY_IMAGE_SLOT_H
#define OBJ_WAY_WAY_IMAGE_SLOT_H


#include "../../display/simimg.h"
#include "../../dataobj/ribi.h"


class way_desc_t;
class cbuffer_t;


/**
 * The "slot" of a way descriptor's image table the renderer picked for
 * a way: either the slope-image table keyed on slope (when the way's
 * body must follow a non-flat tile) or the flat-image table keyed on
 * ribi.  The slot is the engine's intent; the resolved image_id is
 * the pakset's projection.  Tests assert on intent to stay
 * pakset-content-independent.
 *
 * `weg_t::pick_image_slot()` is the single source of truth for which
 * slot the engine would pick.  `calc_image()`, `check_season()` and
 * out-of-line callers (tunnel mouths, schiene reservation, the build
 * tool) all go through the same slot type, so engine intent never
 * forks.
 */
class way_image_slot_t
{
public:
	enum class kind_t : uint8 {
		none = 0,   ///< way's image is borrowed by another object (bruecke_t / tunnel_t).  `apply_image_slot(none)` is a no-op so the borrower's sprite stays in place; callers needing an explicit IMG_EMPTY clear should call `set_image(IMG_EMPTY)` directly.
		flat,       ///< desc->get_image_id(ribi, snow)
		slope,      ///< desc->get_slope_image_id(slope, snow): way crosses the full slope axis.
		slope_half, ///< desc->get_slope_half_image_id(slope, high_half, snow): way ends on the slope's low edge (high_half=false) or high edge (high_half=true).  `high_half_` carries that bit.
	};

	way_image_slot_t() = default;

	static way_image_slot_t for_none()                                                  { return way_image_slot_t(kind_t::none,       0,         false, false); }
	static way_image_slot_t for_flat (ribi_t::ribi r,    bool snow)                     { return way_image_slot_t(kind_t::flat,       (sint16)r,  snow,  false); }
	static way_image_slot_t for_slope(slope_t::type sl, bool snow)                      { return way_image_slot_t(kind_t::slope,      (sint16)sl, snow,  false); }
	static way_image_slot_t for_slope_half(slope_t::type sl, bool high_half, bool snow) { return way_image_slot_t(kind_t::slope_half, (sint16)sl, snow,  high_half); }

	kind_t        kind()    const { return kind_; }
	ribi_t::ribi  get_ribi()  const { return (ribi_t::ribi)(uint8)key_; } ///< only meaningful when kind() == flat
	slope_t::type get_slope() const { return (slope_t::type)key_; }       ///< only meaningful when kind() == slope or slope_half
	bool          is_high_half() const { return high_half_; }             ///< only meaningful when kind() == slope_half
	bool          is_snow() const { return snow_; }

	/// Resolve to the desc's image_id for this slot.  Returns IMG_EMPTY
	/// for kind() == none, for a NULL desc, or when the desc has no
	/// image at this slot (missing pakset coverage).
	image_id resolve(const way_desc_t* desc, bool front) const;

	/// Append a stable canonical label for tests / debug output, in
	/// the same vocabulary the makeobj way-writer uses for `.dat`
	/// keys: "image[<ribi_key>]" for flat, "imageup[<slope_key>]"
	/// for slope, plain "none" for kind() == none.  Snow rows get a
	/// trailing "@snow".  Examples: "image[s]", "image[s_n]",
	/// "image[-]", "imageup[s_double]", "imageup[ne]@snow", "none".
	/// Slope values outside the 18-slot table fall back to
	/// "imageup[raw_<int>]" so they remain distinguishable.
	void to_label(cbuffer_t& buf) const;

	bool operator==(const way_image_slot_t& o) const { return kind_ == o.kind_ && key_ == o.key_ && snow_ == o.snow_ && high_half_ == o.high_half_; }
	bool operator!=(const way_image_slot_t& o) const { return !(*this == o); }

private:
	way_image_slot_t(kind_t k, sint16 key, bool snow, bool high_half) : kind_(k), key_(key), snow_(snow), high_half_(high_half) {}

	kind_t kind_     = kind_t::none;
	sint16 key_      = 0;     ///< ribi for flat, slope_t::type for slope / slope_half; 0 for none
	bool   snow_     = false;
	bool   high_half_ = false; ///< only meaningful when kind() == slope_half
};


#endif
