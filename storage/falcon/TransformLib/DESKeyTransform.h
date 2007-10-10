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

// DESKeyTransform.h: interface for the DESKeyTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DESKEYTRANSFORM_H__229BA057_01CC_4475_ABB4_EA9B6FC0F696__INCLUDED_)
#define AFX_DESKEYTRANSFORM_H__229BA057_01CC_4475_ABB4_EA9B6FC0F696__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class DESKeyTransform : public Transform  
{
public:
	virtual void reset();
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	DESKeyTransform(bool encodeFlag, Transform *src);
	virtual ~DESKeyTransform();

	UCHAR		key[8];
	int			offset;
	Transform	*source;
};

#endif // !defined(AFX_DESKEYTRANSFORM_H__229BA057_01CC_4475_ABB4_EA9B6FC0F696__INCLUDED_)
