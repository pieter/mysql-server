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

// Value.h: interface for the Value class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#if !defined(AFX_VALUE_H__02AD6A4B_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_VALUE_H__02AD6A4B_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Types.h"
#include "TimeStamp.h"
#include "Stream.h"
#include "BigInt.h"
#include "Blob.h"

class Blob;
class Clob;

class Value  
{
public:
	Blob*		getBlob();
	Value (const char *string);

	short		getShort(int scale = 0);
	int			getInt(int scale = 0);
	int64		getQuad(int scale = 0);
	double		getDouble();
	int			getTruncatedString(int bufferSize, char* buffer);
	void		getBigInt(BigInt *bigInt);
	Clob*		getClob();
	TimeStamp	getTimestamp();
	Time		getTime();
	DateTime	getDate();
	void		getStream (Stream *stream, bool copyFlag);

	const char	*getString();
	int			getString (int bufferSize, char *buffer);
	const char* getString (char **tempPtr);
	int			getStringLength();

	void		setValue (double value);
	void		setValue (int32, int scale = 0);
	void		setValue (Value *value, bool copyFlag);
	void		setValue(Blob * blb);
	void		setValue (Clob *blob);
	void		setValue (Type type, Value *value);
	void		setValue (Time value);
	void		setValue (BigInt *value);
	void		setValue (int64 value, int scale = 0);
	void		setValue (short value, int scale = 0);
	void		setValue (DateTime value);
	void		setValue (TimeStamp value);

	void		setString (int length, const char *string, bool copy);
	void		setString (const char *value, bool copy);
	void		setString (const WCString *value);

	void		setNull();
	void		setBinaryBlob (int32 blobId);
	void		setAsciiBlob (int32 blobId);
	void		setDate (int32 value);
	BigInt*		setBigInt(void);
	
	int			compare (Value *value);
	int			compareBlobs (Blob *blob1, Blob *blob2);
	int			compareClobs (Clob *clob1, Clob *clob2);

	bool		isNumber();
	
	void		multiply (Value *value);
	void		subtract (Value* value);
	int			getScale();
	int			convert(int64 value, int scale, char *string);
	char		getByte (int scale = 0);
	void		divide (Value *value);
	void		add (int value);
	void		add (Value *value);
	bool		isNull (Type type);
	char*		allocString (Type typ, int length);
	int64		convertToQuad (double& divisor);
	void		truncateString(int maxLength);

	static double	scaleDouble (double d, int scale);
	static double	convertToDouble (const char *string);
	static double	convertToDouble (const char *string, int length);
	static int64	convertToQuad (const char *string, int length, double& divisor);
	static int64	reScale (int64 number, int from, int to);

	inline void clear()
		{
		switch (type)
			{
			case String:
			case Char:
			case Varchar:
				if (copyFlag && data.string.string)
					{
					delete [] data.string.string;
					data.string.string = NULL;
					}
				break;
			
			case BlobPtr:
				data.blob->release();
				break;
			
			case ClobPtr:
				data.clob->release();
				break;
			
			case Biginteger:
				delete data.bigInt;
				break;

			default:
				break;
			}

		type = Null;
		}

	inline Type getType()
		{
		return type;
		}
	
	inline bool isNull()
		{
		return type == Null;
		}
	
	inline int getBlobId()
		{
		return data.integer;
		}

	Value();
	virtual ~Value();

protected:
	Type		type;
	bool		copyFlag;
	char		scale;

	union
		{
		struct
			{
			char	*string;
			int		length;
			}	string;
		short		smallInt;
		int32		integer;
		double		dbl;
		int64		quad;
		Blob		*blob;
		Clob		*clob;
		BigInt		*bigInt;
		DateTime	date;
		TimeStamp	timestamp;
		Time		time;
		} data;
public:
	int64 getSeconds(void);
	int64 getMilliseconds(void);
	int getNanos(void);
	BigInt* getBigInt(void);
};

#endif // !defined(AFX_VALUE_H__02AD6A4B_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
