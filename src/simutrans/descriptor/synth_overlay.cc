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

#include <cstring>


namespace synth_overlay {


bool prefer_back_wall_over_pakset = true;


// Sentinel for "no pixel here" in the raw scratch buffer used during
// cliff-face rasterisation.  Cliff colors are non-zero RGB555 values,
// so 0 is never produced as an opaque pixel and is safe to use.
static const PIXVAL NO_PIXEL = 0;

// Cliff-face sprites for back walls (NW and N edges).  Indexed by
// [artificial][wall][image_index], following the encoding produced by
// `get_back_image_from_diff` in `grund.cc` (see header).  The fundament
// (artificial=true) and natural-cliff (artificial=false) variants share
// geometry and differ only in palette, mirroring the
// `ground_desc_t::fundament` vs `ground_desc_t::slopes` split on the
// pakset path.  Slope is not part of the key — the cliff face only
// uses h1, h2 from the index, not this tile's overall slope.
static image_t* back_wall[2][back_wall_count][back_wall_image_count];

static bool initialised = false;


static void free_all()
{
	for(  int a = 0;  a < 2;  a++  ) {
		for(  int w = 0;  w < back_wall_count;  w++  ) {
			for(  int i = 0;  i < back_wall_image_count;  i++  ) {
				delete back_wall[a][w][i];
				back_wall[a][w][i] = NULL;
			}
		}
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


// Generic scanline polygon fill into a w*h scratch buffer.  Handles
// non-convex polygons via the even-odd rule: at each scanline y,
// gather x-intersections of every edge, sort, fill spans between
// alternate pairs.  Out-of-range pixels are clipped silently.
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
			// counts on interior scanlines of sloped wedges).  Horizontal
			// edges are closed by the caller when needed.
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
// channel to [0, 31].  Used by the synthetic cliff-face pass below.
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


// Cliff-face palette.  Natural cliffs read as exposed rock / dirt;
// fundament reads as a man-made platform.
#define RGB555(r, g, b) (PIXVAL)(((r) << 10) | ((g) << 5) | (b))
static const PIXVAL CLIFF_NATURAL = RGB555(13, 11,  7); // dirt brown
static const PIXVAL CLIFF_FUNDAMENT = RGB555(20, 20, 20); // light grey
#undef RGB555

// Per-wall shading multiplier (256 = 1.0x).  Wall 0 (NW edge) faces
// screen-up-left, wall 1 (N edge) faces screen-up, wall 2 (NE edge)
// faces screen-up-right.  These values are hand-picked: a flat-ground
// lighting calibration makes vertical walls bunch at the same dark
// end, losing wall-to-wall contrast.
static const sint32 WALL_SHADE[back_wall_count] = { 192, 224, 256 };


// Build one cliff-face sprite.  Wall 0 attaches along this hex's NW
// edge (W → NW corners in screen space); wall 1 attaches along the N
// edge (NW → NE); wall 2 attaches along the NE edge (NE → E).  The
// lower edge sits at the unlifted edge position; the upper edge is
// lifted by `h1 * geom.lift` at corner_a and `h2 * geom.lift` at
// corner_b.  The renderer composes this with a `back_height` shift
// via `yoff` at draw time, so the sprite itself only encodes the
// cliff face above that baseline.
//
// `index` follows the encoding `get_back_image_from_diff` in
// `grund.cc` produces: index 0 = no cliff, 1..8 = `(h1, h2)` for
// `h1, h2 ∈ {0, 1, 2}` with `index = h1 + 3*h2`, and 9..10 = the
// middle slopes of double-height stacks (one corner below baseline,
// one above).  Today 9 / 10 are drawn as the corresponding
// single-step half-cliffs (h1=0,h2=1 / h1=1,h2=0); see TODO.md for
// the missing notch shape.
static image_t* build_back_wall(sint32 u, uint8 wall, uint8 index, bool artificial)
{
	const synth_hex_geometry_t geom = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	const sint32 w = geom.w;
	const sint32 h = geom.h;

	const sint8 h1 = (index == 10) ? 1 : (index == 9 ? 0 : (sint8)(index % 3));
	const sint8 h2 = (index == 10) ? 0 : (index == 9 ? 1 : (sint8)(index / 3));

	// Wall lower-edge endpoints in sprite-local screen space, flat
	// (no per-corner ground lift — the cliff sprite carries h1/h2
	// lift, not the underlying ground slope).
	static const hex_corner_t::type endpoints[back_wall_count][2] = {
		{ hex_corner_t::W,  hex_corner_t::NW }, // wall 0: NW edge
		{ hex_corner_t::NW, hex_corner_t::NE }, // wall 1: N  edge
		{ hex_corner_t::NE, hex_corner_t::E  }, // wall 2: NE edge
	};
	const sint32 ax = geom.vx     [endpoints[wall][0]];
	const sint32 ay = geom.vy_base[endpoints[wall][0]];
	const sint32 bx = geom.vx     [endpoints[wall][1]];
	const sint32 by = geom.vy_base[endpoints[wall][1]];

	const sint32 lift1 = (sint32)h1 * geom.lift;
	const sint32 lift2 = (sint32)h2 * geom.lift;

	PIXVAL* buf = new PIXVAL[w * h];
	memset(buf, 0, w * h * sizeof(PIXVAL));

	if(  lift1 > 0 || lift2 > 0  ) {
		const PIXVAL base = artificial ? CLIFF_FUNDAMENT : CLIFF_NATURAL;
		const PIXVAL face_color = shade_pixval(base, WALL_SHADE[wall]);

		// Cliff polygon: lower edge (a → b), upper edge lifted
		// (b → b - lift2, a - lift1 → a).  Even-odd parity, so
		// winding doesn't matter.
		const sint32 xs[4] = { ax, bx, bx,         ax         };
		const sint32 ys[4] = { ay, by, by - lift2, ay - lift1 };
		fill_polygon(buf, w, h, xs, ys, 4, face_color);

		// `fill_polygon` skips horizontal edges by design (parity
		// correctness), so close them explicitly.  Wall 1's lower edge
		// is horizontal (y=top_y);
		// the upper edge is horizontal whenever lift1 == lift2.
		for(  uint8 i = 0;  i < 4;  i++  ) {
			const uint8 j = (uint8)((i + 1) % 4);
			if(  ys[i] != ys[j]  ) {
				continue;
			}
			const sint32 y = ys[i];
			if(  y < 0 || y >= h  ) {
				continue;
			}
			sint32 x0 = xs[i] < xs[j] ? xs[i] : xs[j];
			sint32 x1 = xs[i] < xs[j] ? xs[j] : xs[i];
			if(  x0 < 0   ) { x0 = 0; }
			if(  x1 >= w  ) { x1 = w - 1; }
			for(  sint32 x = x0;  x <= x1;  x++  ) {
				buf[y * w + x] = face_color;
			}
		}
	}

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
	// trim, see TODO.md) on a clean integer grid.
	const sint32 W = (sint32)gfx->get_base_tile_raster_width();
	const sint32 u = W / 4;
	if(  u < 1  ) {
		dbg->warning("synth_overlay::init", "tile raster width %d too small; synth disabled", W);
		return;
	}

	// Cliff-face sprites are not slope-keyed (they take h1/h2 from
	// the encoded back_image index, not from this tile's overall
	// slope), so build them once per (artificial × wall × index)
	// outside the slope loop.  Index 0 = "no cliff"; we leave it as
	// NULL and rely on the caller's IMG_EMPTY check.
	for(  uint8 art = 0;  art < 2;  art++  ) {
		for(  uint8 wall = 0;  wall < back_wall_count;  wall++  ) {
			for(  uint8 idx = 1;  idx < back_wall_image_count;  idx++  ) {
				image_t* img = build_back_wall(u, wall, idx, art == 1);
				img->register_image();
				back_wall[art][wall][idx] = img;
			}
		}
	}

	initialised = true;
	const auto bbox = synth_hex_geometry(u, TILE_HEIGHT_STEP);
	DBG_DEBUG("synth_overlay::init",
	          "synthesised back-wall sprites (u=%d, bbox %dx%d)",
	          u, bbox.w, bbox.h);
}


image_id get_back_wall(uint8 wall, uint8 index, bool artificial)
{
	if(  !initialised
	  ||  wall >= back_wall_count
	  ||  index == 0
	  ||  index >= back_wall_image_count  ) {
		return IMG_EMPTY;
	}
	const image_t* img = back_wall[artificial ? 1 : 0][wall][index];
	return img != NULL ? img->get_id() : IMG_EMPTY;
}


} // namespace synth_overlay
