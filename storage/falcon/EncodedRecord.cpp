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

// EncodedRecord.cpp: implementation of the EncodedRecord class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "EncodedRecord.h"
#include "Stream.h"
#include "Table.h"
#include "Value.h"

static const UCHAR lengthShifts[] = { 0, 8, 16, 24, 32, 40, 48, 56 };

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

EncodedRecord::EncodedRecord(Table *tbl, Transaction *trans, Stream *stream) : EncodedDataStream (stream)
{
	table = tbl;
	transaction = trans;
}

EncodedRecord::~EncodedRecord()
{

}

void EncodedRecord::encodeAsciiBlob(Value *value)
{
	int32 val = table->getBlobId (value, 0, false, transaction);
	EncodedDataStream::encodeAsciiBlob(val);

	/***
	int count = BYTE_COUNT(val);
	stream->putCharacter((char) (edsClobLen0 + count));

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
	***/
}

void EncodedRecord::encodeBinaryBlob(Value *value)
{
	int32 val = table->getBlobId (value, 0, false, transaction);
		EncodedDataStream::encodeBinaryBlob(val);

	/***
	int count = BYTE_COUNT(val);
	stream->putCharacter((char) (edsBlobLen0 + count));

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
	***/
}

