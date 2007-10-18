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

// BERItem.h: interface for the BERItem class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BERITEM_H__F2275A69_DD1A_4F56_BD2F_53E7192C5649__INCLUDED_)
#define AFX_BERITEM_H__F2275A69_DD1A_4F56_BD2F_53E7192C5649__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class BERDecode;
class BEREncode;

class BERItem  
{
public:
	static BERItem* parseItem (BERDecode *decode);
	BERItem (BERDecode *decode);
	void encodeComponent(BEREncode *encode, int component);
	static int getComponentLength(int component);
	void encodeInteger(BEREncode *encode, int number);
	BERItem(UCHAR itemClass, int number, int length, const UCHAR *stuff);
	virtual void encode(BEREncode *encode);
	virtual void encodeContent(BEREncode *encode, int length);
	int encodeLength(BEREncode *encode);
	void encodeIdentifier(BEREncode *encode);
	static int getIntegerLength(int integer);
	virtual int getEncodingLength();
	virtual int getContentLength();
	virtual int getLengthLength();
	static int getLengthLength(int len);
	virtual int getIdentifierLength();
	static int getIdentifierLength(int tagNumber);
	virtual void printContents();
	BERItem* findChild(int position);
	virtual void prettyPrint(int column);
	void addChild (BERItem *child);
	//BERItem(BERDecode *decode, UCHAR itemClass, int itemNumber);
	virtual ~BERItem();

	BERItem			*children;
	BERItem			*sibling;
	BERItem			*lastChild;
	UCHAR			tagClass;
	int				tagNumber;
	int				contentLength;
	int				parseLength;
	const UCHAR		*content;
};

#endif // !defined(AFX_BERITEM_H__F2275A69_DD1A_4F56_BD2F_53E7192C5649__INCLUDED_)
