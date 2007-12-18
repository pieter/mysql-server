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

// DESTransform.h: interface for the DESTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DESTRANSFORM_H__F2A558E7_83F2_45B4_ACFC_54F923322470__INCLUDED_)
#define AFX_DESTRANSFORM_H__F2A558E7_83F2_45B4_ACFC_54F923322470__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

static const int DES_encrypt	= 0;
static const int DES_decrypt	= 1;

//#define DES
#include "tomcrypt.h"

class DESTransform : public Transform  
{
public:
	virtual void reset();
	void setKey(const UCHAR *rawKey);
	void setKey (Transform *keyTransform);
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	DESTransform(int operatingMode, Transform *src);
	virtual ~DESTransform();

	Transform	*source;
	int			mode;
	UCHAR		block[8];
	UCHAR		*ptr;
	UCHAR		*end;
	symmetric_key	schedule;
};

#endif // !defined(AFX_DESTRANSFORM_H__F2A558E7_83F2_45B4_ACFC_54F923322470__INCLUDED_)
