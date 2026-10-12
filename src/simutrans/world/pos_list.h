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
 * Extended version of pos_list_t. Provides surrounding positions
 * for a range of size h by w.
 * (0, 0) is used as the end marker again.
 */
class pos_list_wh_t : public pos_list_t {
	sint16 w;
	sint16 h;

	sint16 dx;
	sint16 dy;
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
		dx = dy = 0;
		pos_list_t::restart();
	}
	void restart() { pos_list_t::restart(); }

	bool get_next_pos(koord &k) OVERRIDE
	{
		get_pos(k);

		if(k.x == 0 && k.y == 0 && (dx > 0 || dy > 0)) {
			if(dx > 0) {
				dx--;
			}
			else if(dy > 0) {
				dy--;
				dx = w - 1;
			}
			k.x -= dx;
			k.y -= dy;
		}
		else if(dx > 0) {
			k.x -= --dx;
			if(k.y <= 0) {
				k.y -= h - 1;
			}
		}
		else if(dy > 0) {
			k.y -= --dy;
			if(k.x <= 0) {
				k.x -= w - 1;
			}
		}
		else {
			if(pos_list_t::get_next_pos(k)) {
				if(k.y == 0) {
					dy = h - 1;
				}
				if(k.x == 0) {
					dx = w - 1;
				}
				if(k.y <= 0) {
					k.y -= h - 1;
				}
				if(k.x <= 0) {
					k.x -= w - 1;
				}
			}
			else {
				return false;
			}
		}
		return true;
	}
};

#endif
