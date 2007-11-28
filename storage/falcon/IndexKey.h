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

// IndexKey.h: interface for the IndexKey class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INDEXKEY_H__F250AB38_2FE1_4DE5_AE40_1FE016845661__INCLUDED_)
#define AFX_INDEXKEY_H__F250AB38_2FE1_4DE5_AE40_1FE016845661__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// The current index design requires a minimum of three key entries on each page.
// MAX_INDEX_KEY_LENGTH = (((pageSize - keyPageOverhead(52)) / minKeysPerPage(3)) - keyOverhead(17)) * (RUN - 1) / RUN; 

static const uint MAX_INDEX_KEY_LENGTH_1K	=  255;  // actually 255;
static const uint MAX_INDEX_KEY_LENGTH_2K	=  540;  // actually 540;
static const uint MAX_INDEX_KEY_LENGTH_4K	= 1100;  // actually 1109, round off;
static const uint MAX_INDEX_KEY_LENGTH_8K	= 2200;  // actually 2246, round off;
static const uint MAX_INDEX_KEY_LENGTH_16K	= 4500;  // actually 4522, round off;
static const uint MAX_INDEX_KEY_LENGTH_32K	= 9000;  // actually 9073, round off;
static const uint MAX_INDEX_KEY_LENGTH		= MAX_INDEX_KEY_LENGTH_4K;
static const uint RUN						= 6;
static const uint MAX_INDEX_KEY_RUN_LENGTH	= (MAX_INDEX_KEY_LENGTH * RUN / (RUN - 1));
static const uint MAX_PHYSICAL_KEY_LENGTH	= MAX_INDEX_KEY_RUN_LENGTH + sizeof(int64) + 1;
static const uint MAX_KEY_SEGMENTS			= 32;


class Index;

class IndexKey
{
public:
	IndexKey (int length, const unsigned char *indexKey);
	IndexKey();
	IndexKey (IndexKey *indexKey);
	IndexKey(Index *idx);
	virtual ~IndexKey();

	void	appendNumber (double number);
	void	appendRecordNumber(int32 recordNumber);
	int32	getRecordNumber(void);
	bool	isEqual (IndexKey *indexKey);
	void	setKey (IndexKey *indexKey);
	void	setKey(int length, const unsigned char* keyPtr);
	int		checkTail(const unsigned char* endPoint);
	void	setKey(int offset, int length, const unsigned char *data);

	Index	*index;
	uint	keyLength;
	int32	recordNumber;
	unsigned char	key[MAX_PHYSICAL_KEY_LENGTH];

	inline unsigned char *keyEnd()
		{
		return key + keyLength;
		};
	
	inline int getAppendedRecordNumberLength()
		{
		return (keyLength ? key[keyLength - 1] & 0x0f : 0);
		}
	
	int compareValues(IndexKey* indexKey);
	int compareValues(unsigned char *key2, uint len2, bool isPartial);
	int compare(IndexKey* indexKey);
};

#endif // !defined(AFX_INDEXKEY_H__F250AB38_2FE1_4DE5_AE40_1FE016845661__INCLUDED_)
