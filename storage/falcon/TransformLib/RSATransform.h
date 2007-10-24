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

// RSATransform.h: interface for the RSATransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RSATRANSFORM_H__F8A482DA_30B0_4CE4_818C_CE2394FF372E__INCLUDED_)
#define AFX_RSATRANSFORM_H__F8A482DA_30B0_4CE4_818C_CE2394FF372E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

//#define MRSA
#include "tomcrypt.h"

static const int RSA_publicEncrypt	= 0;
static const int RSA_privateDecrypt = 1;
static const int RSA_publicDecrypt	= 3;
static const int RSA_privateEncrypt = 4;

class RSATransform : public Transform  
{
public:
	void throwBadKey();
	void setKey(Transform *transform);
	int getBlockSize();
	virtual void reset();
	void check(int retValue);
	//void setKey(Transform *transform);
	int getSize();
	virtual void setPrivateKey (Transform *transform);
	virtual void setPublicKey (Transform *transform);
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	RSATransform(int operatingMode, Transform *src);
	virtual ~RSATransform();

	int			mode;
	Transform	*source;
	UCHAR		*ptr;
	UCHAR		*end;
	UCHAR		output[1024];
	rsa_key		rsaKey;
	int			prngIndex;		// pseudo random number generator
	int			hashIndex;		// hash function index
	int			hashLength;
};

#endif // !defined(AFX_RSATRANSFORM_H__F8A482DA_30B0_4CE4_818C_CE2394FF372E__INCLUDED_)
