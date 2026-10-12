/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef WORLD_PLACEFINDER_H
#define WORLD_PLACEFINDER_H


#include "../dataobj/koord.h"
#include "pos_list.h"
class karte_t;

class placefinder_t {
protected:
	karte_t *welt;
	sint16 w;
	sint16 h;
	sint16 max_radius;

	virtual bool is_area_ok(koord pos, sint16 w, sint16 h, climate_bits cl) const;

	virtual bool is_tile_ok(koord pos, koord d, climate_bits cl) const;

	bool is_boundary_tile(koord d) const;

	placefinder_t(karte_t *welt, sint16 _max_radius = - 1) { this->welt = welt; max_radius = _max_radius; }
	virtual ~placefinder_t() {}
public:
	koord find_place(koord start, sint16 w, sint16 h, climate_bits cl, bool *r = NULL);
};

#endif
