/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_READER_IMAGE_READER_H
#define DESCRIPTOR_READER_IMAGE_READER_H


#include "obj_reader.h"
#include "../../tpl/inthashtable_tpl.h"


class image_t;


class image_reader_t : public obj_reader_t
{
	OBJ_READER_DEF(image_reader_t, obj_image, "image");

public:
	/// @copydoc obj_reader_t::read_node
	obj_desc_t *read_node(FILE *fp, obj_node_info_t &node) OVERRIDE;

#ifdef TRACK_DESCRIPTORS
	/// Drop the pixel-checksum dedup cache.  Its entries point at image
	/// descs, so free_all_descriptors must clear it when freeing those descs
	/// to avoid a dangling lookup afterwards.
	static void clear_dedup_cache();
#endif

private:
	bool image_has_valid_data(image_t *img) const;

	/// Identical pixel data is loaded once and shared; keyed by adler32.
	static inthashtable_tpl<uint32, image_t *> images_adlers;
};


#endif
