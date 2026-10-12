/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_MAP_SHAPE_H
#define DATAOBJ_MAP_SHAPE_H


#include <cmath>

#include "koord.h"


/**
 * The playable world: a centrally symmetric hexagon whose opposite
 * sides are w, h and d tiles long, corner tiles included.  The w sides
 * run along +q (SE-NW), the h sides along +r (N-S), the d sides along
 * the NE-SW axis.  See documentation/map-shape.md.
 *
 * It is the get_size_q() x get_size_r() axial rhombus with its two 60°
 * corners, (0, 0) and (size_q-1, size_r-1), cut off along the NE-SW
 * axis.  d == 1 cuts nothing: the plain w x h rhombus.
 *
 * Header-only over koord.h, so tools/hex_map_shape_test compiles it.
 */
struct map_shape_t
{
	sint32 w, h, d;

	map_shape_t(sint32 w_, sint32 h_, sint32 d_) : w(w_), h(h_), d(d_) {}

	bool is_valid() const { return w >= 1  &&  h >= 1  &&  d >= 1; }

	/// Extent of the bounding rhombus along q and r.
	sint32 get_size_q() const { return w + d - 1; }
	sint32 get_size_r() const { return h + d - 1; }

	/// On-map tiles satisfy cut_lo <= q + r <= cut_hi.
	sint32 get_cut_lo() const { return d - 1; }
	sint32 get_cut_hi() const { return get_size_q() + get_size_r() - 2 - get_cut_lo(); }

	bool contains(sint32 q, sint32 r) const
	{
		return q >= 0  &&  r >= 0  &&  q < get_size_q()  &&  r < get_size_r()
			&&  q + r >= get_cut_lo()  &&  q + r <= get_cut_hi();
	}

	bool contains(koord k) const { return contains(k.x, k.y); }

	/**
	 * The on-map tiles of row @p r (0 <= r < get_size_r()) are
	 * get_q_begin(r) <= q < get_q_end(r); the range is never empty.
	 */
	sint32 get_q_begin(sint32 r) const
	{
		const sint32 q = get_cut_lo() - r;
		return q > 0 ? q : 0;
	}

	sint32 get_q_end(sint32 r) const
	{
		const sint32 q = get_cut_hi() - r + 1;
		return q < get_size_q() ? q : get_size_q();
	}

	/**
	 * Hex steps from @p k to the hexagon's boundary: 0 on an edge tile,
	 * negative off the map.  The minimum over the six half-planes, each
	 * of which a hex step changes by at most one, so a tile at edge
	 * distance n has the whole hex disc of radius n on the map.
	 */
	sint32 edge_distance(koord k) const
	{
		return edge_distance_cached(k.x, k.y, get_size_q() - 1, get_size_r() - 1, get_cut_lo(), get_cut_hi());
	}

	/// edge_distance() with the extents of contains_cached().
	static sint32 edge_distance_cached(sint32 q, sint32 r, sint32 max_q, sint32 max_r, sint32 cut_lo, sint32 cut_hi)
	{
		sint32 dist = q;
		const sint32 others[5] = { r, max_q - q, max_r - r, q + r - cut_lo, cut_hi - q - r };
		for(  int i = 0;  i < 5;  i++  ) {
			if(  others[i] < dist  ) {
				dist = others[i];
			}
		}
		return dist;
	}

	/**
	 * contains() without branches, in the cached form
	 * surface_t::is_within_limits uses: @p max_q = get_size_q()-1,
	 * @p max_r = get_size_r()-1.  Each term is negative exactly when
	 * its half-plane is violated, so the bitwise or is negative when
	 * any one is.
	 */
	static bool contains_cached(sint32 q, sint32 r, sint32 max_q, sint32 max_r, sint32 cut_lo, sint32 cut_hi)
	{
		return (q | r | (max_q - q) | (max_r - r) | (q + r - cut_lo) | (cut_hi - q - r)) >= 0;
	}

	/// A world vertex is on-map when any of its three owning tiles is.
	bool contains_vertex(hex_vertex_t v) const
	{
		hex_vertex_t owners[3];
		vertex_owners(v.tile, v.corner, owners);
		return contains(owners[0].tile)  ||  contains(owners[1].tile)  ||  contains(owners[2].tile);
	}

	sint64 get_tile_count() const
	{
		const sint64 t = get_cut_lo();
		return (sint64)get_size_q() * get_size_r() - t * (t + 1);
	}

	/**
	 * The side of the square map with as many tiles: the scale the
	 * new-world dialog's city, factory, attraction and river densities
	 * follow.  sqrt(w * h) at d == 1, bit for bit.
	 */
	double get_side_equivalent() const { return std::sqrt((double)get_tile_count()); }

	/**
	 * The relief image frame of the map preview and of an imported
	 * heightfield: the physical plane in offset coordinates.  Column
	 * i = q, row j = r + floor((q - t) / 2) with t = d - 1.  A tile
	 * centre sits at x = 1.5 q, so columns are exact, and the columns
	 * with q - t odd fall half a row down, as on screen.  Pixels whose
	 * tile is off the map are outside the hexagon.
	 */
	sint32 get_image_cols() const { return get_size_q(); }

	/// Rows for the lower half of the last odd column included.
	sint32 get_image_rows() const { return h + get_size_q() / 2; }

	/// Image row of tile (q, r).  The shift is floor((q - t) / 2); >> 1 floors a negative q - t too.
	sint32 get_image_row(sint32 q, sint32 r) const { return r + ((q - get_cut_lo()) >> 1); }

	/// Tile row r of image pixel (i, j), q = i: contains(i, get_tile_row(i, j)) tells whether the pixel is on-map.
	sint32 get_tile_row(sint32 i, sint32 j) const { return j - ((i - get_cut_lo()) >> 1); }

	/**
	 * The shape of a relief image of @p cols x @p rows pixels for a given
	 * @p d: w follows from the columns, h from the rows.  Not valid()
	 * when either would be below 1.
	 */
	static map_shape_t from_image_size(sint32 cols, sint32 rows, sint32 d)
	{
		return map_shape_t(cols - d + 1, rows - cols / 2, d);
	}

	/**
	 * Whether generation fills the canonical vertex slots (E and SE) of
	 * tile (@p q, @p r), one of the slot range [-1, W'-1] x [-1, H']
	 * (documentation/hex-vertex-storage.md): the slot lies within one
	 * vertex step of an on-map vertex, so the smoothing of the relief
	 * reads what it did for the rhombus.  At d == 1 that is the whole
	 * range; with d > 1 it skips the middle of the cut corners.
	 */
	bool slot_near_map(sint32 q, sint32 r) const
	{
		return slot_near_map_cached(q, r, get_cut_lo(), get_cut_hi());
	}

	/// slot_near_map() with the cut of surface_t::cached_cut_lo/hi.
	static bool slot_near_map_cached(sint32 q, sint32 r, sint32 cut_lo, sint32 cut_hi)
	{
		return q + r >= cut_lo - 2  &&  q + r <= cut_hi + 1;
	}

	/**
	 * Upper bounds of hex_vertex_pos() over the slots slot_near_map()
	 * accepts, which size the noise cache of init_perlin_map: x is
	 * 1.5 W' (the exact maximum is 1.5 (W' - 1) + 1) and y is
	 * sqrt(3) (H' + (W' - t) / 2), the exact maximum.
	 */
	hex_pos_t get_vertex_pos_bound() const
	{
		return { 1.5 * get_size_q(), 1.7320508075688772 * (get_size_r() + 0.5 * (get_size_q() - get_cut_lo())) };
	}

	/**
	 * The shape after one 60° rotation step, which takes SE to S and
	 * S to SW: the sides permute (w, h, d) -> (d, w, h).
	 */
	map_shape_t rotated() const { return map_shape_t(d, w, h); }

	/**
	 * Where tile @p k of this shape lands in rotated(): hex_axial_rotate
	 * about the origin, then the shift that moves the rotated hexagon's
	 * corner back to (0, 0).
	 */
	koord rotate_tile(koord k) const
	{
		return hex_axial_rotate(k, 1) + rotate_shift();
	}

	/// Where vertex @p v of this shape lands in rotated().
	hex_vertex_t rotate_vertex(hex_vertex_t v) const
	{
		hex_vertex_t r = hex_axial_rotate(v, 1);
		r.tile += rotate_shift();
		return r;
	}

	/// The shape after @p steps steps (mod 6, also negative).
	map_shape_t rotated(int steps) const
	{
		map_shape_t s = *this;
		for(  int i = ((steps % 6) + 6) % 6;  i > 0;  i--  ) {
			s = s.rotated();
		}
		return s;
	}

	/**
	 * Where tile @p k of this shape lands after @p steps steps
	 * (mod 6, also negative: -1 maps a tile of rotated() back here).
	 * Three steps are the point reflection
	 * (q, r) -> (size_q-1-q, size_r-1-r) onto the same shape.
	 */
	koord rotate_tile(koord k, int steps) const
	{
		map_shape_t s = *this;
		for(  int i = ((steps % 6) + 6) % 6;  i > 0;  i--  ) {
			k = s.rotate_tile(k);
			s = s.rotated();
		}
		return k;
	}

	/// rotate_tile for a vertex: corner c becomes c + steps.
	hex_vertex_t rotate_vertex(hex_vertex_t v, int steps) const
	{
		map_shape_t s = *this;
		for(  int i = ((steps % 6) + 6) % 6;  i > 0;  i--  ) {
			v = s.rotate_vertex(v);
			s = s.rotated();
		}
		return v;
	}

private:
	koord rotate_shift() const { return koord((sint16)(h + d - 2), (sint16)(1 - d)); }
};

#endif
