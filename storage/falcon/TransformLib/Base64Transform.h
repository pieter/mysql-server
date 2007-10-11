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

// Base64Transform.h: interface for the Base64Transform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BASE64TRANSFORM_H__816F5106_A34C_4FE0_A6FF_3A6E725EBB20__INCLUDED_)
#define AFX_BASE64TRANSFORM_H__816F5106_A34C_4FE0_A6FF_3A6E725EBB20__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class Base64Transform : public Transform  
{
public:
	virtual void reset();
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	Base64Transform(bool encodeFlag, Transform *src);
	virtual ~Base64Transform();

	bool		encode;
	Transform	*source;
	int			bitsRemaining;
	int			bits;
	int			inputLength;
	int			padding;
	UCHAR		*ptr;
	UCHAR		*end;
	UCHAR		*tail;
	UCHAR		input[256];
	bool		ignoreStrays;
};

#endif // !defined(AFX_BASE64TRANSFORM_H__816F5106_A34C_4FE0_A6FF_3A6E725EBB20__INCLUDED_)
