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

// SHATransform.h: interface for the SHATransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SHATRANSFORM_H__53E18F6D_9816_4F75_9451_D6488CE1BF63__INCLUDED_)
#define AFX_SHATRANSFORM_H__53E18F6D_9816_4F75_9451_D6488CE1BF63__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

static const int SHA1_LENGTH	= 20;

typedef struct {
    uint32 state[5];
    uint32 count[2];
    UCHAR buffer[64];
} SHA1_CTX;

class SHATransform : public Transform  
{
public:
	virtual void reset();
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	SHATransform(int encrypt, Transform *src);
	virtual ~SHATransform();

	Transform	*source;
	SHA1_CTX	context;
	UCHAR		digest [SHA1_LENGTH];
	UCHAR		workspace[64];
	int			offset;
};

#endif // !defined(AFX_SHATRANSFORM_H__53E18F6D_9816_4F75_9451_D6488CE1BF63__INCLUDED_)
