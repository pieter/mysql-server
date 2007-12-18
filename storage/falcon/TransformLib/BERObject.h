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

// BERObject.h: interface for the BERObject class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEROBJECT_H__D343263B_CDE3_4A35_8FC9_091CAD47C136__INCLUDED_)
#define AFX_BEROBJECT_H__D343263B_CDE3_4A35_8FC9_091CAD47C136__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BERItem.h"

class BERObject : public BERItem  
{
public:
	BERObject(BERDecode *decode);
	virtual void encodeContent(BEREncode *encode, int length);
	BERObject(UCHAR itemClass, int valueCount, const int *values);
	virtual int getContentLength();
	virtual void printContents();
	BERObject(BERDecode *decode, UCHAR itemClass, int itemNumber);
	virtual ~BERObject();

	int		numbers[10];
	int		count;
};

#endif // !defined(AFX_BEROBJECT_H__D343263B_CDE3_4A35_8FC9_091CAD47C136__INCLUDED_)
