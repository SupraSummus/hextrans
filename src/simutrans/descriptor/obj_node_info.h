/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_OBJ_NODE_INFO_H
#define DESCRIPTOR_OBJ_NODE_INFO_H


#include "../simtypes.h"

// these are the sizes of the divers heades on the disk
// if the uint16 size is LARGE_RECORD_SIZE the actual length is in a following uint32
#define OBJ_NODE_INFO_SIZE (8)
#define EXT_OBJ_NODE_INFO_SIZE (12)
#define LARGE_RECORD_SIZE (0xFFFFu)

// Ceiling on a large-record (uint32) node body, to stop a hostile size near
// 2^32 from driving the per-node allocation into a multi-GB new[].  Generous
// against real nodes (largest in shipped paksets is a few hundred KB).
#define MAX_NODE_BODY_SIZE (64u * 1024u * 1024u)

/**
 * Stored structure of a pak node inside the file.
 */
struct obj_node_info_t {
	uint32  type;
	uint16  nchildren;
	uint32  size;
};

#endif
