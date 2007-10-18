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

// DecodeTransform.h: interface for the DecodeTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DECODETRANSFORM_H__1CCC0F79_F001_429E_AD7E_5821A563909F__INCLUDED_)
#define AFX_DECODETRANSFORM_H__1CCC0F79_F001_429E_AD7E_5821A563909F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

template <class Source, class Encode>
class DecodeTransform : public Transform  
{
public:
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

	DecodeTransform(const char *string) 
		: source(string), 
		  encode(false, &source)
		{}

	DecodeTransform(unsigned int length, const UCHAR *data, bool flag = false) 
		: source(length, data, flag), 
		  encode(false, &source)
		{}
	//virtual ~DecodeTransform();

	Source	source;
	Encode	encode;
};

#endif // !defined(AFX_DECODETRANSFORM_H__1CCC0F79_F001_429E_AD7E_5821A563909F__INCLUDED_)
