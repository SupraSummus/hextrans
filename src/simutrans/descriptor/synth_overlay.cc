/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "synth_overlay.h"

#include "ground_desc.h"
#include "image.h"
#include "../display/simgraph.h"
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


// Build the outline image for one slope, half (fg or bg).  The
// flat-top hex is inscribed in the template marker's bounding box
// (E and W vertices on the horizontal extremes at mid-y, NE/SE/NW/SW
// at quarter-width on the top and bottom rows), with each vertex
// lifted by `corner_h * tile_raster_scale_y(TILE_HEIGHT_STEP, w)` for
// slope-aware outlines.  Hex slopes share corners across 3 tiles —
// that's a terrain-storage problem (per AGENTS.md "per-vertex height
// storage") not a marker problem; the marker only ever shows one
// tile's view of its own corners and is consistent by construction.
//
// Lifted vertices may project above the template's top edge; the
// line drawer clips them.  Visually the outline tops can get cut off
// for double-height raised corners — not a concern for the cursor
// at single-height terrain, revisit if the cropping shows up.
static image_t* build_outline(const image_t* tmpl, slope_t::type slope, bool background)
{
	const sint32 w = tmpl->w;
	const sint32 h = tmpl->h;
	const sint32 mid_y = h / 2;
	const sint32 top_y = 0;
	const sint32 bot_y = h - 1;
	const sint16 lift = (sint16)tile_raster_scale_y(TILE_HEIGHT_STEP, w);

	// Vertex screen coords in image-local pixel space, ordered to
	// match hex_corner_t::type so the path tables below index in.
	// Y grows down; corner height lifts UP, so subtract.
	struct vertex_t { sint32 x, y; };
	const vertex_t v[hex_corner_t::count] = {
		{ w - 1,     mid_y - corner_e(slope)  * lift }, // E
		{ (w*3) / 4, bot_y - corner_se(slope) * lift }, // SE
		{  w    / 4, bot_y - corner_sw(slope) * lift }, // SW
		{ 0,         mid_y - corner_w(slope)  * lift }, // W
		{  w    / 4, top_y - corner_nw(slope) * lift }, // NW
		{ (w*3) / 4, top_y - corner_ne(slope) * lift }, // NE
	};

	// Vertex visit order around each half of the outline.  Front
	// half = three S-side edges; back half = three N-side edges.
	// E and W endpoints are shared between the halves.
	static const hex_corner_t::type front_path[4] = {
		hex_corner_t::E, hex_corner_t::SE, hex_corner_t::SW, hex_corner_t::W
	};
	static const hex_corner_t::type back_path[4] = {
		hex_corner_t::E, hex_corner_t::NE, hex_corner_t::NW, hex_corner_t::W
	};
	const hex_corner_t::type* path = background ? back_path : front_path;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	for(  int i = 0;  i < 3;  i++  ) {
		const vertex_t& a = v[path[i]];
		const vertex_t& b = v[path[i + 1]];
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
	img->x = tmpl->x;
	img->y = tmpl->y;
	img->zoomable = tmpl->zoomable;

	return img;
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
			// half-open [y_lo, y_hi) — a vertex shared by two edges
			// only contributes once, avoiding double-counting at
			// scanline = vertex.y.
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
// `brightness` lives in [128, 384] (= 0.5x .. 1.5x), so each channel
// can over-flow but never under-flow — only the upper clamp is live.
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
// Geometry mirrors `build_outline`: hex inscribed in the template's
// W * H bounding box (E and W vertices on the horizontal extremes
// at mid-y; NE/SE/NW/SW at quarter-width on the top and bottom
// rows), each vertex lifted by `corner_h * tile_raster_scale_y`.
// The tile is split into 6 triangle faces meeting at the centre,
// each face shaded by Lambertian on its world-space normal so that
// slopes read as 3D rather than as flat coloured hexes.
static image_t* build_ground(const image_t* tmpl, slope_t::type slope, uint8 climate_idx)
{
	const sint32 w = tmpl->w;
	const sint32 h = tmpl->h;
	const sint32 mid_y = h / 2;
	const sint32 top_y = 0;
	const sint32 bot_y = h - 1;
	const sint16 lift = (sint16)tile_raster_scale_y(TILE_HEIGHT_STEP, w);
	const PIXVAL base = CLIMATE_RGB555[climate_idx];

	const uint8 ch[hex_corner_t::count] = {
		(uint8)corner_e (slope),
		(uint8)corner_se(slope),
		(uint8)corner_sw(slope),
		(uint8)corner_w (slope),
		(uint8)corner_nw(slope),
		(uint8)corner_ne(slope),
	};

	// Vertex screen coords (after lift).  Order matches hex_corner_t.
	const sint32 vx[hex_corner_t::count] = {
		w - 1,        // E
		(w * 3) / 4,  // SE
		 w      / 4,  // SW
		0,            // W
		 w      / 4,  // NW
		(w * 3) / 4,  // NE
	};
	const sint32 vy_base[hex_corner_t::count] = {
		mid_y, // E
		bot_y, // SE
		bot_y, // SW
		mid_y, // W
		top_y, // NW
		top_y, // NE
	};
	sint32 vy[hex_corner_t::count];
	for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
		vy[i] = vy_base[i] - (sint32)ch[i] * lift;
	}

	const sint32 cx = w / 2;
	const sint32 cy_base = mid_y;
	// Centre height — average of the 6 corners.  Picked over max /
	// min so that a flat-top dome and a flat-bottom valley both
	// shade reasonably; centre = max() makes valleys look caved-in
	// and centre = min() makes domes look razored.  Integer-rounded
	// since lift is integer pixels.
	sint32 sum_h = 0;
	for(  int i = 0;  i < hex_corner_t::count;  i++  ) {
		sum_h += ch[i];
	}
	const sint32 cz = (sum_h * lift) / hex_corner_t::count; // pixels
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
		const double ax = (double)(vx[a]      - cx);
		const double ay = (double)(vy_base[a] - cy_base);
		const double az = (double)((sint32)ch[a] * lift - cz);
		const double bx = (double)(vx[b]      - cx);
		const double by = (double)(vy_base[b] - cy_base);
		const double bz = (double)((sint32)ch[b] * lift - cz);

		const double nx = ay * bz - az * by;
		const double ny = az * bx - ax * bz;
		const double nz = ax * by - ay * bx;
		const double n_norm = std::sqrt(nx*nx + ny*ny + nz*nz);

		// Lambertian cos(θ) in [-1, 1] mapped to brightness factor
		// [0.5, 1.5] (× 256 to stay integer for shade_pixval).
		// Floor at 0.5x so back-facing faces don't disappear into
		// black; ceiling at 1.5x clamps highlight before colour
		// blowout.  All-flat slope falls through at brightness=256
		// (= 1.0x).
		sint32 brightness = 256;
		if(  n_norm > 0.0  ) {
			const double cos_theta = (nx*Lx + ny*Ly + nz*Lz) / (n_norm * L_norm);
			brightness = 256 + (sint32)(cos_theta * 128.0);
			if(  brightness < 128  ) { brightness = 128; }
			if(  brightness > 384  ) { brightness = 384; }
		}

		const PIXVAL face_color = shade_pixval(base, brightness);

		const sint32 fxs[3] = { cx, vx[a], vx[b] };
		const sint32 fys[3] = { cy, vy[a], vy[b] };
		fill_polygon(buf, w, h, fxs, fys, 3, face_color);
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
	img->x = tmpl->x;
	img->y = tmpl->y;
	img->zoomable = tmpl->zoomable;

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

	if(  ground_desc_t::marker == NULL  ) {
		dbg->warning("synth_overlay::init", "no Marker pakset; synth disabled, falling back to legacy projection");
		return;
	}
	const image_t* tmpl = ground_desc_t::marker->get_image_ptr(0);
	if(  tmpl == NULL  ||  tmpl->w <= 0  ||  tmpl->h <= 0  ) {
		dbg->warning("synth_overlay::init", "no usable Marker template (image 0); synth disabled");
		return;
	}

	for(  int s = 0;  s < slope_t::max_slopes;  s++  ) {
		for(  int half = 0;  half < 2;  half++  ) {
			image_t* img = build_outline(tmpl, (slope_t::type)s, half == 1);
			img->register_image();
			marker[half][s] = img;
		}
		for(  int c = 0;  c < ground_climate_slots;  c++  ) {
			image_t* img = build_ground(tmpl, (slope_t::type)s, (uint8)c);
			img->register_image();
			ground[c][s] = img;
		}
	}

	initialised = true;
	DBG_DEBUG("synth_overlay::init",
	          "synthesised %d marker + %d ground sprites (w=%d h=%d)",
	          slope_t::max_slopes * 2,
	          slope_t::max_slopes * ground_climate_slots,
	          tmpl->w, tmpl->h);
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


} // namespace synth_overlay
