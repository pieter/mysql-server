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

// HexTransform.h: interface for the HexTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HEXTRANSFORM_H__B6757056_14F5_4D74_81D7_D6A16ED3E0C3__INCLUDED_)
#define AFX_HEXTRANSFORM_H__B6757056_14F5_4D74_81D7_D6A16ED3E0C3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class HexTransform : public Transform  
{
public:
	virtual void reset();
	UCHAR hexDigit(UCHAR c);
	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	HexTransform(bool encodeFlag, Transform *src);
	virtual ~HexTransform();

	bool		encode;
	Transform	*source;
};

#endif // !defined(AFX_HEXTRANSFORM_H__B6757056_14F5_4D74_81D7_D6A16ED3E0C3__INCLUDED_)
