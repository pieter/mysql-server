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

// EncodedDataStream.cpp: implementation of the EncodedDataStream class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "EncodedDataStream.h"
#include "Value.h"
#include "Stream.h"
#include "SQLError.h"
#include "TimeStamp.h"
#include "BigInt.h"

static const UCHAR	lengthShifts[] = { 0, 8, 16, 24, 32, 40, 48, 56 };
static int dummy = EncodedDataStream::init();
signed char	EncodedDataStream::lengths[256];


int EncodedDataStream::init(void)
{
	int code;
	
	for (code = edsIntLen1; code <= edsIntLen8; ++code)
		lengths[code] = code - edsIntLen1 + 1;

	for (code = edsScaledLen0; code <= edsScaledLen8; ++code)
		lengths[code] = code - edsScaledLen0 + 1;

	for (code = edsDoubleLen0; code <= edsDoubleLen8; ++code)
		lengths[code] = code - edsDoubleLen0;

	for (code = edsMilliSecLen0; code <= edsMilliSecLen8; ++code)
		lengths[code] = code - edsMilliSecLen0;

	for (code = edsNanoSecLen0; code <= edsNanoSecLen8; ++code)
		lengths[code] = code - edsNanoSecLen0;

	for (code = edsTimeLen0; code <= edsTimeLen4; ++code)
		lengths[code] = code - edsTimeLen0;

	for (code = edsUtf8Len0; code <= edsUft8LenMax; ++code)
		lengths[code] = code - edsUtf8Len0;

	for (code = edsOpaqueLen0; code <= edsOpaqueLenMax; ++code)
		lengths[code] = code - edsOpaqueLen0;

	for (code = edsBlobLen0; code <= edsBlobLen4; ++code)
		lengths[code] = code - edsBlobLen0;

	for (code = edsClobLen0; code <= edsClobLen4; ++code)
		lengths[code] = code - edsClobLen0;

	for (code = edsUtf8Count1; code <= edsUtf8Count4; ++code)
		lengths[code] = edsUtf8Count1 - code - 1;
	
	for (code = edsOpaqueCount1; code <= edsOpaqueCount4; ++code)
		lengths[code] = edsOpaqueCount1 - code - 1;
	
	lengths[edsScaledCount1] = -1;

	return 0;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

EncodedDataStream::EncodedDataStream()
{
	stream = NULL;
}

EncodedDataStream::EncodedDataStream(Stream *strm)
{
	stream = strm;
}

EncodedDataStream::EncodedDataStream(const unsigned char *data)
{
	ptr = data;
}

EncodedDataStream::~EncodedDataStream()
{
}



void EncodedDataStream::encode(int type, Value *value)
{
	if (value->isNull())
		{
		stream->putCharacter(edsNull);
		
		return;
		}

	switch (type)
		{
		case String:
		case Char:
		case Varchar:
			{
			int len;
			const char *data;
			char *temp = NULL;

			switch (value->getType())
				{
				case String:
				case Char:
				case Varchar:
					len = value->getStringLength();
					data = value->getString();
					break;

				default:
					{
					char buffer [128];
					len = value->getStringLength();

					if (len < (int)sizeof(buffer))
						{
						len = value->getString(sizeof(buffer), buffer);
						data = buffer;
						}
					else
						data = value->getString(&temp);
					}
				}
		
			if (len <= edsUtf8Len39 - edsUtf8Len0)
				stream->putCharacter(edsUtf8Len0 + len);
			else
				{
				int count = BYTE_COUNT(len);
				stream->putCharacter(edsUtf8Count1 + count - 1);

				while (--count >= 0)
					stream->putCharacter(len >> (lengthShifts [count]));
				}

			if (len)
				stream->putSegment(len, data, true);

			if (temp)
				delete [] temp;
			}
			break;

		case Short:
		case Int32:
			{
			int scale = value->getScale();
			int val = value->getInt(scale);
			int count = BYTE_COUNT(val);

			if (scale)
				{
				stream->putCharacter(edsScaledLen0 + count);
				stream->putCharacter(scale);
				}
			else if (val >= edsIntMin && val <= edsIntMax)
				{
				stream->putCharacter(edsInt0 + val);
				break;
				}
			else
				stream->putCharacter(edsIntLen1 + count - 1);

			while (--count >= 0)
				stream->putCharacter(val >> (lengthShifts [count]));
			}
			break;

		
		case Int64:
			{
			int scale = value->getScale();
			INT64 val = value->getQuad(scale);
			int count = BYTE_COUNT64(val);

			if (scale)
				{
				stream->putCharacter(edsScaledLen0 + count);
				stream->putCharacter(scale);
				}
			else if (val >= edsIntMin && val <= edsIntMax)
				{
				stream->putCharacter((char) (edsInt0 + val));
				break;
				}
			else
				stream->putCharacter(edsIntLen1 + count - 1);

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			}
			break;

		case Timestamp:
			{
			TimeStamp timestamp = value->getTimestamp();
			INT64 val = timestamp.getMilliseconds() * 1000 +
						timestamp.getNanos();
			int count = BYTE_COUNT64(val);
			stream->putCharacter(edsNanoSecLen0 + count);

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			}
			break;

		case Date:
			{
			DateTime date = value->getDate();
			INT64 val = date.getMilliseconds();
			int count = BYTE_COUNT64(val);
			stream->putCharacter(edsMilliSecLen0 + count);

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			}
			break;

		case TimeType:
			{
			Time time = value->getTime();
			INT64 val = time.getMilliseconds();
			int count = BYTE_COUNT64(val);
			stream->putCharacter(edsTimeLen0 + count);

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			}
			break;

		case ClobPtr:
			encodeAsciiBlob(value);
			break;

		/***
		case Asciiblob:
			{
			int32 val = value->data.integer;
			int count = BYTE_COUNT(val);
			stream->putCharacter((char) (edsClobLen0 + count));

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			break;
			}

		case Binaryblob:
			{
			int32 val = value->data.integer;
			int count = BYTE_COUNT(val);
			stream->putCharacter((char) (edsBlobLen0 + count));

			while (--count >= 0)
				stream->putCharacter((char) (val >> (lengthShifts [count])));
			break;
			}
		***/

		case BlobPtr:
			encodeBinaryBlob(value);
			break;

		case Float:
		case Double:
			encodeDouble(value->getDouble());
			break;

		case Asciiblob:
			encodeAsciiBlob(value);
			break;

		case Binaryblob:
			encodeBinaryBlob(value);
			break;

		case Biginteger:
			encodeBigInt(value);
			break;
			
		default:
			unsupported(value);
		}

}

const UCHAR* EncodedDataStream::decode(const UCHAR *ptr, Value *value, bool copyFlag)
{
	const UCHAR *p = ptr;
	UCHAR code = *p++;

	switch(code)
		{
		case edsNull:
			value->setNull();
			break;

		case edsIntMinus10:
		case edsIntMinus9:
		case edsIntMinus8:
		case edsIntMinus7:
		case edsIntMinus6:
		case edsIntMinus5:
		case edsIntMinus4:
		case edsIntMinus3:
		case edsIntMinus2:
		case edsIntMinus1:
		case edsInt0:
		case edsInt1:
		case edsInt2:
		case edsInt3:
		case edsInt4:
		case edsInt5:
		case edsInt6:
		case edsInt7:
		case edsInt8:
		case edsInt9:
		case edsInt10:
		case edsInt11:
		case edsInt12:
		case edsInt13:
		case edsInt14:
		case edsInt15:
		case edsInt16:
		case edsInt17:
		case edsInt18:
		case edsInt19:
		case edsInt20:
		case edsInt21:
		case edsInt22:
		case edsInt23:
		case edsInt24:
		case edsInt25:
		case edsInt26:
		case edsInt27:
		case edsInt28:
		case edsInt29:
		case edsInt30:
		case edsInt31:
			value->setValue((short) (code - edsInt0));
			break;

		case edsIntLen1:
		case edsIntLen2:
		case edsIntLen3:
		case edsIntLen4:
			{
			int l = code - edsIntLen1;
			int32 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			value->setValue(val);
			}
			break;

		case edsIntLen5:
		case edsIntLen6:
		case edsIntLen7:
		case edsIntLen8:
			{
			int l = code - edsIntLen1;
			INT64 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			value->setValue(val);
			}
			break;

		case edsScaledLen0:
			*p++;
			value->setValue((int32) 0);
			break;

		case edsScaledLen1:
		case edsScaledLen2:
		case edsScaledLen3:
		case edsScaledLen4:
			{
			int scale = *p++;
			int l = code - edsScaledLen1;
			int32 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			value->setValue(val, scale);
			}
			break;

		case edsScaledLen5:
		case edsScaledLen6:
		case edsScaledLen7:
		case edsScaledLen8:
			{
			int scale = *p++;
			int l = code - edsScaledLen1;
			INT64 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			value->setValue(val, scale);
			}
			break;

		case edsUtf8Len0:
		case edsUtf8Len1:
		case edsUtf8Len2:
		case edsUtf8Len3:
		case edsUtf8Len4:
		case edsUtf8Len5:
		case edsUtf8Len6:
		case edsUtf8Len7:
		case edsUtf8Len8:
		case edsUtf8Len9:
		case edsUtf8Len10:
		case edsUtf8Len11:
		case edsUtf8Len12:
		case edsUtf8Len13:
		case edsUtf8Len14:
		case edsUtf8Len15:
		case edsUtf8Len16:
		case edsUtf8Len17:
		case edsUtf8Len18:
		case edsUtf8Len19:
		case edsUtf8Len20:
		case edsUtf8Len21:
		case edsUtf8Len22:
		case edsUtf8Len23:
		case edsUtf8Len24:
		case edsUtf8Len25:
		case edsUtf8Len26:
		case edsUtf8Len27:
		case edsUtf8Len28:
		case edsUtf8Len29:
		case edsUtf8Len30:
		case edsUtf8Len31:
		case edsUtf8Len32:
		case edsUtf8Len33:
		case edsUtf8Len34:
		case edsUtf8Len35:
		case edsUtf8Len36:
		case edsUtf8Len37:
		case edsUtf8Len38:
		case edsUtf8Len39:
			{
			int len = code - edsUtf8Len0;
			value->setString(len, (const char*) p, copyFlag);
			p += len;
			}
			break;

		case edsUtf8Count1:
		case edsUtf8Count2:
		case edsUtf8Count3:
		case edsUtf8Count4:
			{
			int count = code - edsUtf8Count1;
			int length = *p++;

			for (int n = 0; n < count; ++n)
				length = length << 8 | *p++;

			value->setString(length, (const char*) p, copyFlag);
			p += length;
			}
			break;

		case edsOpaqueLen0:
		case edsOpaqueLen1:
		case edsOpaqueLen2:
		case edsOpaqueLen3:
		case edsOpaqueLen4:
		case edsOpaqueLen5:
		case edsOpaqueLen6:
		case edsOpaqueLen7:
		case edsOpaqueLen8:
		case edsOpaqueLen9:
		case edsOpaqueLen10:
		case edsOpaqueLen11:
		case edsOpaqueLen12:
		case edsOpaqueLen13:
		case edsOpaqueLen14:
		case edsOpaqueLen15:
		case edsOpaqueLen16:
		case edsOpaqueLen17:
		case edsOpaqueLen18:
		case edsOpaqueLen19:
		case edsOpaqueLen20:
		case edsOpaqueLen21:
		case edsOpaqueLen22:
		case edsOpaqueLen23:
		case edsOpaqueLen24:
		case edsOpaqueLen25:
		case edsOpaqueLen26:
		case edsOpaqueLen27:
		case edsOpaqueLen28:
		case edsOpaqueLen29:
		case edsOpaqueLen30:
		case edsOpaqueLen31:
		case edsOpaqueLen32:
		case edsOpaqueLen33:
		case edsOpaqueLen34:
		case edsOpaqueLen35:
		case edsOpaqueLen36:
		case edsOpaqueLen37:
		case edsOpaqueLen38:
		case edsOpaqueLen39:
			{
			int len = code - edsOpaqueLen0;
			value->setString(len, (const char*) p, copyFlag);
			p += len;
			}
			break;

		case edsOpaqueCount1:
		case edsOpaqueCount2:
		case edsOpaqueCount3:
		case edsOpaqueCount4:
			{
			int count =  code - edsOpaqueCount1;
			int length = *p++;

			for (int n = 0; n < count; ++n)
				length = length << 8 | *p++;

			value->setString(length, (const char*) p, copyFlag);
			p += length;
			}
			break;

		case edsDoubleLen0:
		case edsDoubleLen1:
		case edsDoubleLen2:
		case edsDoubleLen3:
		case edsDoubleLen4:
		case edsDoubleLen5:
		case edsDoubleLen6:
		case edsDoubleLen7:
		case edsDoubleLen8:
			{
			union {
				UCHAR chars[8];
				double d;
				} u;

			u.d = 0;
			int count = code - edsDoubleLen0;

#ifdef _BIG_ENDIAN
			for (int n = 0; n < count; ++n)
				u.chars[n] = *p++;
#else
			for (int n = 0; n < count; ++n)
				u.chars[7 - n] = *p++;
#endif

			value->setValue(u.d);
			}
			break;

		case edsTimeLen0:
			{
			Time dt;
			dt.setMilliseconds(0);
			value->setValue(dt);
			}
			break;

		case edsTimeLen1:
		case edsTimeLen2:
		case edsTimeLen3:
		case edsTimeLen4:
			{
			int l = code - edsTimeLen1;
			INT64 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			Time dt;
			dt.setMilliseconds(val);
			value->setValue(dt);
			}
			break;

		case edsMilliSecLen0:
			{
			DateTime dt;
			dt.setMilliseconds(0);
			value->setValue(dt);
			}
			break;

		case edsMilliSecLen1:
		case edsMilliSecLen2:
		case edsMilliSecLen3:
		case edsMilliSecLen4:
		case edsMilliSecLen5:
		case edsMilliSecLen6:
		case edsMilliSecLen7:
		case edsMilliSecLen8:
			{
			int l = code - edsMilliSecLen1;
			INT64 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			DateTime dt;
			dt.setMilliseconds(val);
			value->setValue(dt);
			}
			break;

		case edsNanoSecLen0:
			{
			TimeStamp dt;
			dt.setMilliseconds(0);
			value->setValue(dt);
			}
			break;

		case edsNanoSecLen1:
		case edsNanoSecLen2:
		case edsNanoSecLen3:
		case edsNanoSecLen4:
		case edsNanoSecLen5:
		case edsNanoSecLen6:
		case edsNanoSecLen7:
		case edsNanoSecLen8:
			{
			int l = code - edsNanoSecLen1;
			INT64 val = (signed char) *p++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *p++;

			TimeStamp dt;
			dt.setMilliseconds(val / 1000);
			dt.setNanos((int32) (val % 1000));
			value->setValue(dt);
			}
			break;

		case edsClobLen0:
		case edsClobLen1:
		case edsClobLen2:
		case edsClobLen3:
		case edsClobLen4:
			{
			int count = code - edsClobLen0;
			int blobId = 0;

			for (int n = 0; n < count; ++n)
				blobId = blobId << 8 | *p++;

			value->setAsciiBlob(blobId);
			}
			break;

		case edsBlobLen0:
		case edsBlobLen1:
		case edsBlobLen2:
		case edsBlobLen3:
		case edsBlobLen4:
			{
			int count = code - edsBlobLen0;
			int blobId = 0;

			for (int n = 0; n < count; ++n)
				blobId = blobId << 8 | *p++;

			value->setBinaryBlob(blobId);
			}
			break;

		case edsScaledCount1:
			{
			int length = *p++ - 1;
			int scale = *(const signed char*)p++;
			value->setBigInt()->setBytes(scale, length, (const char*) p);
			p += length;
			}
			break;
			
		default:
			throw SQLError (RUNTIME_ERROR, "Unknown data stream code %d", p[-1]);
		}

	return p;
}

void EncodedDataStream::encodeAsciiBlob(Value *value)
{
	unsupported(value);
}

void EncodedDataStream::encodeBinaryBlob(Value *value)
{
	unsupported(value);
}

void EncodedDataStream::unsupported(Value *value)
{
	throw SQLError(RUNTIME_ERROR, "unsupported data type %d", value->getType());
}

DataStreamType EncodedDataStream::decode()
{
	UCHAR code = *ptr++;
	
	switch(code)
		{
		case edsNull:
			type = edsTypeNull;
			break;

		case edsIntMinus10:
		case edsIntMinus9:
		case edsIntMinus8:
		case edsIntMinus7:
		case edsIntMinus6:
		case edsIntMinus5:
		case edsIntMinus4:
		case edsIntMinus3:
		case edsIntMinus2:
		case edsIntMinus1:
		case edsInt0:
		case edsInt1:
		case edsInt2:
		case edsInt3:
		case edsInt4:
		case edsInt5:
		case edsInt6:
		case edsInt7:
		case edsInt8:
		case edsInt9:
		case edsInt10:
		case edsInt11:
		case edsInt12:
		case edsInt13:
		case edsInt14:
		case edsInt15:
		case edsInt16:
		case edsInt17:
		case edsInt18:
		case edsInt19:
		case edsInt20:
		case edsInt21:
		case edsInt22:
		case edsInt23:
		case edsInt24:
		case edsInt25:
		case edsInt26:
		case edsInt27:
		case edsInt28:
		case edsInt29:
		case edsInt30:
		case edsInt31:
			value.integer32 = code - edsInt0;
			scale = 0;
			type = edsTypeInt32;
			break;

		case edsIntLen1:
		case edsIntLen2:
		case edsIntLen3:
		case edsIntLen4:
			{
			int l = code - edsIntLen1;
			int32 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer32 = val;
			scale = 0;
			type = edsTypeInt32;
			}
			break;

		case edsIntLen5:
		case edsIntLen6:
		case edsIntLen7:
		case edsIntLen8:
			{
			int l = code - edsIntLen1;
			INT64 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer64 = val;
			scale = 0;
			type = edsTypeInt64;
			}
			break;

		case edsScaledLen0:
			scale = (signed char) *ptr++;
			value.integer64 = 0;
			type = edsTypeScaled;
			break;

		case edsScaledLen1:
		case edsScaledLen2:
		case edsScaledLen3:
		case edsScaledLen4:
		case edsScaledLen5:
		case edsScaledLen6:
		case edsScaledLen7:
		case edsScaledLen8:
			{
			scale = (signed char) *ptr++;
			int l = code - edsScaledLen1;
			INT64 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer64 = val;
			type = edsTypeScaled;
			}
			break;

		case edsUtf8Len0:
		case edsUtf8Len1:
		case edsUtf8Len2:
		case edsUtf8Len3:
		case edsUtf8Len4:
		case edsUtf8Len5:
		case edsUtf8Len6:
		case edsUtf8Len7:
		case edsUtf8Len8:
		case edsUtf8Len9:
		case edsUtf8Len10:
		case edsUtf8Len11:
		case edsUtf8Len12:
		case edsUtf8Len13:
		case edsUtf8Len14:
		case edsUtf8Len15:
		case edsUtf8Len16:
		case edsUtf8Len17:
		case edsUtf8Len18:
		case edsUtf8Len19:
		case edsUtf8Len20:
		case edsUtf8Len21:
		case edsUtf8Len22:
		case edsUtf8Len23:
		case edsUtf8Len24:
		case edsUtf8Len25:
		case edsUtf8Len26:
		case edsUtf8Len27:
		case edsUtf8Len28:
		case edsUtf8Len29:
		case edsUtf8Len30:
		case edsUtf8Len31:
		case edsUtf8Len32:
		case edsUtf8Len33:
		case edsUtf8Len34:
		case edsUtf8Len35:
		case edsUtf8Len36:
		case edsUtf8Len37:
		case edsUtf8Len38:
		case edsUtf8Len39:
			{
			value.string.length = code - edsUtf8Len0;
			value.string.data = ptr;
			ptr += value.string.length;
			type = edsTypeUtf8;
			}
			break;

		case edsUtf8Count1:
		case edsUtf8Count2:
		case edsUtf8Count3:
		case edsUtf8Count4:
			{
			int count = code - edsUtf8Count1;
			int length = *ptr++;

			for (int n = 0; n < count; ++n)
				length = length << 8 | *ptr++;

			value.string.length = length;
			value.string.data = ptr;
			ptr += length;
			type = edsTypeUtf8;
			}
			break;

		case edsOpaqueLen0:
		case edsOpaqueLen1:
		case edsOpaqueLen2:
		case edsOpaqueLen3:
		case edsOpaqueLen4:
		case edsOpaqueLen5:
		case edsOpaqueLen6:
		case edsOpaqueLen7:
		case edsOpaqueLen8:
		case edsOpaqueLen9:
		case edsOpaqueLen10:
		case edsOpaqueLen11:
		case edsOpaqueLen12:
		case edsOpaqueLen13:
		case edsOpaqueLen14:
		case edsOpaqueLen15:
		case edsOpaqueLen16:
		case edsOpaqueLen17:
		case edsOpaqueLen18:
		case edsOpaqueLen19:
		case edsOpaqueLen20:
		case edsOpaqueLen21:
		case edsOpaqueLen22:
		case edsOpaqueLen23:
		case edsOpaqueLen24:
		case edsOpaqueLen25:
		case edsOpaqueLen26:
		case edsOpaqueLen27:
		case edsOpaqueLen28:
		case edsOpaqueLen29:
		case edsOpaqueLen30:
		case edsOpaqueLen31:
		case edsOpaqueLen32:
		case edsOpaqueLen33:
		case edsOpaqueLen34:
		case edsOpaqueLen35:
		case edsOpaqueLen36:
		case edsOpaqueLen37:
		case edsOpaqueLen38:
		case edsOpaqueLen39:
			{
			value.string.length = code - edsOpaqueLen0;
			value.string.data = ptr;
			ptr += value.string.length;
			type = edsTypeOpaque;
			}
			break;

		case edsOpaqueCount1:
		case edsOpaqueCount2:
		case edsOpaqueCount3:
		case edsOpaqueCount4:
			{
			int count =  code - edsOpaqueCount1;
			int length = *ptr++;

			for (int n = 0; n < count; ++n)
				length = length << 8 | *ptr++;

			value.string.length = length;
			value.string.data = ptr;
			ptr += length;
			type = edsTypeOpaque;
			}
			break;

		case edsDoubleLen0:
		case edsDoubleLen1:
		case edsDoubleLen2:
		case edsDoubleLen3:
		case edsDoubleLen4:
		case edsDoubleLen5:
		case edsDoubleLen6:
		case edsDoubleLen7:
		case edsDoubleLen8:
			{
			union {
				UCHAR chars[8];
				double d;
				} u;

			u.d = 0;
			int count = code - edsDoubleLen0;

#ifdef _BIG_ENDIAN
			for (int n = 0; n < count; ++n)
				u.chars[n] = *ptr++;
#else
			for (int n = 0; n < count; ++n)
				u.chars[7 - n] = *ptr++;
#endif

			value.dbl = u.d;
			type = edsTypeDouble;
			}
			break;

		case edsTimeLen0:
			{
			value.integer64 = 0;
			scale = 0;
			type = edsTypeTime;
			}
			break;

		case edsTimeLen1:
		case edsTimeLen2:
		case edsTimeLen3:
		case edsTimeLen4:
			{
			int l = code - edsTimeLen1;
			INT64 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer64 = val * 1000;
			scale = 0;
			type = edsTypeTime;
			}
			break;

		case edsMilliSecLen0:
			{
			value.integer64 = 0;
			scale = 0;
			type = edsTypeMilliseconds;
			}
			break;

		case edsMilliSecLen1:
		case edsMilliSecLen2:
		case edsMilliSecLen3:
		case edsMilliSecLen4:
		case edsMilliSecLen5:
		case edsMilliSecLen6:
		case edsMilliSecLen7:
		case edsMilliSecLen8:
			{
			int l = code - edsMilliSecLen1;
			INT64 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer64 = val;
			scale = 0;
			type = edsTypeMilliseconds;
			}
			break;

		case edsNanoSecLen0:
			{
			value.integer64 = 0;
			scale = 0;
			type = edsTypeNanoseconds;
			}
			break;

		case edsNanoSecLen1:
		case edsNanoSecLen2:
		case edsNanoSecLen3:
		case edsNanoSecLen4:
		case edsNanoSecLen5:
		case edsNanoSecLen6:
		case edsNanoSecLen7:
		case edsNanoSecLen8:
			{
			int l = code - edsNanoSecLen1;
			INT64 val = (signed char) *ptr++;

			for (int n = 0; n < l; ++n)
				val = (val << 8) | *ptr++;

			value.integer64 = val  / 1000;
			scale = (int) (val % 1000);
			type = edsTypeNanoseconds;
			}
			break;

		case edsClobLen0:
		case edsClobLen1:
		case edsClobLen2:
		case edsClobLen3:
		case edsClobLen4:
			{
			int count = code - edsClobLen0;
			int32 blobId = 0;

			for (int n = 0; n < count; ++n)
				blobId = blobId << 8 | *ptr++;

			value.blobId = blobId;
			type = edsTypeClob;
			}
			break;

		case edsBlobLen0:
		case edsBlobLen1:
		case edsBlobLen2:
		case edsBlobLen3:
		case edsBlobLen4:
			{
			int count = code - edsBlobLen0;
			int blobId = 0;

			for (int n = 0; n < count; ++n)
				blobId = blobId << 8 | *ptr++;

			value.blobId = blobId;
			type = edsTypeBlob;
			}
			break;

		case edsScaledCount1:
			{
			int length = *ptr++ - 1;
			int scale = *(const signed char*)ptr++;
			type = edsTypeBigInt;
			bigInt.clear();
			bigInt.setBytes(scale, length, (const char*) ptr);
			ptr += length;
			}
			break;
			
		default:
			return edsTypeUnknown;
		}

	return type;
}

void EncodedDataStream::skip()
{
	ptr = skip(ptr);
}

void EncodedDataStream::setData(const UCHAR *data)
{
	ptr = data;
}

void EncodedDataStream::encodeNull()
{
	stream->putCharacter(edsNull);
}

void EncodedDataStream::encodeInt(int value, int scale)
{
	int val = value;
	int count = BYTE_COUNT(val);

	if (scale)
		{
		stream->putCharacter(edsScaledLen0 + count);
		stream->putCharacter(scale);
		}
	else if (val >= edsIntMin && val <= edsIntMax)
		{
		stream->putCharacter(edsInt0 + val);

		return;
		}
	else
		stream->putCharacter(edsIntLen1 + count - 1);

	while (--count >= 0)
		stream->putCharacter(val >> (lengthShifts [count]));
}

void EncodedDataStream::encodeInt64(INT64 value, int scale)
{
	INT64 val = value;
	int count = BYTE_COUNT64(val);

	if (scale)
		{
		stream->putCharacter(edsScaledLen0 + count);
		stream->putCharacter(scale);
		}
	else if (val >= edsIntMin && val <= edsIntMax)
		{
		stream->putCharacter((char) (edsInt0 + val));

		return;
		}
	else
		stream->putCharacter(edsIntLen1 + count - 1);

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
}

void EncodedDataStream::encodeUtf8(int length, const char *string)
{
	int len = length;
	
	if (len <= edsUtf8Len39 - edsUtf8Len0)
		stream->putCharacter(edsUtf8Len0 + len);
	else
		{
		int count = BYTE_COUNT(len);
		stream->putCharacter(edsUtf8Count1 + count - 1);

		while (--count >= 0)
			stream->putCharacter(len >> (lengthShifts [count]));
		}

	if (len)
		stream->putSegment(len, string, true);
}

void EncodedDataStream::encodeTimestamp(INT64 nanoseconds)
{
	INT64 val = nanoseconds;
	int count = BYTE_COUNT64(val);
	stream->putCharacter(edsNanoSecLen0 + count);

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
}

void EncodedDataStream::encodeDate(INT64 milliseconds)
{
	INT64 val = milliseconds;
	int count = BYTE_COUNT64(val);
	stream->putCharacter(edsMilliSecLen0 + count);

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
}

void EncodedDataStream::encodeTime(INT64 milliseconds)
{
	INT64 val = milliseconds;
	int count = BYTE_COUNT64(val);
	stream->putCharacter(edsTimeLen0 + count);

	while (--count >= 0)
		stream->putCharacter((char) (val >> (lengthShifts [count])));
}

void EncodedDataStream::setStream(Stream *newStream)
{
	stream = newStream;
}

void EncodedDataStream::encodeDouble(double dbl)
{
	union {
		char chars[8];
		double d;
		} u;

	// Convert negative 0 to 0
		
	u.d = (dbl == 0 ? 0 : dbl);
		
#ifdef _BIG_ENDIAN
	int count = 8;

	while (count && u.chars[count - 1] == 0)
		count--;

	stream->putCharacter(edsDoubleLen0 + count);

	for (int n = 0; n < count; n++)
		stream->putCharacter(u.chars[n]);
#else
	int count = 0;

	while (count < 8 && u.chars[count] == 0)
		++count;

	stream->putCharacter(edsDoubleLen0 + 8 - count);

	for (int n = 7; n >= count; --n)
		stream->putCharacter(u.chars[n]);
#endif
}

void EncodedDataStream::encodeAsciiBlob(int32 blobId)
{
	int count = BYTE_COUNT(blobId);
	stream->putCharacter((char) (edsClobLen0 + count));

	while (--count >= 0)
		stream->putCharacter((char) (blobId >> (lengthShifts [count])));
}

void EncodedDataStream::encodeBinaryBlob(int32 blobId)
{
	int count = BYTE_COUNT(blobId);
	stream->putCharacter((char) (edsBlobLen0 + count));

	while (--count >= 0)
		stream->putCharacter((char) (blobId >> (lengthShifts [count])));
}

void EncodedDataStream::encodeOpaque(int length, const char *string)
{
	int len = length;
	
	if (len <= edsOpaqueLen39 - edsOpaqueLen0)
		stream->putCharacter(edsOpaqueLen0 + len);
	else
		{
		int count = BYTE_COUNT(len);
		stream->putCharacter(edsOpaqueCount1 + count - 1);

		while (--count >= 0)
			stream->putCharacter(len >> (lengthShifts [count]));
		}

	if (len)
		stream->putSegment(len, string, true);
}

void EncodedDataStream::encodeEncoding(const UCHAR *encodedValue)
{
	const UCHAR *p = skip(encodedValue);
	stream->putSegment(p - encodedValue, (const char*) encodedValue, true);
}

INT64 EncodedDataStream::getInt64(int requiredScale)
{
	int deltaScale = scale - requiredScale;
	INT64 val = (type == edsTypeInt32) ? value.integer32 : value.integer64;

	if (deltaScale > 0)
		while (--deltaScale >= 0)
			val /= 10;
	else if (deltaScale < 0)
		while (++deltaScale <= 0)
			val *= 10;

	return val;
}

void EncodedDataStream::encodeBigInt(BigInt* bigInt)
{
	int length = bigInt->getByteLength();
	
	if (false && length <= 8)
		encodeInt64(bigInt->getInt(), bigInt->scale);
	else
		{
		stream->putCharacter((char) edsScaledCount1);
		stream->putCharacter((char) (length + 1));
		stream->putCharacter((char) bigInt->scale);
		char *space = stream->alloc(length);
		bigInt->getBytes(space);
		}
}

void EncodedDataStream::encodeBigInt(Value *value)
{
	BigInt bigInt;
	value->getBigInt(&bigInt);
	int length = bigInt.getByteLength();
	
	if (false && length <= 8)
		encodeInt64(bigInt.getInt(), bigInt.scale);
	else
		{
		stream->putCharacter((char) edsScaledCount1);
		stream->putCharacter((char) (length + 1));
		stream->putCharacter((char) bigInt.scale);
		char *space = stream->alloc(length);
		bigInt.getBytes(space);
		}
}
