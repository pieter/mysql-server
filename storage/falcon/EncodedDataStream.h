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

// EncodedDataStream.h: interface for the EncodedDataStream class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODEDDATASTREAM_H__D9311EE4_7097_4A61_932B_BCC85076722C__INCLUDED_)
#define AFX_ENCODEDDATASTREAM_H__D9311EE4_7097_4A61_932B_BCC85076722C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _WIN32
typedef __int64		INT64;
#else
typedef long long	INT64;
#endif

#include "BigInt.h"

#define BYTES_POS(n)   ((n == 0) ? 0 : \
						(n < (1<<7)) ? 1 : \
						(n < (1<<15)) ? 2 : \
						(n < (1<<23)) ? 3 : 4)

#define BYTES_NEG(n)   ((n >= -(1<<7)) ? 1 : \
						(n >= -(1<<15)) ? 2 : \
						(n >= -(1<<23)) ? 3 : 4)

#define BYTE_COUNT(n)	((n >= 0) ? BYTES_POS(n) : BYTES_NEG(n))

#define BYTES_POS64(n) ((n == 0) ? 0 : \
						(n < (1<<7)) ? 1 : \
						(n < (1<<15)) ? 2 : \
						(n < (1<<23)) ? 3 :\
						(n < ((INT64) 1<<31)) ? 4 :\
						(n < ((INT64) 1<<39)) ? 5 :\
						(n < ((INT64) 1<<47)) ? 6 :\
						(n < ((INT64) 1<<55)) ? 7 : 8)

#define BYTES_NEG64(n) ((n >= -(1<<7)) ? 1 : \
						(n >= -(1<<15)) ? 2 : \
						(n >= -(1<<23)) ? 3 : \
						(n >= -((INT64) 1<<31)) ? 4 : \
						(n >= -((INT64) 1<<39)) ? 5 : \
						(n >= -((INT64) 1<<47)) ? 6 : \
						(n >= -((INT64) 1<<55)) ? 7 : 8)

#define BYTE_COUNT64(n)	((n >= 0) ? BYTES_POS64(n) : BYTES_NEG64(n))

static const int edsVersion = 1;
static const int edsIntMin	= -10;
static const int edsIntMax	= 31;

enum DataStreamType
	{
	edsTypeNull,
	edsTypeUnknown,
	edsTypeInt32,
	edsTypeInt64,
	edsTypeScaled,
	edsTypeUtf8,
	edsTypeOpaque,
	edsTypeDouble,
	edsTypeBlob,				// blob id
	edsTypeClob,				// clob id
	edsTypeTime,				// milliseconds since midnight
	edsTypeMilliseconds,		// milliseconds since January 1, 1970
	edsTypeNanoseconds,			// milliseconds since January 1, 1970
	edsTypeBigInt,
	};
	
enum DataStreamCode
	{
	edsNull			= 1,

	edsIntMinus10	= 10,
	edsIntMinus9,
	edsIntMinus8,
	edsIntMinus7,
	edsIntMinus6,
	edsIntMinus5,
	edsIntMinus4,
	edsIntMinus3,
	edsIntMinus2,
	edsIntMinus1,
	edsInt0,
	edsInt1,
	edsInt2,
	edsInt3,
	edsInt4,
	edsInt5,
	edsInt6,
	edsInt7,
	edsInt8,
	edsInt9,
	edsInt10,
	edsInt11,
	edsInt12,
	edsInt13,
	edsInt14,
	edsInt15,
	edsInt16,
	edsInt17,
	edsInt18,
	edsInt19,
	edsInt20,
	edsInt21,
	edsInt22,
	edsInt23,
	edsInt24,
	edsInt25,
	edsInt26,
	edsInt27,
	edsInt28,
	edsInt29,
	edsInt30,
	edsInt31,

	edsIntLen1,				// signed integer of length 1
	edsIntLen2,
	edsIntLen3,
	edsIntLen4,
	edsIntLen5,
	edsIntLen6,
	edsIntLen7,
	edsIntLen8,

	edsScaledLen0,			// scaled integer of length 0 (aka 0)
	edsScaledLen1,
	edsScaledLen2,
	edsScaledLen3,
	edsScaledLen4,
	edsScaledLen5,
	edsScaledLen6,
	edsScaledLen7,
	edsScaledLen8,

	edsUtf8Count1,			// Utf8 with one count byte
	edsUtf8Count2,
	edsUtf8Count3,
	edsUtf8Count4,

	edsOpaqueCount1,		// Opaque with one count byte
	edsOpaqueCount2,
	edsOpaqueCount3,
	edsOpaqueCount4,

	edsDoubleLen0,			// IEEE double precision floating point
	edsDoubleLen1,	
	edsDoubleLen2,
	edsDoubleLen3,
	edsDoubleLen4,
	edsDoubleLen5,
	edsDoubleLen6,
	edsDoubleLen7,
	edsDoubleLen8,

	edsMilliSecLen0,		// milliseconds since January 1, 1970
	edsMilliSecLen1,
	edsMilliSecLen2,
	edsMilliSecLen3,
	edsMilliSecLen4,
	edsMilliSecLen5,
	edsMilliSecLen6,
	edsMilliSecLen7,
	edsMilliSecLen8,

	edsNanoSecLen0,			// nanoseconds since January 1, 1970
	edsNanoSecLen1,
	edsNanoSecLen2,
	edsNanoSecLen3,
	edsNanoSecLen4,
	edsNanoSecLen5,
	edsNanoSecLen6,
	edsNanoSecLen7,
	edsNanoSecLen8,

	edsTimeLen0,			// milliseconds since midnight
	edsTimeLen1,
	edsTimeLen2,
	edsTimeLen3,
	edsTimeLen4,

	edsUtf8Len0,			// Utf8 string of length 0
	edsUtf8Len1,
	edsUtf8Len2,
	edsUtf8Len3,
	edsUtf8Len4,
	edsUtf8Len5,
	edsUtf8Len6,
	edsUtf8Len7,
	edsUtf8Len8,
	edsUtf8Len9,
	edsUtf8Len10,
	edsUtf8Len11,
	edsUtf8Len12,
	edsUtf8Len13,
	edsUtf8Len14,
	edsUtf8Len15,
	edsUtf8Len16,
	edsUtf8Len17,
	edsUtf8Len18,
	edsUtf8Len19,
	edsUtf8Len20,
	edsUtf8Len21,
	edsUtf8Len22,
	edsUtf8Len23,
	edsUtf8Len24,
	edsUtf8Len25,
	edsUtf8Len26,
	edsUtf8Len27,
	edsUtf8Len28,
	edsUtf8Len29,
	edsUtf8Len30,
	edsUtf8Len31,
	edsUtf8Len32,
	edsUtf8Len33,
	edsUtf8Len34,
	edsUtf8Len35,
	edsUtf8Len36,
	edsUtf8Len37,
	edsUtf8Len38,
	edsUtf8Len39,
	edsUft8LenMax = edsUtf8Len39,

	edsOpaqueLen0,			// Opaque string of length 0
	edsOpaqueLen1,
	edsOpaqueLen2,
	edsOpaqueLen3,
	edsOpaqueLen4,
	edsOpaqueLen5,
	edsOpaqueLen6,
	edsOpaqueLen7,
	edsOpaqueLen8,
	edsOpaqueLen9,
	edsOpaqueLen10,
	edsOpaqueLen11,
	edsOpaqueLen12,
	edsOpaqueLen13,
	edsOpaqueLen14,
	edsOpaqueLen15,
	edsOpaqueLen16,
	edsOpaqueLen17,
	edsOpaqueLen18,
	edsOpaqueLen19,
	edsOpaqueLen20,
	edsOpaqueLen21,
	edsOpaqueLen22,
	edsOpaqueLen23,
	edsOpaqueLen24,
	edsOpaqueLen25,
	edsOpaqueLen26,
	edsOpaqueLen27,
	edsOpaqueLen28,
	edsOpaqueLen29,
	edsOpaqueLen30,
	edsOpaqueLen31,
	edsOpaqueLen32,
	edsOpaqueLen33,
	edsOpaqueLen34,
	edsOpaqueLen35,
	edsOpaqueLen36,
	edsOpaqueLen37,
	edsOpaqueLen38,
	edsOpaqueLen39,
	edsOpaqueLenMax=edsOpaqueLen39,

	edsBlobLen0,
	edsBlobLen1,
	edsBlobLen2,
	edsBlobLen3,
	edsBlobLen4,

	edsClobLen0,
	edsClobLen1,
	edsClobLen2,
	edsClobLen3,
	edsClobLen4,

	edsScaledCount1,			// Scaled binary with single count byte

	};


class Stream;
class Value;
class BigInt;

class EncodedDataStream  
{
public:
	virtual void encodeOpaque(int length, const char *string);
	EncodedDataStream();
	EncodedDataStream (Stream *stream);
	EncodedDataStream(const unsigned char *data);
	virtual ~EncodedDataStream();

	virtual void	encodeDouble (double dbl);
	virtual void	encodeTime (INT64 milliseconds);
	virtual void	encodeDate (INT64 milliseconds);
	virtual void	encodeTimestamp (INT64 nanoseconds);
	virtual void	encodeUtf8(int length, const char *string);
	virtual void	encodeInt64 (INT64 value, int scale=0);
	virtual void	encodeInt (int value, int scale=0);
	virtual void	encodeNull();
	virtual void	encodeBinaryBlob (int32 blobId);
	virtual void	encodeAsciiBlob (int32 blobId);
	virtual void	encodeBigInt(BigInt* bigInt);
	virtual void	encodeBigInt(Value *value);
	virtual void	skip();
	virtual void	unsupported (Value *value);
	virtual void	encodeBinaryBlob (Value *value);
	virtual void	encodeAsciiBlob (Value *value);
	virtual void	encodeEncoding (const unsigned char *encodedValue);
	virtual INT64	getInt64 (int requiredScale);
	virtual DataStreamType		decode();

	void			setStream (Stream *newStream);
	void			setData (const unsigned char *data);
	void			encode (int type, Value *value);
	
	static int		init(void);
	static const unsigned char* decode (const unsigned char *ptr, Value *value, bool copyFlag);

	inline static const unsigned char* skip (const unsigned char *ptr)
		{
		int length = lengths[*ptr++];
		
		if (length < 0)
			{
			int count = 0;

			for (; length < 0; ++length)
				count = (count << 8) + *ptr++;

			length = count;
			}
		
		return ptr + length;
		}

	inline int32 getInt32()
		{
		return (type == edsTypeInt32) ? value.integer32 : (int32) value.integer64;
		}
	
	inline INT64 getInt64()
		{
		return (type == edsTypeInt32) ? value.integer32 : value.integer64;
		}

	Stream				*stream;
	const unsigned char	*ptr;
	DataStreamType		type;
	int					scale;
	BigInt				bigInt;
	static signed char	lengths[256];

	union DecodedValue
		{
		int32	integer32;
		int32	blobId;
		INT64	integer64;
		double	dbl;
		struct
			{
			uint32				length;
			const unsigned char	*data;
			}	string;
		} value;

public:
};

#endif // !defined(AFX_ENCODEDDATASTREAM_H__D9311EE4_7097_4A61_932B_BCC85076722C__INCLUDED_)
