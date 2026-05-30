/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_OBJ_DESC_H
#define DESCRIPTOR_OBJ_DESC_H


#include <cstddef>
#include "../simdebug.h"
#include "../simtypes.h"

/// Allocation hooks for the TRACK_DESCRIPTORS teardown (see
/// pakset_manager.cc, which owns the table).  Defining them out of line keeps
/// obj_desc.h free of a pakset_manager dependency; the shipping build links
/// the empty inlines and pays nothing.
#ifdef TRACK_DESCRIPTORS
void obj_desc_track_new(void *ptr);
void obj_desc_track_delete(void *ptr);
#else
inline void obj_desc_track_new(void *) {}
inline void obj_desc_track_delete(void *) {}
#endif

/**
 * Basis of all desc_t classes, which are loaded from the .pak files.
 *
 * Non-polymorphic in the shipping build (tens of thousands of nodes, no
 * vtable wanted).  A TRACK_DESCRIPTORS build makes the destructor virtual so
 * free_all_descriptors can delete each node through its concrete type.
 */
class obj_desc_t
{
	friend class pakset_manager_t;

public:
	obj_desc_t() : children(), nchildren() {}

#ifdef TRACK_DESCRIPTORS
	virtual
#endif
	~obj_desc_t() { delete [] children; }

	void* operator new(size_t size)
	{
		void *p = ::operator new(size);
		obj_desc_track_new(p);
		return p;
	}

	void* operator new(size_t size, size_t extra)
	{
		void *p = ::operator new(size + extra);
		obj_desc_track_new(p);
		return p;
	}

	/*
	* Only support basic delete operator.
	* Prevents C++14 and newer compilers from implicitly adding a sized delete operator.
	* The sized delete operator conflicts with the definiton of the placement new operator.
	*/
	void operator delete(void* ptr)
	{
		obj_desc_track_delete(ptr);
		return ::operator delete(ptr);
	}

protected:
	template<typename T> T const* get_child(int const i) const
	{
		if (static_cast<unsigned>(i) >= nchildren) {
			dbg->fatal("obj_desc_t::get_child", "requested child %d of %u", i, nchildren);
		}
		return static_cast<T const*>(children[i]);
	}

private:
	obj_desc_t** children;
	uint16 nchildren;

	friend class factory_field_group_reader_t;
	friend class obj_reader_t;
};

#endif
