/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 *
 * nettool-side definitions of symbols the shared utils/ TUs reach
 * into.  Game-side counterparts live in log_fatal_display.cc and
 * (for env_t) the in-game `karte_t` initialisation; nettool gets
 * minimal defaults here.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../simutrans/dataobj/environment.h"
#include "../simutrans/utils/log.h"


log_t::level_t env_t::verbose_debug = log_t::LEVEL_FATAL;
bool env_t::pakset_debug = false;


FILE *dr_fopen(const char *filename, const char *mode) { return fopen(filename, mode); }


void log_t_platform_fatal_exit(char *buffer)
{
	puts(buffer);
	exit(1);
}
