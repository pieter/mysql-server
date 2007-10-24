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

// BERItem.cpp: implementation of the BERItem class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "BERItem.h"
#include "BERDecode.h"
#include "BEREncode.h"
#include "BERException.h"
#include "BERObject.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


BERItem::BERItem(BERDecode *decode)
{
	tagClass = decode->tagClass;
	tagNumber = decode->tagNumber;
	children = NULL;
	lastChild = NULL;
	contentLength = decode->contentLength;
	content = decode->content;

	if (tagClass & CONSTRUCTED)
		{
		BERDecode sub (decode);
		
		while (sub.next())
			addChild(parseItem(&sub));
		}
}


BERItem::BERItem(UCHAR itemClass, int itemNumber, int length, const UCHAR *stuff)
{
	tagClass = itemClass;
	tagNumber = itemNumber;
	contentLength = length;
	content = stuff;
	children = NULL;
	lastChild = NULL;
}

BERItem::~BERItem()
{
	for (BERItem *child; child = children;)
		{
		children = child->sibling;
		delete child;
		}
}

void BERItem::addChild(BERItem *child)
{
	if (lastChild)
		lastChild->sibling = child;
	else
		children = child;

	lastChild = child;
	child->sibling = NULL;
}

void BERItem::prettyPrint(int level)
{
	JString identifier = BERDecode::getTagString(tagNumber);
	printf ("%*s %s length %d", level * 3, "", (const char*) identifier, contentLength);
	printContents();
	printf ("\n");
	++level;

	for (BERItem *child = children; child; child = child->sibling)
		child->prettyPrint(level);
}

BERItem* BERItem::findChild(int position)
{
	BERItem *child = children;

	for (int n = 0; n < position && child; ++n, child = child->sibling)
		;

	return child;
}

void BERItem::printContents()
{

}

int BERItem::getIdentifierLength(int tagNumber)
{
	if (tagNumber <= 30)
		return 1;

	if (tagNumber < 128)
		return 2;

	return 3;
}

int BERItem::getIdentifierLength()
{
	return getIdentifierLength(tagNumber);
}

int BERItem::getLengthLength(int length)
{
	if (length < 128)
		return 1;

	int count = 1;

	for (int value = length; value; value >>=  8)
		++count;

	return (count) ? count : 1;
}

int BERItem::getLengthLength()
{
	return getLengthLength(contentLength);
}

int BERItem::getContentLength()
{
	/***
	if (tagNumber != SET && tagNumber != SEQUENCE)
		return contentLength;
	***/

	if (!children)
		return contentLength;

	int length = 0;

	for (BERItem *child = children; child; child = child->sibling)
		length += child->getEncodingLength();

	return length;
}

int BERItem::getEncodingLength()
{
	int len = getIdentifierLength();

	switch (tagNumber)
		{
		case SEQUENCE:
		case SET:
			{
			int contentLen = 0;
			for (BERItem *child = children; child; child = child->sibling)
				{
				int l = child->getEncodingLength();
				contentLen += l;
				}
			len += getLengthLength(contentLen) + contentLen;
			}
			break;

		default:
			len += getLengthLength() + getContentLength();
		}

	return len;
}

int BERItem::getIntegerLength(int integer)
{
	int count = 0;

	for (int value = integer; value || value == -1; value >>= 8)
		++count;

	return (count) ? count : 1;
}

void BERItem::encodeIdentifier(BEREncode *encode)
{
	UCHAR ident = tagClass & 0xC0;
	ident |= (tagNumber <= 30) ? tagNumber : TAG_MASK;
	
	if (children || (tagNumber == SEQUENCE && contentLength > 0))
		ident |= CONSTRUCTED;

	encode->put(ident);
	int count = getIdentifierLength();

	switch (count)
		{
		case 3:
			encode->put(0x80 | (tagNumber >> 7));
		case 2:
			encode->put(tagNumber && 0x7f);
		}
}

int BERItem::encodeLength(BEREncode *encode)
{
	int length = getContentLength();
	int count = getLengthLength(length);

	if (count == 1)
		encode->put(length);
	else
		{
		encode->put(0x80 + count - 1);
		switch (count)
			{
			case 4:
				encode->put(length >> 16);
			case 3:
				encode->put(length >> 8);
			case 2:
				encode->put(length);
			}
		}

	return length;
}

void BERItem::encodeContent(BEREncode *encode, int length)
{

	if (children)
		for (BERItem *child = children; child; child = child->sibling)
			child->encode(encode);
	else
		for (int n = 0; n < length; ++n)
			encode->put(content[n]);	
}

void BERItem::encode(BEREncode *encode)
{
	encodeIdentifier(encode);
	int length = encodeLength(encode);
	encodeContent(encode, length);
}

void BERItem::encodeInteger(BEREncode *encode, int number)
{
	int count = getIntegerLength(number);

	while (--count >= 0)
		encode->put(number >> (count * 8));
}

int BERItem::getComponentLength(int component)
{
	int count = 0;

	for (int value = component; value; value >>= 7)
		++count;

	return (count) ? count : 1;
}

void BERItem::encodeComponent(BEREncode *encode, int component)
{
	int count = getComponentLength(component);

	while (--count > 0)
		encode->put(0x80 | (component >> (count * 7)));

	encode->put(0x7F & component);
}

BERItem* BERItem::parseItem(BERDecode *decode)
{
	if (!decode->next())
		throw BERException("no BER item");

	BERItem *item;

	switch (decode->tagNumber)
		{
		case OBJECT_TAG:
			item = new BERObject(decode);
			break;

		default:
			item = new BERItem(decode);
		}

	//item->parseLength = ptr - start;

	return item;
}
