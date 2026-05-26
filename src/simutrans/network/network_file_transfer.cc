/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "network_file_transfer.h"
#include "../simdebug.h"
#include "../simloadingscreen.h"
#include "../sys/simsys.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "../utils/cbuffer.h"

#include "../dataobj/translator.h"
#include "../simversion.h"


/*
 * Functions required by both Simutrans and Nettool
 */


sint32 parse_content_length(const char *content_length_str)
{
	char *endp = NULL;
	errno = 0;
	const long parsed = strtol(content_length_str, &endp, 10);
	if (endp == content_length_str || errno != 0 || parsed < 0 || parsed > INT32_MAX) {
		return -1;
	}

	return (sint32)parsed;
}


const char *network_receive_file(const SOCKET src_sock, const char *const save_as, sint32 const length, sint32 const timeout )
{
	// ok, we have a socket to connect
	dr_remove(save_as);

	DBG_MESSAGE("network_receive_file", "File size %i", length );

	if(length>0) {
		loadingscreen_t ls(translator::translate("Downloading"),length,true,true);

		// good place to show a progress bar
		char rbuf[4096];
		sint32 length_read = 0;
		if (FILE* const f = dr_fopen(save_as, "wb")) {
			while(length_read < length) {
				if(  timeout > 0  ) {
					/** 10s for 4096 bytes:
					 * As long as you are not connected with less than 1200 Baud that should be fine
					 * otherwise upgrade your acoustic coupler to 56k ...
					 */
					fd_set fds;
					FD_ZERO(&fds);
					FD_SET(src_sock,&fds);
					struct timeval tv; // 10 s timeout
					tv.tv_sec = 10000 / 1000;
					tv.tv_usec = (10000 % 1000) * 1000ul;
					// can we read?
					if(  select( FD_SETSIZE, &fds, NULL, NULL, &tv )!=1  ) {
						dbg->warning("network_receive_file", "Timeout during transfer: %s", strerror(errno) );
						break;
					}
				}
				// ok, now here should be something new to read
				int i = recv(src_sock, rbuf, length_read + 4096 < length ? 4096 : length - length_read, 0);
				if (i > 0) {
					fwrite(rbuf, 1, i, f);
					length_read += i;
					ls.set_progress(length_read);
				}
				else {
					if (i < 0) {
						dbg->warning("network_receive_file", "recv failed with %i", i);
					}
					break;
				}
			}
			fclose(f);
		}
		if(  length_read<length  ) {
			return "Not enough bytes transferred";
		}
	}
	return NULL;
}
