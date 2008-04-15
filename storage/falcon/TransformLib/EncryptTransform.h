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

// EncryptTransform.h: interface for the EncryptTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCRYPTTRANSFORM_H__5C655B30_36DF_4B2B_8C23_F534B3BD2092__INCLUDED_)
#define AFX_ENCRYPTTRANSFORM_H__5C655B30_36DF_4B2B_8C23_F534B3BD2092__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

template <class Source, class Encrypt, class Encode>
class EncryptTransform : public Transform  
{
public:
	EncryptTransform(const char *string, int mode)
			: source(string), 
			  encrypt(mode, &source), 
			  encode(true, &encrypt)
		{};

	EncryptTransform(unsigned int length, UCHAR *data, int mode)
			: source(length, data), 
			  encrypt(mode, &source), 
			  encode(true, &encrypt)
		{}

	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer)
		{
		return encode.get(bufferLength, buffer);
		}

	virtual unsigned int getLength()
		{
		return encode.getLength();
		}


	virtual void reset()
		{
		encode.reset();
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
	Encrypt	encrypt;
	Encode	encode;
};

#endif // !defined(AFX_ENCRYPTTRANSFORM_H__5C655B30_36DF_4B2B_8C23_F534B3BD2092__INCLUDED_)