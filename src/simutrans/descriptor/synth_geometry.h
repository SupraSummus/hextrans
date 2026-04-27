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

#include <cmath>


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
	// Half-lift: height-3 corners (base-4 encoding) need 3*lift headroom.
	// Using lift = full_lift/2 keeps top_pad = 2*full_lift (same absolute
	// pixels as before) while satisfying top_pad >= 3*lift = 1.5*full_lift.
	const sint32 full_lift = tile_raster_scale_y(height_step, g.w);
	g.lift    = full_lift / 2;
	g.top_pad = 2 * full_lift; // = 4 * g.lift
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


/**
 * Lambert face normal for one boundary triangle in `build_ground`:
 * corners @p a and @p b (adjacent on the hex), centre at `(w/2, mid_y-cz)`
 * in screen space with z from corner heights.  Screen Y uses lifted corners
 * (`vy_base - ch*lift`) and matches `fill_polygon` — no alternate path.
 */
inline void synth_ground_lambert_face_normal(const synth_hex_geometry_t &geom,
                                             slope_t::type slope,
                                             sint32 cz,
                                             uint8 corner_a,
                                             uint8 corner_b,
                                             double *nx, double *ny, double *nz)
{
	uint8 ch[hex_corner_t::count];
	for (int i = 0; i < hex_corner_t::count; i++) {
		ch[i] = hex_corner_height(slope, (hex_corner_t::type)i);
	}

	const sint32 cx = geom.w / 2;
	const sint32 cy_centre = geom.mid_y - cz;

	const double ax = (double)(geom.vx[corner_a] - cx);
	const double ay = (double)((geom.vy_base[corner_a] - (sint32)ch[corner_a] * geom.lift) - cy_centre);
	const double az = (double)((sint32)ch[corner_a] * geom.lift - cz);
	const double bx = (double)(geom.vx[corner_b] - cx);
	const double by = (double)((geom.vy_base[corner_b] - (sint32)ch[corner_b] * geom.lift) - cy_centre);
	const double bz = (double)((sint32)ch[corner_b] * geom.lift - cz);

	*nx = ay * bz - az * by;
	*ny = az * bx - ax * bz;
	*nz = ax * by - ay * bx;
}


/**
 * Convert a face normal into the brightness multiplier used by synth ground
 * tiles.  The light is directional, but calibrated against the flat tile
 * plane: flat ground stays at exactly 1.0x and only deviations from that
 * plane produce highlights or shadows.
 */
inline sint32 synth_ground_lambert_brightness(double nx, double ny, double nz)
{
	const double Lx =  1.0;
	const double Ly = -1.0;
	const double Lz =  2.0;
	const double L_norm = std::sqrt(Lx*Lx + Ly*Ly + Lz*Lz);
	const double flat_cos = Lz / L_norm;

	sint32 brightness = 256;
	const double n_norm = std::sqrt(nx*nx + ny*ny + nz*nz);
	if(  n_norm > 0.0  ) {
		const double cos_theta = (nx*Lx + ny*Ly + nz*Lz) / (n_norm * L_norm);
		brightness = 256 + (sint32)((cos_theta - flat_cos) * 384.0);
		if(  brightness < 128  ) { brightness = 128; }
		if(  brightness > 352  ) { brightness = 352; }
	}
	return brightness;
}


} // namespace synth_overlay


#endif
