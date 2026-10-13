/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../simdebug.h"
#include "../sys/simsys.h"

#include "ai_policy.h"


const ai_policy_t *ai_policy_t::goods = NULL;
const ai_policy_t *ai_policy_t::passengers = NULL;
bool ai_policy_t::trace = false;

const char *const ai_policy_t::feature_names[FEATURES] = {
	"revenue", "distance", "log_distance", "density", "production", "margin", "loads",
	"raw", "stock", "end", "downstream", "served", "water"
};

const char *const ai_policy_t::passenger_feature_names[PASSENGER_FEATURES] = {
	"population_from", "population_to", "distance", "log_distance", "new_hubs", "across_water"
};


ai_policy_t::ai_policy_t(const char *const *names, int count) :
	names(names),
	count(count),
	bias(0),
	explore(0),
	cash_reserve(0),
	retire_loss(0),
	retire_line(0),
	hq_reserve(1),
	city_stops(6)
{
	for(  int i = 0;  i < FEATURES;  i++  ) {
		weight[i] = 0;
	}
}


double ai_policy_t::score(const double *features) const
{
	double s = bias;
	for(  int i = 0;  i < count;  i++  ) {
		s += weight[i] * features[i];
	}
	return s;
}


const ai_policy_t *ai_policy_t::load(const char *filename, const char *const *names, int count)
{
	FILE *file = dr_fopen( filename, "r" );
	if(  !file  ) {
		dbg->fatal( "ai_policy_t::load", "cannot read %s", filename );
	}
	ai_policy_t *policy = new ai_policy_t( names, count );
	struct { const char *key; double *value; } const fields[] = {
		{ "bias",         &policy->bias },
		{ "explore",      &policy->explore },
		{ "cash_reserve", &policy->cash_reserve },
		{ "retire_loss",  &policy->retire_loss },
		{ "retire_line",  &policy->retire_line },
		{ "hq_reserve",   &policy->hq_reserve },
		{ "city_stops",   &policy->city_stops },
	};
	char line[256];
	while(  fgets( line, sizeof(line), file )  ) {
		char *end = line + strcspn( line, "#\r\n" );
		*end = 0;
		char *eq = strchr( line, '=' );
		if(  !eq  ) {
			if(  strspn( line, " \t" ) != strlen( line )  ) {
				dbg->fatal( "ai_policy_t::load", "%s: no '=' in \"%s\"", filename, line );
			}
			continue;
		}
		*eq = 0;
		char *key = line + strspn( line, " \t" );
		for(  char *k = eq;  k > key  &&  (k[-1] == ' '  ||  k[-1] == '\t');  k--  ) {
			k[-1] = 0;
		}
		double *value = NULL;
		for(  const auto &field : fields  ) {
			if(  strcmp( key, field.key ) == 0  ) {
				value = field.value;
			}
		}
		for(  int i = 0;  i < count  &&  !value;  i++  ) {
			if(  key[0] == 'w'  &&  key[1] == '_'  &&  strcmp( key + 2, names[i] ) == 0  ) {
				value = &policy->weight[i];
			}
		}
		if(  !value  ) {
			dbg->fatal( "ai_policy_t::load", "%s: unknown key \"%s\"", filename, key );
		}
		*value = strtod( eq + 1, NULL );
	}
	fclose( file );
	return policy;
}
