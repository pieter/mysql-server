/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// ZLibTransform.h: interface for the ZLibTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ZLIBTRANSFORM_H__A7D3180B_F91E_4063_BAC3_21E4DEB29CC5__INCLUDED_)
#define AFX_ZLIBTRANSFORM_H__A7D3180B_F91E_4063_BAC3_21E4DEB29CC5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"
#include "zlib.h"

/* following lines taken from "zutil.h" as binary installs may not have it */

#ifndef DEF_WBITS
#  define DEF_WBITS MAX_WBITS
#endif

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

class ZLibTransform : public Transform  
{
public:
	static void free(void *opaque, void *memory);
	static void* alloc (void *opaque, unsigned int items, unsigned int size);
	virtual void reset();
	virtual unsigned int getLength();
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	ZLibTransform(bool encodeFlag, Transform *src);
	virtual ~ZLibTransform();

	bool		encode;
	Transform	*source;
	z_stream	zStream;
	bool		active;
	bool		eof;
	UCHAR		temp [1024];
	unsigned int residualLength;
};

#endif // !defined(AFX_ZLIBTRANSFORM_H__A7D3180B_F91E_4063_BAC3_21E4DEB29CC5__INCLUDED_)
