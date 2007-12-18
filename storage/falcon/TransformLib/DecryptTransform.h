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

// DecryptTransform.h: interface for the DecryptTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DECRYPTTRANSFORM_H__574347AE_A6CA_4FA5_AC0B_264BABC01DF0__INCLUDED_)
#define AFX_DECRYPTTRANSFORM_H__574347AE_A6CA_4FA5_AC0B_264BABC01DF0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

template <class Source, class Decode, class Decrypt>
class DecryptTransform : public Transform  
{
public:
	DecryptTransform(const char *string, int mode)
		: source(string), 
		  decode(false, &source),
		  decrypt(mode, &decode)
		{}

	DecryptTransform(int length, UCHAR *data, int mode)
		: source(length, data), 
		  decode(false, &source),
		  decrypt(mode, &decode)
		{}

	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer)
		{
		return decrypt.get(bufferLength, buffer);
		}

	virtual unsigned int getLength()
		{
		return decrypt.getLength();
		}

	virtual void reset()
		{
		decrypt.reset();
		}

	virtual void setString(const char *string, bool flag = false)
		{
		reset();
		source.setString(string, flag);
		}

	virtual void setString(unsigned int length, const UCHAR *data, bool flag = false)
		{
		reset();
		source.setString(length, data, flag);
		}

	Source	source;
	Decode	decode;
	Decrypt	decrypt;
};

#endif // !defined(AFX_DECRYPTTRANSFORM_H__574347AE_A6CA_4FA5_AC0B_264BABC01DF0__INCLUDED_)
