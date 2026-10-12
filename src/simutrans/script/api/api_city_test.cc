/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/** @file api_city_test.cc
 *
 * TEST SUPPORT ONLY - not part of the public Script API.
 *
 * Deliberately absent from documentation/DoxyfileSQAPI.in, following the same
 * pattern as api_ai_test.cc.
 *
 * stadt_t::test_and_build_cityroad lays a straight piece of city road, and its
 * one live caller, check_bau_townhall, only ever asks for a strip running
 * along +S or +SE.  The function itself has to take a line along any of the
 * three hex axes in either direction, which nothing in the game reaches yet,
 * so this module gives the tests the caller they lack.  It calls the
 * production function and nothing else.
 */

#include "api.h"

#include "../api_class.h"
#include "../api_function.h"

#include "../../world/simcity.h"

using namespace script_api;


/// city_x, coord3d start, coord3d end -> bool: whether the road was built
static SQInteger citytest_cityroad(HSQUIRRELVM vm)
{
	stadt_t* city = param<stadt_t*>::get(vm, 2);
	if (city == NULL) {
		return sq_raise_error(vm, "citytest_cityroad: no city");
	}
	const koord3d start = param<koord3d>::get(vm, 3);
	const koord3d end = param<koord3d>::get(vm, 4);
	return param<bool>::push(vm, city->test_and_build_cityroad(start.get_2d(), end.get_2d()));
}


void export_city_test(HSQUIRRELVM vm)
{
	register_function(vm, citytest_cityroad, "citytest_cityroad", 4, ".xxx");
}
