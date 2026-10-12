/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "api.h"

/** @file api_control.cc script control and debug functions. */

#include "../api_function.h"
#include "../api_class.h"
#include "../script.h"
#include "../../../squirrel/sq_extensions.h"
#include "../../tool/simtool.h"
#include "../../world/simworld.h"
#include "../../dataobj/environment.h"
#include "../../dataobj/settings.h"
#include "../api_param.h"

namespace script_api {

	bool pause_game()
	{
		tool_pause_t t;
		if (!t.is_selected()) {
			t.init(NULL);
		}
		return t.is_selected();
	}

	bool is_game_paused()
	{
		tool_pause_t t;
		return t.is_selected();
	}

	bool rotate_map()
	{
		if (env_t::networkmode  ||  !welt->can_rotate90(3)) {
			return false;
		}
		tool_rotate90_t t;
		t.init(NULL);
		return true;
	}
};

using namespace script_api;

/**
 * Grows the map to W x H with the same D, as the enlarge-map dialog does,
 * and adds up to @p cities new cities in the new part.
 * Returns the shift of the script's coordinates: the new part lies beyond
 * the far sides of the map as it is, so the old tiles keep their
 * coordinates, unless the map is turned by 180 degrees, where the script's
 * frame has the new part at its near sides.  Null when the map cannot grow
 * to that size or the game is a network game.
 */
SQInteger debug_enlarge_map(HSQUIRRELVM vm)
{
	const sint32 w = param<sint32>::get(vm, 2);
	const sint32 h = param<sint32>::get(vm, 3);
	const sint32 cities = param<sint32>::get(vm, 4);

	const map_shape_t old_shape = welt->get_map_shape();
	const map_shape_t new_shape(w, h, old_shape.d);
	if (env_t::networkmode  ||  w < old_shape.w  ||  h < old_shape.h  ||  cities < 0
		||  new_shape.get_size_q() > 32766  ||  new_shape.get_size_r() > 32766) {
		sq_pushnull(vm);
		return 1;
	}

	settings_t sets(welt->get_settings());
	sets.set_size(w, h);
	sets.set_city_count(cities);
	welt->enlarge_map(&sets, NULL);

	const bool turned = coordinate_transform_t::get_rotation() != 0;
	return push_instance(vm, "coord",
		turned ? new_shape.get_size_q() - old_shape.get_size_q() : 0,
		turned ? new_shape.get_size_r() - old_shape.get_size_r() : 0);
}

SQInteger sleep(HSQUIRRELVM vm)
{
	if (const char* blocker = sq_get_suspend_blocker(vm)) {
		return sq_raise_error(vm, "Cannot call sleep from within `%s'.", blocker);
	}
	return sq_suspendvm(vm);
}

SQInteger set_pause_on_error(HSQUIRRELVM vm)
{
	if (script_vm_t *script = (script_vm_t*)sq_getforeignptr(vm)) {
		bool poe = param<bool>::get(vm, 2);
		script->pause_on_error = poe;
	}
	return SQ_OK;
}



void export_control(HSQUIRRELVM vm)
{
	// in global scope

	/**
	 * Suspends execution of the script. Upon resume, the script
	 * will have full count of opcodes to spent for fragile code,
	 * i.e. writing into @ref persistent.
	 * @typemask void()
	 */
	register_function(vm, sleep, "sleep", 1, ".");

	/**
	 * @returns total amount of opcodes executed by vm
	 */
	register_function<int(*)()>(vm, sq_get_ops_total, "get_ops_total");

	/**
	 * @returns amount of remaining opcodes until vm will be suspended
	 */
	register_function<int(*)()>(vm, sq_get_ops_remaing, "get_ops_remaining");


	/**
	 * Table with methods to pause the game from a script.
	 * @note None of these work in network games.
	 */
	begin_class(vm, "debug");
	/**
	 * Pauses game. Does not work in network games. Use with care.
	 * @returns true when successful.
	 */
	STATIC register_method(vm, &pause_game, "pause", false, true);
	/**
	 * Checks whether game is paused.
	 * Note that scripts only run for unpaused games.
	 * This can only be used in functions that are called by user actions (e.g., is_*_allowed).
	 * @returns true when successful.
	 */
	STATIC register_method(vm, &is_game_paused, "is_paused", false, true);

	/**
	 * Rotates the map by 180 degrees, as the rotate-map tool does.
	 * The script keeps seeing the map as before: coordinates, directions
	 * and slopes are translated (see coord).
	 * Does not work in network games.
	 * @returns true when the map was rotated.
	 */
	STATIC register_method(vm, &rotate_map, "rotate_map", false, true);

	/**
	 * Enlarges the map to @p w x @p h with the same depth D, as the
	 * enlarge-map dialog does, and adds up to @p cities cities in the new
	 * part.  The old hexagon keeps its tiles; see karte_t::enlarge_map.
	 * Does not work in network games.
	 * @param w new side W, at least the current one
	 * @param h new side H, at least the current one
	 * @param cities number of cities to add
	 * @returns the coord by which the script's coordinates of the old tiles
	 *          moved, or null when the map was not enlarged.
	 * @typemask coord(integer,integer,integer)
	 */
	STATIC register_function(vm, debug_enlarge_map, "enlarge_map", 4, ".iii");

	/**
	 * Scripts can pause the game in case of error. Toggle this behavior by parameter @p p.
	 * Does not work in network games. Use with care.
	 * @param p true if script should make the game pause in case of error
	 */
	STATIC register_function<void(*)(bool)>(vm, set_pause_on_error, "set_pause_on_error", true);

	end_class(vm);
}
