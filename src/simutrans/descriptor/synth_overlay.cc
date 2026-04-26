/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "synth_overlay.h"

#include "ground_desc.h"
#include "image.h"
#include "synth_geometry.h"
#include "../display/simgraph.h"
#include "../display/hex_proj.h"
#include "../simconst.h"
#include "../simdebug.h"
#include "../dataobj/environment.h"
#include "../dataobj/koord.h"

#include <cmath>
#include <cstring>


namespace synth_overlay {


bool prefer_over_pakset = true;


// Outline color in the runtime PIXVAL layout — RGB555 with no flag
// bit, bits 0..4 = B, 5..9 = G, 10..14 = R (matches the red_comp /
// green_comp / blue_comp macros in ground_desc.cc).  Values >= 0x8000
// are reserved for player-color slots and special palette entries
// (see simgraph16.cc register_image), so RGB pixels live in
// 0x0000..0x7FFF and are read directly without rgbmap remap.
//
// Bright yellow: R=31, G=31, B=0 → (31<<10) | (31<<5) = 0x7FE0.
static const PIXVAL OUTLINE_COLOR = 0x7FE0;

// Sentinel for "no pixel here" in the raw scratch buffer used during
// outline rasterisation.  The outline color is non-zero (0x7FE0), so
// 0 is never produced as an outline pixel and is safe to use.
static const PIXVAL NO_PIXEL = 0;

// One image_t per slope per half — index 0 = front half, 1 = back half
// (matches the @p background bool exposed in the public API).  Eager
// build in init(), get_marker() is then a flat table read; this also
// dodges thread-safety questions in the multi-threaded display path.
// Memory budget: hex outlines RLE-compress to ~1-2 KB each, so the
// full set fits comfortably under 3 MB.
static image_t* marker[2][slope_t::max_slopes];

// Per-climate-slot, per-slope filled hex ground tiles.  Eager build
// alongside marker; lookup is a flat array read.  Memory budget:
// each filled hex RLE-compresses to ~3-4 KB at pak64 raster width,
// so the full 8 * 729 set is ~20 MB.  Acceptable but the dominant
// cost; consider lazy generation only if it becomes a problem.
static image_t* ground[ground_climate_slots][slope_t::max_slopes];

// Per-slope alpha-mask hex tiles, RLE-shape-identical to ground[].
// Paired with the synth ground source by the climate / beach /
// snowline overlay blits — see get_alpha in the header.
static image_t* alpha[slope_t::max_slopes];

// Per-slope grid-line border tiles — single-image full hex outline,
// drawn over the tile when `grund_t::show_grid` is on.  Mirrors the
// marker geometry but combines both halves into one image since the
// grid overlay is a single draw call (`grund_t::display_boden`) and
// is not bracketed around tile content.
static image_t* border[slope_t::max_slopes];

static bool initialised = false;


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
#define RGB555(r, g, b) (PIXVAL)(((r) << 10) | ((g) << 5) | (b))
static const PIXVAL CLIMATE_RGB555[ground_climate_slots] = {
	RGB555(28, 26, 17), // 0 desert
	RGB555( 6, 14,  5), // 1 tropic
	RGB555(15, 17,  8), // 2 mediterran
	RGB555(11, 17,  8), // 3 temperate
	RGB555(12, 12,  6), // 4 tundra
	RGB555(15, 15, 15), // 5 rocky
	RGB555(20, 22, 22), // 6 arctic non-snow
	RGB555(29, 29, 29), // 7 snow
};
#undef RGB555


static void free_all()
{
	for(  int half = 0;  half < 2;  half++  ) {
		for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
			delete marker[half][s];
			marker[half][s] = NULL;
		}
	}
	for(  int c = 0;  c < ground_climate_slots;  c++  ) {
		for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
			delete ground[c][s];
			ground[c][s] = NULL;
		}
	}
	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		delete alpha[s];
		alpha[s] = NULL;
	}
	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		delete border[s];
		border[s] = NULL;
	}
}


// Bresenham line into a w*h scratch buffer; out-of-bounds pixels
// are clipped (silently dropped) so callers can pass vertices that
// extend above the image when corner heights are raised.
static void draw_line(PIXVAL* buf, sint32 w, sint32 h,
                      sint32 x0, sint32 y0, sint32 x1, sint32 y1,
                      PIXVAL color)
{
	const sint32 dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
	const sint32 dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
	const sint32 sx = x0 < x1 ? 1 : -1;
	const sint32 sy = y0 < y1 ? 1 : -1;
	sint32 err = dx + dy;
	while(  true  ) {
		if(  x0 >= 0  &&  x0 < w  &&  y0 >= 0  &&  y0 < h  ) {
			buf[y0 * w + x0] = color;
		}
		if(  x0 == x1  &&  y0 == y1  ) {
			break;
		}
		const sint32 e2 = 2 * err;
		if(  e2 >= dy  ) { err += dy; x0 += sx; }
		if(  e2 <= dx  ) { err += dx; y0 += sy; }
	}
}


// Encode a w*h scratch buffer into the simgraph16 RLE format.
// Each row is encoded as
//     <start_x> { <run_len> <run_pixels...> <skip> }* 0
// with the trailing 0 (skip) marking end-of-line; an empty row is
// the canonical [0][0][0].  The decoder lives in simgraph16.cc and
// drives RLE consumption from `len`, which the caller sets to the
// returned PIXVAL count.
static size_t encode_rle(const PIXVAL* buf, sint32 w, sint32 h, PIXVAL* out)
{
	PIXVAL* p = out;
	for(  sint32 y = 0;  y < h;  y++  ) {
		const PIXVAL* row = buf + y * w;
		sint32 x = 0;
		while(  x < w  &&  row[x] == NO_PIXEL  ) { x++; }
		if(  x >= w  ) {
			// empty row
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
			continue;
		}
		*p++ = (PIXVAL)x;
		while(  x < w  ) {
			const sint32 run_start = x;
			while(  x < w  &&  row[x] != NO_PIXEL  ) { x++; }
			const sint32 run_len = x - run_start;
			*p++ = (PIXVAL)run_len;
			for(  sint32 i = 0;  i < run_len;  i++  ) {
				*p++ = row[run_start + i];
			}
			const sint32 skip_start = x;
			while(  x < w  &&  row[x] == NO_PIXEL  ) { x++; }
			if(  x >= w  ) {
				*p++ = 0;  // end-of-line
				break;
			}
			*p++ = (PIXVAL)(x - skip_start);
		}
	}
	return p - out;
}


// Rasterise a sequence of hex-vertex line segments into an image.
// The flat-top hex footprint is a `4u × 2u` lattice cell (E and W
// vertices on the horizontal extremes at mid-y, NE/SE/NW/SW at
// quarter-width on the top and bottom rows), with each vertex lifted
// by `corner_h * tile_raster_scale_y(TILE_HEIGHT_STEP, 4u)` for
// slope-aware outlines.  The footprint dimensions are dictated by the
// hex iso lattice in `hex_proj.h` (column step `(3u, u)`, row step
// `(0, 2u)`).  The image bbox adds headroom above that footprint for
// the maximum 2-corner lift; without it, raised N-side vertices clip at
// local y=0 even though neighbouring endpoints mathematically meet.
// See `tools/hex_proj_test/hex_proj_test.cc ::
// test_synth_slope_bbox_contains_lifted_vertices`.
//
// @p path is a list of corner indices to visit in order; @p closed
// adds the wraparound edge from the last vertex back to the first.
// `path = {E, SE, SW, W}, closed=false` is the front half of the
// marker outline (3 south-side edges); `closed=true` over all 6
// corners is the full grid border.
//
static image_t* rasterise_outline(sint32 u, slope_t::type slope,
                                  const hex_corner_t::type* path, int n,
                                  bool closed)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;

	// Vertex screen coords in image-local pixel space, ordered to
	// match hex_corner_t::type so @p path indexes in directly.
	// Y grows down; corner height lifts UP, so subtract.
	struct vertex_t { sint32 x, y; };
	const vertex_t v[hex_corner_t::count] = {
		{ geom.vx[hex_corner_t::E ], geom.vy(slope, hex_corner_t::E ) }, // E
		{ geom.vx[hex_corner_t::SE], geom.vy(slope, hex_corner_t::SE) }, // SE
		{ geom.vx[hex_corner_t::SW], geom.vy(slope, hex_corner_t::SW) }, // SW
		{ geom.vx[hex_corner_t::W ], geom.vy(slope, hex_corner_t::W ) }, // W
		{ geom.vx[hex_corner_t::NW], geom.vy(slope, hex_corner_t::NW) }, // NW
		{ geom.vx[hex_corner_t::NE], geom.vy(slope, hex_corner_t::NE) }, // NE
	};

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	const int n_edges = closed ? n : n - 1;
	for(  int i = 0;  i < n_edges;  i++  ) {
		const vertex_t& a = v[path[i]];
		const vertex_t& b = v[path[(i + 1) % n]];
		draw_line(buf, w, h, a.x, a.y, b.x, b.y, OUTLINE_COLOR);
	}

	// Encode into RLE.  Worst-case bound for alternating opaque/skip
	// is 3 PIXVALs per pixel + per-row header, comfortably below
	// w*h*2 + h*4.
	const size_t cap = (size_t)w * h * 2 + (size_t)h * 4 + 4;
	PIXVAL* tmp = new PIXVAL[cap];
	const size_t rle_len = encode_rle(buf, w, h, tmp);
	delete [] buf;

	image_t* img = new image_t(rle_len);
	memcpy(img->data, tmp, rle_len * sizeof(PIXVAL));
	delete [] tmp;

	img->w = (scr_coord_val)w;
	img->h = (scr_coord_val)h;
	img->x = 0;
	img->y = geom.image_y();
	img->zoomable = 1;

	return img;
}


// Marker outline half — three S-side edges (front) or three N-side
// edges (back); E and W endpoints are shared between the halves so
// the two halves bracket tile content (vehicles, buildings) cleanly.
//
// Hex slopes share corners across 3 tiles — that's a terrain-storage
// problem (per AGENTS.md "per-vertex height storage") not a marker
// problem; the marker only ever shows one tile's view of its own
// corners and is consistent by construction.
static image_t* build_marker(sint32 u, slope_t::type slope, bool background)
{
	static const hex_corner_t::type front_path[4] = {
		hex_corner_t::E, hex_corner_t::SE, hex_corner_t::SW, hex_corner_t::W
	};
	static const hex_corner_t::type back_path[4] = {
		hex_corner_t::E, hex_corner_t::NE, hex_corner_t::NW, hex_corner_t::W
	};
	const hex_corner_t::type* path = background ? back_path : front_path;
	return rasterise_outline(u, slope, path, 4, /*closed=*/false);
}


// Grid border — full closed 6-edge hex outline, drawn once over the
// tile when `grund_t::show_grid` is on.  Unlike the marker, the grid
// is not split into front/back halves: it's a single draw call atop
// tile content (`grund_t::display_boden`), so all 6 edges live in
// one image.
//
// Adjacent tiles draw their own borders on the same shared edge.
// Per-tile slope storage means two neighbours can disagree on the
// height of a shared vertex once terraforming touches them, and the
// two grid lines will visibly mismatch at that vertex.  Retired by
// per-vertex height storage (AGENTS.md "Critical findings driving
// priority"); same caveat as the synth ground tile.
static image_t* build_border(sint32 u, slope_t::type slope)
{
	static const hex_corner_t::type full_path[hex_corner_t::count] = {
		hex_corner_t::E,  hex_corner_t::SE, hex_corner_t::SW,
		hex_corner_t::W,  hex_corner_t::NW, hex_corner_t::NE,
	};
	return rasterise_outline(u, slope, full_path, hex_corner_t::count, /*closed=*/true);
}


// Generic scanline polygon fill into a w*h scratch buffer.  Handles
// non-convex polygons via the even-odd rule: at each scanline y,
// gather x-intersections of every edge, sort, fill spans between
// alternate pairs.  Lifted hex slopes mostly stay convex, but
// double-height-adjacent-corners cases (e.g. NE=2 with NW=0) can
// briefly invert a vertex above its neighbour and produce a
// concave silhouette; even-odd handles that without separate
// casework.  Out-of-range pixels are clipped silently.
static void fill_polygon(PIXVAL* buf, sint32 w, sint32 h,
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
			// closed in build_ground after the wedge fills.
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


// Multiply an RGB555 PIXVAL by `brightness/256`, clamping each
// channel to [0, 31].  Caller is the per-face shading pass below;
// `brightness` lives in [96, 416] (= 0.375x .. 1.625x) after the
// Lambert remap in `build_ground`; only the upper clamp is live for
// saturated base colours.
static PIXVAL shade_pixval(PIXVAL p, sint32 brightness)
{
	sint32 r = ((p >> 10) & 0x1F) * brightness / 256;
	sint32 g = ((p >>  5) & 0x1F) * brightness / 256;
	sint32 b = ( p        & 0x1F) * brightness / 256;
	if(  r > 31  ) { r = 31; }
	if(  g > 31  ) { g = 31; }
	if(  b > 31  ) { b = 31; }
	return (PIXVAL)((r << 10) | (g << 5) | b);
}


// Build the filled hex ground tile for one (slope, climate_idx).
// Geometry mirrors `build_outline`: a `4u × 2u` lattice footprint (E
// and W vertices on the horizontal extremes at mid-y; NE/SE/NW/SW at
// quarter-width on the top and bottom rows), plus bbox headroom for
// lifted vertices.
//
// The tile is split into 6 triangle faces meeting at the centre,
// each face shaded by Lambertian on its world-space normal so that
// slopes read as 3D rather than as flat coloured hexes.
static image_t* build_ground(sint32 u, slope_t::type slope, uint8 climate_idx)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;
	const PIXVAL base = CLIMATE_RGB555[climate_idx];

	const uint8 ch[hex_corner_t::count] = {
		(uint8)corner_e (slope),
		(uint8)corner_se(slope),
		(uint8)corner_sw(slope),
		(uint8)corner_w (slope),
		(uint8)corner_nw(slope),
		(uint8)corner_ne(slope),
	};

	// Vertex screen Y after lift (x from geom.vx).  Order matches hex_corner_t.
	sint32 vy[hex_corner_t::count];
	for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
		vy[i] = geom.vy_base[i] - (sint32)ch[i] * geom.lift;
	}

	const sint32 cx = w / 2;
	const sint32 cy_base = geom.mid_y;
	// Centre height — average of the 6 corners.  Picked over max /
	// min so that a flat-top dome and a flat-bottom valley both
	// shade reasonably; centre = max() makes valleys look caved-in
	// and centre = min() makes domes look razored.  Integer-rounded
	// since lift is integer pixels.
	sint32 sum_h = 0;
	for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
		sum_h += ch[i];
	}
	const sint32 cz = (sum_h * geom.lift) / hex_corner_t::count; // pixels
	const sint32 cy = cy_base - cz;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	// Light direction in world space: above and slightly to the
	// upper-right (toward the NE corner).  Picked so south-facing
	// faces (those whose corners are lower than centre on the
	// south side of the tile) come out darker — matches the iso
	// convention pak64 uses, where the light source is high in the
	// north.  Components in (x, y_screen, z_screen) where +z_screen
	// = "up" in screen-space (= world height in pixels).
	const double Lx =  1.0;
	const double Ly = -1.0;
	const double Lz =  3.0;
	const double L_norm = std::sqrt(Lx*Lx + Ly*Ly + Lz*Lz);

	// 6 faces, each a triangle (centre, corner_a, corner_b) with the
	// two corners adjacent on the hex boundary.  Boundary walk
	// E → SE → SW → W → NW → NE follows hex_corner_t's enum order
	// 0..5, so face f's corners are simply (f, (f+1) % 6).
	for(  int f = 0;  f < hex_corner_t::count;  f++  ) {
		const uint8 a = (uint8)f;
		const uint8 b = (uint8)((f + 1) % hex_corner_t::count);

		// World-space edges from centre.  Face normal = a × b — our
		// boundary walk is screen-CW (= world-CW with screen-Y-down),
		// the winding order that makes a × b point in +z for a flat
		// tile.  Flip the operands and flat tiles come out dark.
		// `synth_ground_lambert_face_normal` matches fill_polygon.
		double nx, ny, nz;
		synth_ground_lambert_face_normal(geom, slope, cz, a, b, &nx, &ny, &nz);
		const double n_norm = std::sqrt(nx*nx + ny*ny + nz*nz);

		// Lambertian cos(θ) in [-1, 1] mapped to brightness around
		// 1.0×256, with a wider swing than legacy pak lightmaps so
		// slope relief reads clearly on the synth hex tiles.  Floor
		// keeps back faces off pure black; ceiling caps highlights.
		// All-flat slope falls through at brightness=256 (= 1.0x).
		sint32 brightness = 256;
		if(  n_norm > 0.0  ) {
			const double cos_theta = (nx*Lx + ny*Ly + nz*Lz) / (n_norm * L_norm);
			brightness = 256 + (sint32)(cos_theta * 176.0);
			if(  brightness < 96   ) { brightness = 96; }
			if(  brightness > 416  ) { brightness = 416; }
		}

		const PIXVAL face_color = shade_pixval(base, brightness);

		const sint32 fxs[3] = { cx, geom.vx[a], geom.vx[b] };
		const sint32 fys[3] = { cy, vy[a], vy[b] };
		fill_polygon(buf, w, h, fxs, fys, 3, face_color);

		// Boundary chord parallel to x (e.g. flat-top hex SE–SW): both
		// slanted edges use y_hi == that row, so half-open contributes
		// zero crossings while the horizontal rim is skipped in the
		// edge loop above.  Fill the rim explicitly; skip vy==cy
		// (degenerate horizontal triangle) to avoid double-painting the
		// centre scanline.
		if(  vy[a] == vy[b]  &&  vy[a] != cy  ) {
			const sint32 y = vy[a];
			if(  y >= 0  &&  y < h  ) {
				sint32 x0 = geom.vx[a];
				sint32 x1 = geom.vx[b];
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

	// Encode into RLE.  Worst case as in build_outline: 2 PIXVALs per
	// pixel + per-row header + tail.
	const size_t cap = (size_t)w * h * 2 + (size_t)h * 4 + 4;
	PIXVAL* tmp = new PIXVAL[cap];
	const size_t rle_len = encode_rle(buf, w, h, tmp);
	delete [] buf;

	image_t* img = new image_t(rle_len);
	memcpy(img->data, tmp, rle_len * sizeof(PIXVAL));
	delete [] tmp;

	img->w = (scr_coord_val)w;
	img->h = (scr_coord_val)h;
	img->x = 0;
	img->y = geom.image_y();
	img->zoomable = 1;

	return img;
}


// Build the alpha-mask hex tile by copying the ground tile's RLE
// verbatim and overwriting every coloured-pixel slot with a full
// opaque PIXVAL.  Copying the RLE byte-for-byte (rather than
// re-rasterising the geometry) is what guarantees byte-identical
// RLE shape to the ground tile — `display_img_alpha_wc` walks the
// source (= ground) and alpha pointers in lockstep using the
// source's RLE, and any divergence in run boundaries between the
// two tiles would walk the alpha pointer off the end of its
// allocation.
static image_t* build_alpha(const image_t* ground)
{
	image_t* img = ground->copy_rotate(0);

	const PIXVAL full_alpha = 0x7FFF;
	PIXVAL* p = img->data;
	for(  int y = 0;  y < img->h;  y++  ) {
		p++; // start_x
		do {
			sint16 runlen = *p++;
			for(  int i = 0;  i < runlen;  i++  ) {
				*p++ = full_alpha;
			}
		} while(  *p++ != 0  );
	}

	return img;
}


void init()
{
	if(  initialised  ) {
		// On world reset, gfx->free_all_images_above has already
		// cleared the gfx-side entries; we still own the image_t*
		// objects on the heap and free them here before rebuilding.
		free_all();
		initialised = false;
	}

	// The lattice unit `u = W/4` is the natural parameter for every
	// piece of hex iso geometry (column step `(3u, u)`, row step
	// `(0, 2u)`, inscribed-hex bbox `4u × 2u`).  Pinning u as the
	// builder input makes the multiples explicit at the call sites
	// and keeps any future scale change (e.g. art-larger-than-bbox
	// trim, see TODO.md) on a clean integer grid.  Inheriting from
	// a pakset template's actual w/h is not safe — pak64's Marker.pak
	// ships 64×64, which gave a 64-tall hex on a 32-tall lattice
	// slot and gapped adjacent tiles by `u` px.  See
	// `tools/hex_proj_test/hex_proj_test.cc :: test_inscribed_hex_tiles_lattice`.
	const sint32 W = (sint32)gfx->get_base_tile_raster_width();
	const sint32 u = W / 4;
	if(  u < 1  ) {
		dbg->warning("synth_overlay::init", "tile raster width %d too small; synth disabled", W);
		return;
	}

	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		for(  int half = 0;  half < 2;  half++  ) {
			image_t* img = build_marker(u, (slope_t::type)s, half == 1);
			img->register_image();
			marker[half][s] = img;
		}
		for(  int c = 0;  c < ground_climate_slots;  c++  ) {
			image_t* img = build_ground(u, (slope_t::type)s, (uint8)c);
			img->register_image();
			ground[c][s] = img;
		}
		// Alpha shape mirrors the climate-0 ground tile — all 8
		// climate ground tiles for a given slope share the same
		// geometry (only colours differ), so any climate works as
		// the donor; pick 0 by convention.
		image_t* a = build_alpha(ground[0][s]);
		a->register_image();
		alpha[s] = a;

		image_t* bd = build_border(u, (slope_t::type)s);
		bd->register_image();
		border[s] = bd;
	}

	initialised = true;
	DBG_DEBUG("synth_overlay::init",
	          "synthesised %d marker + %d ground + %d alpha + %d border sprites (u=%d, bbox %dx%d)",
	          slope_t::max_slopes * 2,
	          slope_t::max_slopes * ground_climate_slots,
	          slope_t::max_slopes,
	          slope_t::max_slopes,
	          u, synth_hex_geometry(u, TILE_HEIGHT_STEP).w,
	          synth_hex_geometry(u, TILE_HEIGHT_STEP).h);
}


image_id get_marker(slope_t::type slope, bool background)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = marker[background ? 1 : 0][slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_ground(slope_t::type slope, uint8 climate_idx)
{
	if(  !initialised
	  ||  slope < 0
	  ||  slope >= slope_t::max_slopes
	  ||  climate_idx >= ground_climate_slots  ) {
		return IMG_EMPTY;
	}
	const image_t* img = ground[climate_idx][slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_alpha(slope_t::type slope)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = alpha[slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


image_id get_border(slope_t::type slope)
{
	if(  !initialised  ||  slope < 0  ||  slope >= slope_t::max_slopes  ) {
		return IMG_EMPTY;
	}
	const image_t* img = border[slope];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


} // namespace synth_overlay
