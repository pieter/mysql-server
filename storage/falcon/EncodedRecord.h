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

// EncodedRecord.h: interface for the EncodedRecord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODEDRECORD_H__3472AE47_92A9_4A28_8B30_503BA48C86B1__INCLUDED_)
#define AFX_ENCODEDRECORD_H__3472AE47_92A9_4A28_8B30_503BA48C86B1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "EncodedDataStream.h"

class Transaction;
class Table;
class Record;
class Stream;

class EncodedRecord : public EncodedDataStream  
{
public:
	//void encode(Record *record);
	virtual void encodeBinaryBlob(Value *value);
	virtual void encodeAsciiBlob(Value *value);
	EncodedRecord(Table *tbl, Transaction *trans, Stream *stream);
	virtual ~EncodedRecord();

	Table		*table;
	Transaction	*transaction;
	int			itemCount;
};

#endif // !defined(AFX_ENCODEDRECORD_H__3472AE47_92A9_4A28_8B30_503BA48C86B1__INCLUDED_)
