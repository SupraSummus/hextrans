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
bool ai_policy_t::trace = false;

const char *const ai_policy_t::feature_names[FEATURES] = {
	"revenue", "distance", "log_distance", "density", "production", "margin", "loads",
	"raw", "stock", "end", "downstream", "served", "water"
};


ai_policy_t::ai_policy_t() :
	bias(0),
	explore(0),
	cash_reserve(0),
	retire_loss(0),
	retire_line(0),
	hq_reserve(1)
{
	for(  int i = 0;  i < FEATURES;  i++  ) {
		weight[i] = 0;
	}
}


double ai_policy_t::score(const double features[FEATURES]) const
{
	double s = bias;
	for(  int i = 0;  i < FEATURES;  i++  ) {
		s += weight[i] * features[i];
	}
	return s;
}


void ai_policy_t::load_goods(const char *filename)
{
	FILE *file = dr_fopen( filename, "r" );
	if(  !file  ) {
		dbg->fatal( "ai_policy_t::load_goods", "cannot read %s", filename );
	}
	ai_policy_t *policy = new ai_policy_t();
	struct { const char *key; double *value; } const fields[] = {
		{ "bias",         &policy->bias },
		{ "explore",      &policy->explore },
		{ "cash_reserve", &policy->cash_reserve },
		{ "retire_loss",  &policy->retire_loss },
		{ "retire_line",  &policy->retire_line },
		{ "hq_reserve",   &policy->hq_reserve },
	};
	char line[256];
	while(  fgets( line, sizeof(line), file )  ) {
		char *end = line + strcspn( line, "#\r\n" );
		*end = 0;
		char *eq = strchr( line, '=' );
		if(  !eq  ) {
			if(  strspn( line, " \t" ) != strlen( line )  ) {
				dbg->fatal( "ai_policy_t::load_goods", "%s: no '=' in \"%s\"", filename, line );
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
		for(  int i = 0;  i < FEATURES  &&  !value;  i++  ) {
			if(  key[0] == 'w'  &&  key[1] == '_'  &&  strcmp( key + 2, feature_names[i] ) == 0  ) {
				value = &policy->weight[i];
			}
		}
		if(  !value  ) {
			dbg->fatal( "ai_policy_t::load_goods", "%s: unknown key \"%s\"", filename, key );
		}
		*value = strtod( eq + 1, NULL );
	}
	fclose( file );
	goods = policy;
}
