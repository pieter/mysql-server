/* mySTL helpers.hpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


/* mySTL helpers implements misc constructs for vector and list
 *
 */

#ifndef mySTL_HELPERS_HPP
#define mySTL_HELPERS_HPP

#include <stdlib.h>
#include <new>        // placement new



#ifdef __IBMCPP__
/*
      Workaround for the lack of operator new(size_t, void*)
      in IBM VA C++ 6.0
*/
    struct Dummy {};

    inline void* operator new(size_t size, Dummy* d) 
    { 
        return static_cast<void*>(d);
    }

    typedef Dummy* yassl_pointer;
#else
    typedef void*  yassl_pointer;
#endif


namespace mySTL {


template <typename T, typename T2>
inline void construct(T* p, const T2& value)
{
    new (reinterpret_cast<yassl_pointer>(p)) T(value);
}


template <typename T>
inline void construct(T* p)
{
    new (reinterpret_cast<yassl_pointer>(p)) T();
}


template <typename T>
inline void destroy(T* p)
{
    p->~T();
}


template <typename Iter>
void destroy(Iter first, Iter last)
{
    while (first != last) {
        destroy(&*first);
        ++first;
    }
}


template <typename Iter, typename PlaceIter>
PlaceIter uninit_copy(Iter first, Iter last, PlaceIter place)
{
    while (first != last) {
        construct(&*place, *first);
        ++first;
        ++place;
    }
    return place;
}


template <typename PlaceIter, typename Size, typename T>
PlaceIter uninit_fill_n(PlaceIter place, Size n, const T& value)
{
    while (n) {
        construct(&*place, value);
        --n;
        ++place;
    }
    return place;
}



} // namespace mySTL

#endif // mySTL_HELPERS_HPP
