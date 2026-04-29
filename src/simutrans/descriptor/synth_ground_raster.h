/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_SYNTH_GROUND_RASTER_H
#define DESCRIPTOR_SYNTH_GROUND_RASTER_H


#include "synth_geometry.h"
#include "synth_plane_partition.h"
#include "../dataobj/ribi.h"
#include "../simcolor.h"
#include "../simtypes.h"

#include <cmath>
#include <cstring>


/**
 * Header-only core of the synth ground tile rasteriser.  Exposes the
 * pure pixel-rasterisation step (`rasterise_ground` and helpers) split
 * out of `synth_overlay.cc` so external tools can call it without
 * linking the rest of the engine (image_t, simgraph, ...).  See
 * `tools/synth_capture/` for the standalone capture tool that writes
 * the engine's flat-tile output as PPM for the parametric pipeline
 * on the pakset side.
 *
 * synth_overlay.cc consumes the same functions for its in-engine
 * `build_ground` path; there is no duplicated rasterisation logic.
 */
namespace synth_overlay {


/// Number of climate slots `get_ground` accepts.  Indexing matches
/// the `climate_image[]` block the pakset path uses: 0..6 = climate-1
/// (desert..arctic non-snow), 7 = snow.
static const uint8 ground_climate_slots = 8;


/// Sentinel for "no pixel here" in a w*h scratch buffer used during
/// ground rasterisation.  The synth palette lives in 0x0001..0x7FFF,
/// so 0 is never produced as a tile pixel and is safe to use.
static const PIXVAL GROUND_NO_PIXEL = 0;


// Climate base colours (RGB555 PIXVAL).  Picked to roughly match
// pak64 climate colour intent — desert = sandy yellow, tropic = dark
// green, ..., arctic = pale grey, snow = near-white.  Indexing
// matches the `climate_image[]` block the pakset path uses: 0..6 =
// climate-1 (desert..arctic non-snow), 7 = snow.  Values live in the
// rgb pixel range 0..0x7FFF — bit 15 is reserved for player-color
// slots and special palette entries.
//
// Hand-picked rather than read from pakset's `boden_texture`: that
// texture is a tiled noise pattern, not a single colour, and the
// mid-tone we'd average out of it doesn't always match what a hex
// climate "ought to look like" (e.g. tundra averages to a muddy grey
// that reads as rocky next to it).  Easier to specify the palette
// here and tune by eyeball than to pull mid-tones from a sprite that
// may not even be loaded yet on synth init.
#define SYNTH_RGB555(r, g, b) (PIXVAL)(((r) << 10) | ((g) << 5) | (b))
static const PIXVAL CLIMATE_RGB555[ground_climate_slots] = {
	SYNTH_RGB555(28, 26, 17), // 0 desert
	SYNTH_RGB555( 6, 14,  5), // 1 tropic
	SYNTH_RGB555(15, 17,  8), // 2 mediterran
	SYNTH_RGB555(11, 17,  8), // 3 temperate
	SYNTH_RGB555(12, 12,  6), // 4 tundra
	SYNTH_RGB555(15, 15, 15), // 5 rocky
	SYNTH_RGB555(20, 22, 22), // 6 arctic non-snow
	SYNTH_RGB555(29, 29, 29), // 7 snow
};
#undef SYNTH_RGB555


inline void decode_corner_heights(slope_t::type slope, uint8 ch[hex_corner_t::count])
{
	ch[hex_corner_t::E ] = (uint8)corner_e (slope);
	ch[hex_corner_t::SE] = (uint8)corner_se(slope);
	ch[hex_corner_t::SW] = (uint8)corner_sw(slope);
	ch[hex_corner_t::W ] = (uint8)corner_w (slope);
	ch[hex_corner_t::NW] = (uint8)corner_nw(slope);
	ch[hex_corner_t::NE] = (uint8)corner_ne(slope);
}


// Multiply an RGB555 PIXVAL by `brightness/256`, clamping each
// channel to [0, 31].  Caller is the per-face shading pass below;
// `brightness` lives in [128, 352] (= 0.5x .. 1.375x) after the
// calibrated Lambert remap; only the upper clamp is live for
// saturated base colours.
inline PIXVAL shade_pixval(PIXVAL p, sint32 brightness)
{
	sint32 r = ((p >> 10) & 0x1F) * brightness / 256;
	sint32 g = ((p >>  5) & 0x1F) * brightness / 256;
	sint32 b = ( p        & 0x1F) * brightness / 256;
	if(  r > 31  ) { r = 31; }
	if(  g > 31  ) { g = 31; }
	if(  b > 31  ) { b = 31; }
	return (PIXVAL)((r << 10) | (g << 5) | b);
}


// Generic scanline polygon fill into a w*h scratch buffer.  Handles
// non-convex polygons via the even-odd rule: at each scanline y,
// gather x-intersections of every edge, sort, fill spans between
// alternate pairs.  Lifted hex slopes mostly stay convex, but
// double-height-adjacent-corners cases (e.g. NE=2 with NW=0) can
// briefly invert a vertex above its neighbour and produce a
// concave silhouette; even-odd handles that without separate
// casework.  Out-of-range pixels are clipped silently.
inline void synth_fill_polygon(PIXVAL* buf, sint32 w, sint32 h,
                               const sint32* xs, const sint32* ys, int n,
                               PIXVAL color)
{
	sint32 y_min = ys[0], y_max = ys[0];
	for(  int i = 1;  i < n;  i++  ) {
		if(  ys[i] < y_min  ) { y_min = ys[i]; }
		if(  ys[i] > y_max  ) { y_max = ys[i]; }
	}
	if(  y_min < 0      ) { y_min = 0; }
	if(  y_max >= h     ) { y_max = h - 1; }

	for(  sint32 y = y_min;  y <= y_max;  y++  ) {
		// Each edge contributes at most one x-intersection per
		// scanline; bound the buffer by polygon size.  A 6-sided
		// hex polygon is fine with 8 slots.
		sint32 xints[8];
		int nx = 0;
		for(  int i = 0;  i < n  &&  nx < (int)(sizeof(xints)/sizeof(xints[0]));  i++  ) {
			const int j = (i + 1) % n;
			const sint32 ya = ys[i];
			const sint32 yb = ys[j];
			if(  ya == yb  ) { continue; } // skip horizontal edges
			const sint32 y_lo = ya < yb ? ya : yb;
			const sint32 y_hi = ya < yb ? yb : ya;
			// Half-open [y_lo, y_hi) — textbook scanline parity so a
			// vertex shared by two edges counts once (avoids odd hit
			// counts on interior scanlines of sloped wedges).  The flat
			// hex SE–SW bottom is horizontal (skipped above); that row is
			// closed in `rasterise_ground` after the wedge fills.
			if(  y < y_lo  ||  y >= y_hi  ) { continue; }
			const sint32 xa = xs[i];
			const sint32 xb = xs[j];
			const sint32 x_int = xa + (y - ya) * (xb - xa) / (yb - ya);
			xints[nx++] = x_int;
		}
		// Insertion sort; n <= 6 so simplicity beats asymptotics.
		for(  int i = 1;  i < nx;  i++  ) {
			const sint32 v = xints[i];
			int j = i - 1;
			while(  j >= 0  &&  xints[j] > v  ) {
				xints[j + 1] = xints[j];
				j--;
			}
			xints[j + 1] = v;
		}
		for(  int i = 0;  i + 1 < nx;  i += 2  ) {
			sint32 x0 = xints[i];
			sint32 x1 = xints[i + 1];
			if(  x0 < 0   ) { x0 = 0; }
			if(  x1 >= w  ) { x1 = w - 1; }
			for(  sint32 x = x0;  x <= x1;  x++  ) {
				buf[y * w + x] = color;
			}
		}
	}
}


/**
 * Rasterise the synth ground tile for @p slope at climate @p climate_idx
 * into @p buf (size `geom.w * geom.h`, expected zero-initialised).  Pure
 * pixel writes; no allocation, no RLE, no image_t — the in-engine path
 * (`synth_overlay::build_ground`) wraps this, and external tools can
 * call it directly to dump the engine's authoritative output as a
 * raster (see `tools/synth_capture/`).
 *
 * Geometry: `4u × 2u` lattice footprint plus headroom for lifted
 * vertices (full bbox in @p geom).  Per-region planar Lambert shading
 * driven by `synth_ground_lambert_brightness` (light `L = (-1, 1, 2)`,
 * flat tile = 1.0×).
 */
inline void rasterise_ground(PIXVAL* buf, const synth_hex_geometry_t &geom,
                             slope_t::type slope, uint8 climate_idx,
                             const plane_partition::hex_partition_t &partition)
{
	const sint32 w = geom.w;
	const sint32 h = geom.h;
	const PIXVAL base = CLIMATE_RGB555[climate_idx];

	uint8 ch[hex_corner_t::count];
	decode_corner_heights(slope, ch);

	// Vertex screen Y after lift (x from geom.vx).  Order matches hex_corner_t.
	sint32 vy[hex_corner_t::count];
	for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
		vy[i] = geom.vy_base[i] - (sint32)ch[i] * geom.lift;
	}

	for(  uint8 ri = 0;  ri < partition.region_count;  ri++  ) {
		const plane_partition::hex_region_t &reg = partition.region[ri];
		if(  reg.len < 3  ) {
			continue;
		}

		const uint8 i0 = reg.v[0];
		uint8 i1 = reg.v[1];
		uint8 i2 = reg.v[2];
		double nx = 0.0, ny = 0.0, nz = 0.0;
		bool have_normal = false;
		for(  uint8 k = 2;  k < reg.len;  k++  ) {
			i1 = reg.v[k - 1];
			i2 = reg.v[k];
			const double ax = (double)(geom.vx[i1] - geom.vx[i0]);
			const double ay = (double)(vy[i1] - vy[i0]);
			const double az = (double)((sint32)ch[i1] * geom.lift - (sint32)ch[i0] * geom.lift);
			const double bx = (double)(geom.vx[i2] - geom.vx[i0]);
			const double by = (double)(vy[i2] - vy[i0]);
			const double bz = (double)((sint32)ch[i2] * geom.lift - (sint32)ch[i0] * geom.lift);
			nx = ay * bz - az * by;
			ny = az * bx - ax * bz;
			nz = ax * by - ay * bx;
			if(  nx != 0.0 || ny != 0.0 || nz != 0.0  ) {
				have_normal = true;
				break;
			}
		}
		if(  !have_normal  ) {
			nx = 0.0;
			ny = 0.0;
			nz = 1.0;
		}
		if(  nz < 0.0  ) {
			nx = -nx;
			ny = -ny;
			nz = -nz;
		}
		const sint32 brightness = synth_ground_lambert_brightness(nx, ny, nz);
		const PIXVAL face_color = shade_pixval(base, brightness);

		sint32 xs[hex_corner_t::count];
		sint32 ys[hex_corner_t::count];
		for(  uint8 i = 0;  i < reg.len;  i++  ) {
			xs[i] = geom.vx[reg.v[i]];
			ys[i] = vy[reg.v[i]];
		}
		synth_fill_polygon(buf, w, h, xs, ys, reg.len, face_color);

		// `synth_fill_polygon` uses half-open scanline crossing and
		// skips horizontal edges by design (for parity correctness).
		// Seal every horizontal boundary edge here so top/bottom rows
		// of planar regions are filled deterministically.
		for(  uint8 i = 0;  i < reg.len;  i++  ) {
			const uint8 j = (uint8)((i + 1) % reg.len);
			if(  ys[i] != ys[j]  ) {
				continue;
			}
			const sint32 y = ys[i];
			if(  y < 0 || y >= h  ) {
				continue;
			}
			sint32 x0 = xs[i];
			sint32 x1 = xs[j];
			if(  x0 > x1  ) {
				const sint32 t = x0;
				x0 = x1;
				x1 = t;
			}
			if(  x0 < 0   ) { x0 = 0; }
			if(  x1 >= w  ) { x1 = w - 1; }
			for(  sint32 x = x0;  x <= x1;  x++  ) {
				buf[y * w + x] = face_color;
			}
		}
	}
}


} // namespace synth_overlay


#endif
