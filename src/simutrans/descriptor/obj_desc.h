/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_OBJ_DESC_H
#define DESCRIPTOR_OBJ_DESC_H


#include <cstddef>
#include "../simdebug.h"
#include "../simtypes.h"

/**
 * Basis of all desc_t classes, which are loaded from the .pak files.
 * No virtual methods are allowed!
 */
class obj_desc_t
{
	friend class pakset_manager_t;

public:
	obj_desc_t() : children(), nchildren() {}

	~obj_desc_t() { delete [] children; }

	void* operator new(size_t size)
	{
		return ::operator new(size);
	}

	void* operator new(size_t size, size_t extra)
	{
		return ::operator new(size + extra);
	}

	/*
	* Only support basic delete operator.
	* Prevents C++14 and newer compilers from implicitly adding a sized delete operator.
	* The sized delete operator conflicts with the definiton of the placement new operator.
	*/
	void operator delete(void* ptr)
	{
		return ::operator delete(ptr);
	}

protected:
	template<typename T> T const* get_child(unsigned const i) const
	{
		if (i >= nchildren) {
			dbg->fatal("obj_desc_t::get_child", "requested child %u of %u", i, nchildren);
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
