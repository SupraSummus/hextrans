/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef FUZZ_FUZZ_ENGINE_H
#define FUZZ_FUZZ_ENGINE_H


/*
 * Engine standup for the harnesses that need a real world
 * (fuzz_command, fuzz_savegame): a silent logger, the null renderer,
 * translations, the pakset and the tool menus, in simu_main() order.
 *
 * Needs a pakset and base files on disk: SIMUTRANS_FUZZ_BASE is the data
 * dir holding config/ and text/ (default "simutrans", absolute for -fork
 * mode), SIMUTRANS_FUZZ_PAK the pakset dir under it (default "pak"; run
 * tools/test.py once to fetch pak64).  Fatals if either is unusable.
 */
void fuzz_standup_engine();


#endif
