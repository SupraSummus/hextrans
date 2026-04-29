/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_GEOMETRY_H
#define DESCRIPTOR_SYNTH_GEOMETRY_H


#include "../dataobj/ribi.h"
#include "../display/hex_proj.h"
#include "../simconst.h"
#include "../simtypes.h"

namespace synth_overlay {


inline uint8 hex_corner_height(slope_t::type slope, hex_corner_t::type corner)
{
	switch (corner) {
		case hex_corner_t::E:  return (uint8)corner_e(slope);
		case hex_corner_t::SE: return (uint8)corner_se(slope);
		case hex_corner_t::SW: return (uint8)corner_sw(slope);
		case hex_corner_t::W:  return (uint8)corner_w(slope);
		case hex_corner_t::NW: return (uint8)corner_nw(slope);
		case hex_corner_t::NE: return (uint8)corner_ne(slope);
		default: return 0;
	}
}


struct synth_hex_geometry_t {
	sint32 u;
	sint32 w;
	sint32 h;
	sint32 lift;
	sint32 top_pad;
	sint32 top_y;
	sint32 mid_y;
	sint32 bot_y;
	sint32 vx[hex_corner_t::count];
	sint32 vy_base[hex_corner_t::count];

	sint32 image_y() const { return hex_visible_centre_y((sint16)w) - u - top_pad; }

	sint32 vy(slope_t::type slope, hex_corner_t::type corner) const
	{
		return vy_base[corner] - (sint32)hex_corner_height(slope, corner) * lift;
	}
};


inline synth_hex_geometry_t synth_hex_geometry(sint32 u, sint16 height_step)
{
	synth_hex_geometry_t g;
	g.u = u;
	g.w = 4 * u;
	// Per-step screen lift comes from `hex_height_raster_scale_y` so the
	// sprite bbox tracks any change to the display-side z scale without a
	// duplicated /2 here.  height-3 corners (base-4 encoding) need 3*lift
	// headroom; top_pad = 4*lift keeps a comfortable extra step of margin
	// and matches the legacy 2*full_lift absolute pixel count.
	g.lift    = hex_height_raster_scale_y(height_step, g.w);
	g.top_pad = 4 * g.lift;
	g.h = 2 * u + g.top_pad;
	g.top_y = g.top_pad;
	g.mid_y = g.top_pad + u;
	g.bot_y = g.top_pad + 2 * u - 1;

	g.vx[hex_corner_t::E ] = g.w - 1;  g.vy_base[hex_corner_t::E ] = g.mid_y;
	g.vx[hex_corner_t::SE] = 3 * u;    g.vy_base[hex_corner_t::SE] = g.bot_y;
	g.vx[hex_corner_t::SW] = u;        g.vy_base[hex_corner_t::SW] = g.bot_y;
	g.vx[hex_corner_t::W ] = 0;        g.vy_base[hex_corner_t::W ] = g.mid_y;
	g.vx[hex_corner_t::NW] = u;        g.vy_base[hex_corner_t::NW] = g.top_y;
	g.vx[hex_corner_t::NE] = 3 * u;    g.vy_base[hex_corner_t::NE] = g.top_y;
	return g;
}


} // namespace synth_overlay


#endif
