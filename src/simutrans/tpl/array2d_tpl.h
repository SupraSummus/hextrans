/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef TPL_ARRAY2D_TPL_H
#define TPL_ARRAY2D_TPL_H


#include <stdio.h>
#include <string.h>

#include "../dataobj/koord.h"
#include "../dataobj/map_shape.h"
#include "../simdebug.h"

/**
 * A template class for bounds checked 2-dimensional arrays.
 * This is kept as simple as possible. Does not use exceptions
 * for error handling.
 */
template <class T>
class array2d_tpl
{
private:
	T* data;
	unsigned w, h;

public:

	array2d_tpl(unsigned _w, unsigned _h) : w(_w), h(_h) {
		data = new T[w*h];
	}

	array2d_tpl(const array2d_tpl <T> &other) {
		w = other.w;
		h = other.h;
		data = new T[w*h];
		memcpy(data, other.data, sizeof(T)*w*h);
	}

	~array2d_tpl() {
		delete [] data;
	}

	unsigned get_width() const {
		return w;
	}

	unsigned get_height() const {
		return h;
	}

	void clear()
	{
		delete [] data;
		data = 0;
		w = h = 0;
	}

	void init( T value )
	{
		if(sizeof(T)==1) {
			memset( data, value, w*h );
		}
		else {
			unsigned i=(w*h);
			while(  i>0  ) {
				data[--i] = value;
			}
		}
	}

	// all informations in the array are lost
	void resize(unsigned resize_x, unsigned resize_y )
	{
		if( w*h != resize_x*resize_y  ) {
			T* new_data = new T[resize_x*resize_y];
			delete [] data;
			data = new_data;
		}
		w = resize_x;
		h = resize_y;
	}

	/**
	 * One 60° step of the map @p from this array holds a cell per tile
	 * of, sized to its bounding rhombus: the cell of each tile moves to
	 * from.rotate_tile, and the array becomes the bounding rhombus of
	 * from.rotated().  Cells that are no tile of the rotated map are T().
	 */
	void rotate90( const map_shape_t &from )
	{
		if(  w*h > 0  ) {
			const map_shape_t to = from.rotated();
			const unsigned new_w = (unsigned)to.get_size_q();
			const unsigned new_h = (unsigned)to.get_size_r();
			T *new_data = new T[new_w*new_h];
			for(  unsigned i = 0;  i < new_w*new_h;  i++  ) {
				new_data[i] = T();
			}
			for(  unsigned y=0;  y<h;  y++  ) {
				for(  unsigned x=0;  x<w;  x++  ) {
					const koord k0( (sint16)x, (sint16)y );
					if(  from.contains( k0 )  ) {
						const koord k = from.rotate_tile( k0 );
						new_data[k.x + k.y*new_w] = data[x+(y*w)];
					}
				}
			}
			delete [] data;
			data = new_data;
			w = new_w;
			h = new_h;
		}
	}

	// keep old information, new cell get default
	void resize(unsigned resize_x, unsigned resize_y, T default_value )
	{
		if( w*h != resize_x*resize_y  ) {
			T* new_data = new T[resize_x*resize_y];
			for( uint y = 0; y < resize_y; y++ ) {
				for( uint x = 0; x < resize_x; x++ ) {
					if( x < w   &&  y < h  ) {
						new_data[ x + y*resize_x ] = data[x+y*w];
					}
					else {
						new_data[ x + y*resize_x ] = default_value;
					}
				}
			}
			delete [] data;
			data = new_data;
		}
		w = resize_x;
		h = resize_y;
	}

	T& at(unsigned x, unsigned y)
	{
		if(  (int)((w-x)|(h-y))<0  ) {
			dbg->fatal("array2d_tpl<T>::at()","index out of bounds: (%d,%d) not in (0..%d, 0..%d)", x, y, w-1, h-1);
		}
		return data[y*w + x];
	}

	T& at(koord k)
	{
		return at((unsigned)k.x, (unsigned)k.y);
	}

	/*
	 * use this with care, you'll lose all checks!
	 */
	const T* to_array() const
	{
		return data;
	}

	array2d_tpl<T> & operator = (const array2d_tpl <T> &other)
	{
		if(  this != &other  ) {// protect against invalid self-assignment
			if(  h != other.h  &&  w != other.w  ) {
				if(  h*w!=0  ) {
					dbg->error("array2d_tpl<T>::=()","source has different size!");
				}
			}
			delete [] data;
			w = other.w;
			h = other.h;
			data = new T[w*h];
			memcpy(data, other.data, sizeof(T)*w*h);
		}
		return *this;
	}
};


#endif
