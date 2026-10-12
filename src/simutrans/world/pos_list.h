/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef WORLD_POS_LIST_H
#define WORLD_POS_LIST_H


#include "../dataobj/koord.h"

/**
 * pos_list_t:
 *
 * Provides a list of all coordinates in the hex disc of radius max_xy around
 * (0, 0), sorted by physical distance (physical_distance_sq in koord.h) in
 * increasing order.  (0, 0) comes first, and is used as the end marker again.
 *
 * The square-grid version sorted the tiles of the (-max_xy .. max_xy)^2
 * square by x^2 + y^2.  In axial coordinates that is an ellipse stretched
 * along the (1, 1) diagonal, and the square's corners lie twice as far out as
 * its sides.
 *
 * Header-only over koord.h, so tools/hex_neighbourhood_test compiles it.
 */
class pos_list_t {
	sint16 max_radius;
	sint32 *columns;
	sint16 radius;
	sint16 row;
	sint16 quadrant;

	/* The tiles are walked as (row, col) = (|q|, |2r + q|), which has the same
	 * parity in both and puts the physical distance q^2 + q*r + r^2 at
	 * (3*row^2 + col^2) / 4: symmetric under both sign flips, as x^2 + y^2 was,
	 * so the four quadrants of the square-grid walk carry over.  Their tiles
	 * are the images of (row, col) under the 2 reflections of the hex lattice. */
	static sint64 sort_key(sint32 row, sint32 col) { return 3 * (sint64)row * row + (sint64)col * col; }

	sint16 find_best_row()
	{
		sint64 best_dist = -1;
		sint16 best_row = -1;

		for(sint16 i = 0; i < radius; i++) {
			// still inside the disc: |q|, |r|, |q + r| <= max_xy
			if(columns[i] + i < 2 * (sint32)max_radius - 1) {
				const sint64 dist = sort_key(i, columns[i]);

				if(best_dist == -1 || dist < best_dist) {
					best_dist = dist;
					best_row = i;
				}
			}
		}
		if(radius < max_radius && best_dist > sort_key(radius, radius & 1)) {
			return -1;
		} else {
			return best_row;
		}
	}

public:
	/**
	* @param max_xy (Maximum distance in hex steps)
	*/
	pos_list_t(sint16 max_xy)
	{
		this->max_radius = max_xy + 1;
		columns = new sint32[this->max_radius];

		restart();
	}

	virtual ~pos_list_t()
	{
		delete [] columns;
	}

	void restart()
	{
		radius = 1;
		columns[0] = 0;
		row = 0;
		quadrant = 0;
	}

	bool get_pos(koord &k)
	{
		if(row != -1) {
			const sint32 col = columns[row];

			switch(quadrant) {
				case 1:
					k = koord(row, (sint16)((col - row) / 2));
					return true;
				case 2:
					k = koord(row, (sint16)((-col - row) / 2));
					return true;
				case 3:
					k = koord((sint16)-row, (sint16)((col + row) / 2));
					return true;
				case 4:
					k = koord((sint16)-row, (sint16)((row - col) / 2));
					return true;
			}
		}
		return false;
	}

	virtual bool get_next_pos(koord &k)
	{
		if(row != -1) {
			if(quadrant++ == 4) {
				quadrant = 1;

				columns[row] += 2;

				row = find_best_row();
				if(row == -1) {
					if(radius < max_radius) {
						columns[radius] = radius & 1;
						radius++;
					}
					row = find_best_row();
				}
			}
		}
		if(row != -1) {
			if(quadrant == 1 && !row) {
				quadrant += 2; // skip second 0, +/-y
			}
			if(quadrant % 2 == 1 && !columns[row]) {
				quadrant ++; // skip second +/-x, 0
			}
			return get_pos(k);
		}
		return false;
	}
};


/**
 * pos_list_wh_t:
 *
 * Extended version of pos_list_t. Provides the anchors (the tile with the
 * smallest q and r) of the w by h boxes around the origin, closest first: the
 * physical distance to the nearest tile of the box does not decrease.  The
 * boxes are those whose nearest tile is in the hex disc of radius max_radius,
 * once each; the w * h boxes that contain the origin come first.
 *
 * The square-grid version took the corner of the box nearest the origin along
 * each axis for its nearest tile.  That is the nearest tile in hex steps, but
 * in the map plane only when q and r have the same sign: for q > 0 > r the
 * distance q^2 + q*r + r^2 falls with q up to |r| / 2, so a box at
 * (2..4, -6) is nearest at (3, -6), not at (2, -6).
 *
 * Every box belongs to its nearest tile p, the smallest (q, r) of the tiles
 * at the same distance, which pos_list_t hands out closest first.  p is the
 * nearest tile of the box exactly when no neighbour of p nearer the origin
 * (or as near with a smaller (q, r)) is in it: the distance is a sum of convex
 * functions of q, r and q + r, so a tile with no nearer neighbour in the box is
 * nearer than all of them.  The boxes of p are the offsets (i, j) of p from
 * the anchor with that property.
 */
class pos_list_wh_t : public pos_list_t {
	sint16 w;
	sint16 h;

	koord p;          // the tile that the boxes below have as their nearest
	uint8 nearer;     // bit n: neighbours[n] of p is nearer the origin
	sint16 i_lo, i_hi, j_lo, j_hi; // offsets of p in the box, bounded by the neighbours on an axis
	sint16 i, j;      // the next offset to try, j > j_hi when p has no more

	void exhaust()
	{
		i = 0;
		j = 1;
		j_hi = 0;
	}

	// whether neighbours[n] of p is nearer the origin than p, or as near and
	// first in (q, r) order
	static bool is_nearer(koord t, koord p)
	{
		const uint32 dt = physical_distance_sq(t, koord(0, 0));
		const uint32 dp = physical_distance_sq(p, koord(0, 0));
		return dt < dp  ||  (dt == dp  &&  (t.x < p.x  ||  (t.x == p.x  &&  t.y < p.y)));
	}

	bool next_tile()
	{
		if(!pos_list_t::get_next_pos(p)) {
			return false;
		}
		nearer = 0;
		for(uint8 n = 0; n < 6; n++) {
			if(is_nearer(p + koord::neighbours[n], p)) {
				nearer |= 1u << n;
			}
		}
		// a nearer SE, NW, S or N neighbour in the box leaves one column or row
		// for p: koord::neighbours[] is SE, S, SW, NW, N, NE
		i_lo = (nearer & (1u << 0)) ? w - 1 : 0;
		i_hi = (nearer & (1u << 3)) ? 0 : w - 1;
		j_lo = (nearer & (1u << 1)) ? h - 1 : 0;
		j_hi = (nearer & (1u << 4)) ? 0 : h - 1;
		i = i_lo;
		j = j_lo;
		if(i_lo > i_hi) {
			j = j_hi + 1;
		}
		return true;
	}

	bool is_nearest_tile(sint16 oi, sint16 oj) const
	{
		for(uint8 n = 0; n < 6; n++) {
			if(nearer & (1u << n)) {
				const sint16 x = oi + koord::neighbours[n].x;
				const sint16 y = oj + koord::neighbours[n].y;
				if(x >= 0  &&  x < w  &&  y >= 0  &&  y < h) {
					return false;
				}
			}
		}
		return true;
	}

public:
	pos_list_wh_t(sint16 max_radius, sint16 w, sint16 h) :
		pos_list_t(max_radius)
	{
		restart(w, h);
	}

	void restart(sint16 w, sint16 h)
	{
		this->w = w;
		this->h = h;
		restart();
	}
	void restart()
	{
		pos_list_t::restart();
		exhaust();
	}

	bool get_next_pos(koord &k) OVERRIDE
	{
		for(;;) {
			while(j <= j_hi) {
				const sint16 oi = i, oj = j;
				if(++i > i_hi) {
					i = i_lo;
					j++;
				}
				if(is_nearest_tile(oi, oj)) {
					k = koord(p.x - oi, p.y - oj);
					return true;
				}
			}
			if(!next_tile()) {
				return false;
			}
		}
	}
};

#endif
