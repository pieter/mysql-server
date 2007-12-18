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

// BERDecode.h: interface for the BERDecode class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BERDECODE_H__2EB035BA_7EBB_4D2C_84F9_38BBA550F10A__INCLUDED_)
#define AFX_BERDECODE_H__2EB035BA_7EBB_4D2C_84F9_38BBA550F10A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JString.h"

static const int INTEGER		= 0x2;
static const int BIT_STRING		= 0x3;
static const int OCTET_STRING	= 0x4;
static const int NULL_TAG		= 0x5;
static const int OBJECT_TAG		= 0x6;
static const int SEQUENCE		= 0x10;	// 16
static const int SET			= 0x11;	// 17
static const int PRINTABLE		= 0x13; // 19;
static const int T61String		= 0x14; // 20;
static const int IA5String		= 0x16; // 22;
static const int UTCTime		= 0x17; // 23;

static const int CONSTRUCTED	= 0x20;
static const int TAG_MASK		= 0x1f;

static const int MAX_LEVELS		= 16;

class BERItem;
class Transform;

class BERDecode  
{
public:
	int getInteger();
	BERDecode(int length, const UCHAR *gook);
	int getRawLength();
	void print (int level);
	static JString getTagString(int tagNumber);
	void print();
	void popUp();
	void pushDown();
	void next (int requiredTagNumber);
	BERDecode (BERDecode *parent);
	bool next();
	void setSource(Transform *src);
	BERDecode(Transform *src);
	int getType();
	int getLength();
	UCHAR getOctet();
	virtual ~BERDecode();

	const UCHAR	*end;
	const UCHAR	*ptr;
	int			contentLength;
	bool		definiteLength;	
	UCHAR		tagClass;
	UCHAR		tagNumber;
	const UCHAR	*content;
	Transform	*source;
	UCHAR		*buffer;
	int			level;
	const UCHAR	*state[2 * MAX_LEVELS];
};

#endif // !defined(AFX_BERDECODE_H__2EB035BA_7EBB_4D2C_84F9_38BBA550F10A__INCLUDED_)
