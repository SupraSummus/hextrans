/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_MAP_SHAPE_H
#define DATAOBJ_MAP_SHAPE_H


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
	 * The shape after one 60° rotation step, which takes SE to S and
	 * S to SW: the sides permute (w, h, d) -> (d, w, h).
	 */
	map_shape_t rotated() const { return map_shape_t(d, w, h); }

	/// Where tile @p k of this shape lands in rotated().
	koord rotate_tile(koord k) const
	{
		return koord((sint16)(h + d - 2 - k.y), (sint16)(k.x + k.y - (d - 1)));
	}

	/// Where vertex @p v of this shape lands in rotated().
	hex_vertex_t rotate_vertex(hex_vertex_t v) const
	{
		return { rotate_tile(v.tile), (hex_corner_t::type)((v.corner + 1) % hex_corner_t::count) };
	}
};

#endif
