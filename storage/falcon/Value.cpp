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

// Value.cpp: implementation of the Value class.
//
//////////////////////////////////////////////////////////////////////


// copyright (c) 1999 - 2000 by James A. Starkey

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Engine.h"
#include "Value.h"
#include "SQLError.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "Unicode.h"

#define DECIMAL_POINT		'.'
#define DIGIT_SEPARATOR		','

#define BACKWARDS

static const double powersOfTen [] = {
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#undef NOT_YET_IMPLEMENTED
#define NOT_YET_IMPLEMENTED throw SQLEXCEPTION (FEATURE_NOT_YET_IMPLEMENTED, "conversion is not implemented");

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Value::Value()
{
	type = Null;
}

Value::~Value()
{
	clear();
}

void Value::setValue (int32 number, int scl)
{
	clear();
	type = Int32;
	scale = scl;
	data.integer = number;
}

void Value::setString(const char * string, bool copy)
{
	clear();

	if (!string)
		return;

	type = String;
	copyFlag = copy;
	data.string.length = (int) strlen(string);

	if (copy)
		{
		data.string.string = new char [data.string.length + 1];
		strcpy(data.string.string, string);
		}
	else
		data.string.string = (char*) string;
}

Value::Value(const char * string)
{
	type = Null;
	setString(string, true);
}

void Value::setValue(Value * value, bool copyFlag)
{
	switch (value->type)
		{
		case String:
		case Char:
		case Varchar:
			setString(value->data.string.length, value->data.string.string, copyFlag);
			return;

		default:
			break;
		}

	clear();

	switch (value->type)
		{
		case Null:
			break;

		case Short:
			scale = value->scale;
			data.smallInt = value->data.smallInt;
			break;

		case Int32:
			scale = value->scale;
			data.integer = value->data.integer;
			break;

		case Int64:
			scale = value->scale;
			data.quad = value->data.quad;
			break;

		case Double:
			data.dbl = value->data.dbl;
			scale = 0;
			break;

		case BlobPtr:
			data.blob = value->data.blob;
			data.blob->addRef();
			break;

		case ClobPtr:
			data.clob = value->data.clob;
			data.clob->addRef();
			break;

		case Date:
			data.date = value->data.date;
			break;

		case Timestamp:
			data.timestamp = value->data.timestamp;
			break;

		case TimeType:
			data.time = value->data.time;
			break;

		case Biginteger:
			data.bigInt = new BigInt(value->data.bigInt);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	type = value->type;
}

void Value::setString(int length, const char * string, bool copy)
{
	clear();

	if (length < 0)
		throw SQLEXCEPTION(RUNTIME_ERROR, "invalid string length (<0)");

	if (!string)
		return;

	type = String;
	copyFlag = copy;
	data.string.length = length;

	if (copy)
		{
		data.string.string = new char [length + 1];
		memcpy(data.string.string, string, length);
		data.string.string [length] = 0;
		}
	else
		data.string.string = (char*) string;
}


void Value::setString(const WCString *value)
{
	clear();

	if (!value)
		return;

	if (value->count < 0)
		throw SQLEXCEPTION(RUNTIME_ERROR, "invalid wstring length (<0)");

	type = String;
	copyFlag = true;
	data.string.length = Unicode::getUtf8Length(value->count, value->string);

	char *p = data.string.string = new char [data.string.length + 1];
	Unicode::convert(value->count, value->string, p);
	p[data.string.length] = 0;
}

const char* Value::getString()
{
	switch (type)
		{
		case Null:
			return "";

		case String:
		case Char:
		case Varchar:
			return data.string.string;

		default:
			NOT_YET_IMPLEMENTED;
		}

	return "";
}

int Value::getString(int bufferSize, char * buffer)
{
	switch (type)
		{
		case String:
		case Char:
		case Varchar:
			if (data.string.length > bufferSize)
				throw SQLEXCEPTION(TRUNCATION_ERROR, "string truncation of %d bytes into %d bytes from \"%.*s\"",
									data.string.length, bufferSize, data.string.length, data.string.string);

			memcpy(buffer, data.string.string, data.string.length);
			return data.string.length;

		case Date:
			return data.date.getString(bufferSize, buffer);

		case Timestamp:
			return data.timestamp.getString(bufferSize, buffer);

		case Null:
			buffer [0] = 0;
			return 0;

		case Short:
			if (bufferSize < 6)
				throw SQLEXCEPTION(TRUNCATION_ERROR, "string truncation during conversion");

			return convert(data.smallInt, scale, buffer);

		case Int32:
			if (bufferSize < 11)
				throw SQLEXCEPTION(TRUNCATION_ERROR, "string truncation during conversion");

			return convert(data.integer, scale, buffer);

		case Double:
			if (bufferSize < 23)
				throw SQLEXCEPTION(TRUNCATION_ERROR, "string truncation during conversion");

			sprintf(buffer, "%f", data.dbl);
			return (int) strlen(buffer) + 1;

		case Int64:
			if (bufferSize < 23)
				throw SQLEXCEPTION(TRUNCATION_ERROR, "string truncation during conversion");

			return convert(data.quad, scale, buffer);

		case BlobPtr:
			data.blob->getBytes(0, bufferSize, buffer);
			break;

		case ClobPtr:
			data.clob->getSubString(0, bufferSize, buffer);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	return -1;
}

double Value::getDouble()
{
	switch (type)
		{
		case Null:
			return 0;

		case Double:
			return data.dbl;

		case Short:
			return scaleDouble(data.smallInt, scale);

		case Int32:
			return scaleDouble(data.integer, scale);

		case Int64:
			return scaleDouble((double) data.quad, scale);

		case Char:
		case Varchar:
		case String:
			break;

		case Date:
			return (double) data.date.getSeconds();

		case Timestamp:
			return (double) data.time.getSeconds() +
				   (double) data.timestamp.getNanos();

		case TimeType:
			return (double) data.time.getSeconds();

		case Biginteger:
			return data.bigInt->getDouble();

		default:
			NOT_YET_IMPLEMENTED;
		}

	double divisor;
	int64 number = convertToQuad(divisor);

	return number / divisor;
}

int Value::compare(Value * value)
{
	// If the values are the same type, everything is easy

	if (type == value->type)
		switch (type)
			{
			case Null:
				return 0;

			case Short:
				if (scale != value->scale)
					break;

				return data.smallInt - value->data.smallInt;

			case Int32:
				if (scale != value->scale)
					break;

				return data.integer - value->data.integer;

			case Double:
				if (data.dbl > value->data.dbl)
					return 1;

				if (data.dbl < value->data.dbl)
					return -1;

				return 0;

			case Int64:
				if (scale != value->scale)
					break;

				if (data.quad > value->data.quad)
					return 1;

				if (data.quad < value->data.quad)
					return -1;
					
				return 0;

			/*** fall through is just fine
			case BlobPtr:
				{
				Blob *blob1 = getBlob();
				Blob *blob2 = value->getBlob();
				int result = compareBlobs(blob1, blob2);
				blob1->release();
				blob2->release();
				return result;
				}
			***/

			case ClobPtr:
				{
				Clob *clob1 = getClob();
				Clob *clob2 = value->getClob();
				int result = compareClobs(clob1, clob2);
				clob1->release();
				clob2->release();

				return result;
				}

			case Biginteger:
				return data.bigInt->compare(value->data.bigInt);

			default:
				break;
			}

	// If either is null, everything is also easy

	if (type == Null)
		return -1;

	if (value->type == Null)
		return 1;

	switch (MAX(type, value->type))
		{
		case Null:
			return 0;

		case String:
		case Char:
		case Varchar:
			{
			const UCHAR *p = (const UCHAR*) data.string.string;
			const UCHAR *q = (const UCHAR*) value->data.string.string;
			int l1 = data.string.length;
			int l2 = value->data.string.length;
			int l = MIN(l1, l2);
			int n;

			for (n = 0; n < l; ++n)
				{
				int c = *p++ - *q++;

				if (c)
					return c;
				}

			int c;

			if (n < l1)
				{
				for (; n < l1; ++n)
					if ( (c = *p++ - ' ') )
						return c;

				return 0;
				}

			if (n < l2)
				{
				for (; n < l2; ++n)
					if ( (c = ' ' - *q++) )
						return c;
				}

			return 0;
			}

		case Double:
		case Float:
			return (int) (getDouble() - value->getDouble());

		case Int64:
			{
			int s1 = getScale();
			int s2 = value->getScale();
			int maxScale = MAX(s1, s2);

			return (int) (getQuad(maxScale) - value->getQuad(maxScale));
			}

		case Short:
		case Int32:
			{
			int s1 = getScale();
			int s2 = value->getScale();
			int maxScale = MAX(s1, s2);

			return (int) (getInt(maxScale) - value->getInt(maxScale));
			}

		case Date:
			return getDate().compare(value->getDate());

		case Timestamp:
			return getTimestamp().compare(value->getTimestamp());

		case TimeType:
			return getTime().compare(value->getTime());

		case ClobPtr:
			{
			char *temp1 = NULL;
			char *temp2 = NULL;
			const char *string1 = getString(&temp1);
			const char *string2 = value->getString (&temp2);
			int result = strcmp(string1, string2);
			delete [] temp1;
			delete [] temp2;

			return result;
			}

		case BlobPtr:
			{
			Blob *blob1 = getBlob();
			Blob *blob2 = value->getBlob();
			int result = compareBlobs(blob1, blob2);
			blob1->release();
			blob2->release();

			return result;
			}

		case Biginteger:
			{
			BigInt temp;

			if (type == Biginteger)
				{
				value->getBigInt(&temp);

				return data.bigInt->compare(&temp);
				}

			getBigInt(&temp);

			return temp.compare(value->data.bigInt);
			}

		default:
			NOT_YET_IMPLEMENTED;
			return 0;
		}

}

void Value::setValue(double value)
{
	clear();
	type = Double;
	data.dbl = (value == 0 ? 0 : value);
	scale = 0;
}

int64 Value::getQuad(int scl)
{
	int64 number;
	int s = 0;

	switch (type)
		{
		case Null:
			return 0;

		case Short:
			number = data.smallInt;
			s = scale;
			break;

		case Int32:
			number = data.integer;
			s = scale;
			break;

		case Int64:
			number = data.quad;
			s = scale;
			break;

		case Double:
			{
			double d = data.dbl;

			if (scl)
				d = scaleDouble(d, -scl);
			/***
			if (scl > 0)
				for (int n = 0; n < scl; ++n)
					d *= 10;
			else if (scl < 0)
				for (int n = 0; n > scl; --n)
					d /= 10;
			***/
			
			if (d > 0)
				d += 0.5;
			else if (d < 0)
				d -= 0.5;

			return (int64) d;
			}

		case String:
		case Char:
		case Varchar:
			{
			number = 0;
			bool neg = false;
			int dec = 0;

			for (int n = 0; n < data.string.length; ++n)
				{
				char c = data.string.string [n];

				if (c >= '0' && c <= '9')
					{
					number = number * 10 + c - '0';
					s += dec;
					}
				else if (c == '-')
					neg = true;
				else if (c == '.')
					dec = 1;
				}

			if (neg)
				number = -number;
			}
			break;

		default:
			throw SQLError(RUNTIME_ERROR, "conversion not defined");
		}

	if (s == scl)
		return number;

	return reScale(number, s, scl);
}

int Value::getInt(int scl)
{
	int number;
	int s = 0;

	switch (type)
		{
		case Null:
			return 0;

		case Short:
			number = data.smallInt;
			s = scale;
			break;

		case Int32:
			number = data.integer;
			s = scale;
			break;

		case Int64:
			number = (int32) data.quad;
			s = scale;
			break;

		case Double:
			{
			double d = data.dbl;
			s = scl - scale;

			if (s > 0)
				for (int n = 0; n < s; ++n)
					d *= 10;
			else if (s < 0)
				for (int n = 0; n > s; --n)
					d /= 10;

			if (d > 0)
				d += 0.5;
			else if (d < 0)
				d -= 0.5;

			return (int32) d;
			}

		case String:
		case Char:
		case Varchar:
			{
			number = 0;
			bool neg = false;
			int dec = 0;

			for (int n = 0; n < data.string.length; ++n)
				{
				char c = data.string.string [n];

				if (c >= '0' && c <= '9')
					{
					number = number * 10 + c - '0';
					s += dec;
					}
				else if (c == '-')
					neg = true;
				else if (c == '.')
					dec = 1;
				}

			if (neg)
				number = -number;
			}
			break;

		default:
			throw SQLError(RUNTIME_ERROR, "conversion not defined");
		}

	if (s == scl)
		return number;

	return (int32) reScale(number, s, scl);
}

short Value::getShort(int scl)
{
	switch (type)
		{
		case Short:
			if (scale == scl)
				return data.smallInt;

			break;

		case Int32:
			if (scale == scl)
				return (short) data.integer;

			break;

		default:
			break;
		}

	return (short) getQuad(scl);
}

const char* Value::getString(char **tempPtr)
{
	char	temp [128];

	if (tempPtr)
		*tempPtr = NULL;

	switch (type)
		{
		case Null:
			return "";

		case String:
		case Char:
			{
			if (data.string.string [data.string.length] == 0)
				return data.string.string;

			*tempPtr = new char [data.string.length + 1];
			memcpy(*tempPtr, data.string.string, data.string.length);
			(*tempPtr)[data.string.length] = 0;

			return *tempPtr;
			}

		case Short:
			convert(data.smallInt, scale, temp);
			break;

		case Int32:
			convert(data.integer, scale, temp);
			break;

		case Double:
			sprintf(temp, "%f", data.dbl);
			break;

		case Int64:
			convert(data.quad, scale, temp);
			break;

		case Date:
			data.date.getString(sizeof(temp), temp);
			break;

		case TimeType:
			data.time.getString(sizeof(temp), temp);
			break;

		case Timestamp:
			data.timestamp.getString(sizeof(temp), temp);
			break;

		case BlobPtr:
			{
			if (*tempPtr)
				delete [] *tempPtr;

			int length = data.blob->length();

			if (length < 0)
				throw SQLError(LOST_BLOB, "repository blob has been lost");

			*tempPtr = new char [length + 1];
			data.blob->getBytes(0, length, *tempPtr);
			(*tempPtr) [length] = 0;

			return *tempPtr;
			}

		case ClobPtr:
			{
			if (*tempPtr)
				delete [] *tempPtr;

			int length = data.clob->length();

			if (length < 0)
				throw SQLError(LOST_BLOB, "repository blob has been lost");

			*tempPtr = new char [length + 1];
			data.clob->getSubString(0, length, *tempPtr);
			(*tempPtr) [length] = 0;

			return *tempPtr;
			}

		case Biginteger:
			data.bigInt->getString(sizeof(temp), temp);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	UIPTR length = strlen(temp);

	if (*tempPtr)
		delete *tempPtr;

	*tempPtr = new char [length + 1];
	strcpy(*tempPtr, temp);

	return *tempPtr;
}

Blob* Value::getBlob()
{
	BinaryBlob *blob;

	switch (type)
		{
		case Null:
			return new BinaryBlob;

		case BlobPtr:
			data.blob->addRef();
			return data.blob;

		case ClobPtr:
			return new BinaryBlob(data.clob);

		case String:
			blob = new BinaryBlob;
			blob->putSegment(data.string.length, data.string.string, false);
			return blob;

		default:
			NOT_YET_IMPLEMENTED;
			return NULL;
		}

}


Clob* Value::getClob()
{
	AsciiBlob *clob;

	switch (type)
		{
		case Null:
			return new AsciiBlob;

		case ClobPtr:
			data.clob->addRef();
			return data.clob;

		case BlobPtr:
			return new AsciiBlob(data.blob);

		case String:
			clob = new AsciiBlob;
			clob->putSegment(data.string.length, data.string.string, false);
			return clob;

		default:
			NOT_YET_IMPLEMENTED;
			return NULL;
		}
}

void Value::getStream(Stream * stream, bool copyFlag)
{
	switch (type)
		{
		case Null:
			break;

		case Char:
		case Varchar:
		case String:
			stream->putSegment(data.string.length, data.string.string, copyFlag);
			break;

		case ClobPtr:
			stream->putSegment(data.clob);
			break;

		case BlobPtr:
			stream->putSegment(data.blob);
			break;

		default:
			{
			char temp [128];
			int length = getString(sizeof(temp), temp);
			stream->putSegment(length, temp, true);
			}
		}
}

void Value::setValue(Blob * blb)
{
	clear();
	type = BlobPtr;
	data.blob = blb;
	data.blob->addRef();
}


int64 Value::convertToQuad(double & divisor)
{
	return convertToQuad(data.string.string, data.string.length, divisor);
}

int64 Value::convertToQuad(const char *string, int length, double &divisor)
{
	int64 number = 0;
	divisor = 1;
	bool decimal = false;
	bool negative = false;

	for (const char *p = string, *end = p + length; p < end;)
		{
		char c = *p++;

		if (c >= '0' && c <= '9')
			{
			number = number * 10 + c - '0';

			if (decimal)
				divisor *= 10;
			}
		else if (c == '-')
			negative = true;
		else if (c == '+' || c == DIGIT_SEPARATOR)
			;
		else if (c == DECIMAL_POINT)
			decimal = true;
		else if (c != ' ' && c != '\t' && c != '\n')
			throw SQLEXCEPTION(CONVERSION_ERROR, "error converting to numeric from '%*s'",
									length, string);
		}

	return (negative) ? -number : number;
}

DateTime Value::getDate()
{
	switch (type)
		{
		case Char:
		case String:
		case Varchar:
			break;

		case Date:
			return data.date;

		case Null:
			{
			DateTime date;
			date.setSeconds(0);
			return date;
			}

		case Int32:
			{
			DateTime date;
			date.setSeconds(data.integer);
			return date;
			}

		case Timestamp:
			//return data.timestamp.getDate();
			return data.timestamp;

		case Int64:
			{
			DateTime date;
			date.setMilliseconds(data.quad);
			return date;
			}

		default:
			NOT_YET_IMPLEMENTED;
		}

	return DateTime::convert(data.string.string, data.string.length);
}


TimeStamp Value::getTimestamp()
{
	if (type == Timestamp)
		return data.timestamp;

	TimeStamp timestamp;
	timestamp = getDate();

	return timestamp;
}

void Value::setValue(DateTime value)
{
	clear();
	type = Date;
	data.date = value;
}

void Value::setDate(int32 value)
{
	clear();
	type = Date;
	data.date.setSeconds(value);
}

char* Value::allocString(Type typ, int length)
{
	if (length < 0)
		throw SQLEXCEPTION(RUNTIME_ERROR, "invalid allocString length (<0)");

	clear();
	type = typ;
	data.string.length = length;
	data.string.string = new char [length + 1];
	data.string.string [length] = 0;

	return data.string.string;
}

void Value::setValue(short value, int scl)
{
	clear();
	type = Short;
	scale = scl;
	data.smallInt = value;
}

void Value::setValue(int64 value, int scl)
{
	clear();
	type = Int64;
	scale = scl;
	data.quad = value;
}
void Value::setNull()
{
	clear();
}


bool Value::isNull(Type conversionType)
{
	if (type == Null)
		return true;

	if (conversionType == Date)
		switch (type)
			{
			case Char:
			case String:
			case Varchar:
				if (data.string.length == 0)
					return true;

				break;

			default:
				break;
			}

	return false;
}

void Value::add(Value * value)
{
	Type maxType = MAX(type, value->type);
	int s1 = getScale();
	int s2 = value->getScale();
	int maxScale = MAX(s1, s2);

	if (Null || value->type == Null)
		{
		clear();

		return;
		}

	switch (maxType)
		{
		/***
		case Short:
		case Int32:
			setValue(getInt(maxScale) + value->getInt(maxScale), maxScale);
			break;
		***/

		case Float:
		case Double:
			setValue(getDouble() + value->getDouble());
			break;

		case Char:
		case String:
		case Varchar:
		case Short:
		case Int32:
		case Int64:
			setValue(getQuad(maxScale) + value->getQuad(maxScale), maxScale);
			break;

		case Date:
			{
			DateTime date = getDate();
			int64 incr = value->getQuad(0);
			//date.setSeconds(date.getSeconds() + incr * 60 * 60 * 24);
			date.add(incr * 60 * 60 * 24);
			setValue(date);
			}
			break;

		case Timestamp:
			{
			TimeStamp date = getTimestamp();
			int64 incr = value->getQuad(0);
			//date.setSeconds(date.getSeconds() + incr * 60 * 60 * 24);
			date.add(incr * 60 * 60 * 24);
			setValue(date);
			}
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}


void Value::subtract(Value *value)
{
	Type maxType = MAX(type, value->type);
	int s1 = getScale();
	int s2 = value->getScale();
	int maxScale = MAX(s1, s2);

	if (Null || value->type == Null)
		{
		clear();

		return;
		}

	switch(maxType)
		{
		/***
		case Short:
		case Int32:
			setValue(getInt(maxScale) - value->getInt(maxScale), maxScale);
			break;
		***/

		case Float:
		case Double:
			setValue(getDouble() - value->getDouble());
			break;

		case Char:
		case String:
		case Varchar:
		case Short:
		case Int32:
		case Int64:
			setValue(getQuad(maxScale) - value->getQuad(maxScale), maxScale);
			break;

		case Date:
			{
			DateTime date = getDate();
			int64 incr = value->getQuad(0);
			//date.setSeconds(date.getSeconds() - incr * 60 * 60 * 24);
			date.add(- incr * 60 * 60 * 24);
			setValue(date);
			}
			break;

		case Timestamp:
			{
			TimeStamp date = getTimestamp();

			if (value->isNumber())
				{
				int64 incr = value->getQuad(0);
				date.setSeconds(date.getSeconds() - incr * 60 * 60 * 24);
				setValue(date);
				}
			else
				{
				TimeStamp origin = value->getTimestamp();
				double duration = (double) (date.getSeconds() - origin.getSeconds());
				setValue(duration / (24 * 60 * 60));
				}
			}
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}


void Value::multiply(Value *value)
{
	if (Null || value->type == Null)
		{
		clear();
		return;
		}

	Type maxType = MAX(type, value->type);
	int s1 = getScale();
	int s2 = value->getScale();

	switch (maxType)
		{
		case Float:
		case Double:
			setValue(getDouble() * value->getDouble());
			break;

		case Short:
		case Int32:
		case Char:
		case String:
		case Varchar:
		case Int64:
			setValue(getQuad(s1) * value->getQuad(s2), s1 + s2);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}

void Value::divide(Value * value)
{
	if (Null || value->type == Null)
		{
		clear();
		return;
		}

	int s1 = getScale();
	int s2 = value->getScale();

	switch (type)
		{
		case Short:
		case Int32:
		case Int64:
		case Char:
		case String:
		case Varchar:
			{
			int64 divisor = value->getQuad(s2);

			if (divisor == 0)
				throw SQLEXCEPTION(RUNTIME_ERROR, "integer divide by zero");

			int64 quotient = getQuad(s1) / divisor;
			setValue(reScale(quotient, s1 - s2, s1), s1);
			}
			break;

		case Float:
		case Double:
			{
			double divisor = value->getInt();

			if (divisor == 0)
				throw SQLEXCEPTION(RUNTIME_ERROR, "integer divide by zero");

			setValue(getDouble() / divisor);
			}
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}

void Value::add(int value)
{
	setValue((int) (getInt() + value));
}


char Value::getByte(int scale)
{
	switch (type)
		{
		case Short:
			return (char) data.smallInt;

		case Int32:
			return (char) data.integer;

		case Int64:
		default:
			return (char) getQuad();
		}

	return 0;
}

void Value::setValue(TimeStamp value)
{
	clear();
	type = Timestamp;
	data.timestamp = value;
}

Time Value::getTime()
{
	switch (type)
		{
		case Char:
		case String:
		case Varchar:
			break;

		case TimeType:
			return data.time;

		case Null:
			{
			Time date;
			date.setSeconds(0);

			return date;
			}

		case Int32:
			{
			Time date;
			date.setSeconds(data.integer);

			return date;
			}

		default:
			NOT_YET_IMPLEMENTED;
		}

	return Time::convert(data.string.string, data.string.length);
}

void Value::setValue(Clob * blob)
{
	clear();
	type = ClobPtr;
	data.clob = blob;
	data.clob->addRef();
}

int Value::convert(int64 value, int scale, char *string)
{
	int64 number = value;

	if (number == 0)
		{
		strcpy (string, "0");

		return 1;
		}

	if (scale < -18)
		{
		strcpy(string, "***");

		return sizeof("***");
		}

	bool negative = false;

	if (number < 0)
		{
		number = -number;
		negative = true;
		}

	char temp [100], *p = temp;
	int n;
	int digits = 0;

	for (n = 0; number; number /= 10, --n)
		{
		if (scale && scale == digits++)
			*p++ = '.';

		*p++ = '0' + (char) (number % 10);
		}

	if (scale && digits <= scale)
		{
		for (; digits < scale; ++digits)
			*p++ = '0';

		*p++ = '.';
		}

	char *q = string;

	if (negative)
		*q++ = '-';

	while (p > temp)
		*q++ = *--p;

	*q = 0;

	return (int) (q - string);
}

int Value::compareClobs(Clob *blob1, Clob *blob2)
{
	for (int offset = 0;;)
		{
		int length1 = blob1->getSegmentLength(offset);
		int length2 = blob2->getSegmentLength(offset);

		if (length1 == 0)
			{
			if (length2)
				return -1;
			else
				return 0;
			}

		if (length2 == 0)
			return 1;

		int length = MIN(length1, length2);
		const char *p1 = blob1->getSegment(offset);
		const char *p2 = blob2->getSegment(offset);

		for (const char *end = p1 + length; p1 < end;)
			{
			int n = *p1++ - *p2++;

			if (n)
				return n;
			}

		offset += length;
		}
}

int Value::compareBlobs(Blob *blob1, Blob *blob2)
{
	for (int offset = 0;;)
		{
		int length1 = blob1->getSegmentLength(offset);
		int length2 = blob2->getSegmentLength(offset);

		if (length1 == 0)
			{
			if (length2)
				return -1;
			else
				return 0;
			}

		if (length2 == 0)
			return 1;

		int length = MIN(length1, length2);
		const char *p1 = (const char*) blob1->getSegment(offset);
		const char *p2 = (const char*) blob2->getSegment(offset);

		for (const char *end = p1 + length; p1 < end;)
			{
			int n = *p1++ - *p2++;

			if (n)
				return n;
			}

		offset += length;
		}
}

int Value::getScale()
{
	switch (type)
		{
		case Short:
		case Int32:
		case Int64:
			return scale;

		case String:
		case Char:
		case Varchar:
			break;

		default:
			return 0;
		}

	int scale = 0;
	bool point = false;

	for (int n = 0; n < data.string.length; ++n)
		{
		char c = data.string.string [n];

		if (point && c >= '0' && c <= '9')
			++scale;
		else if (c == '.')
			point = true;
		}

	return scale;
}


int64 Value::reScale(int64 number, int from, int to)
{
	int delta = to - from;

	if (delta > 0)
		for (int n = 0; n < delta; ++n)
			number *= 10;
	else if (delta < 0)
		for (int n = 0; n > delta; --n)
			number /= 10;

	return number;
}


double Value::convertToDouble(const char *string, int length)
{
	double divisor;
	int64 number = convertToQuad(string, length, divisor);

	return number / divisor;
}

double Value::convertToDouble(const char *string)
{
	return convertToDouble(string, (int) strlen(string));
}

int Value::getStringLength()
{
	switch (type)
		{
		case Char:
		case String:
		case Varchar:
			return data.string.length;

		case Null:
			return 0;

		case Short:
			return 6;

		case Int32:
			return 11;

		case Int64:
		case Double:
			return 23;

		case Date:
			return 10;

		case Timestamp:
			return 16;

		case BlobPtr:
			return data.blob->length();

		case ClobPtr:
			return data.clob->length();

		default:
			return 32;
		}

}

double Value::scaleDouble(double d, int scale)
{
	int n = scale;
	
	/***
	if (scale > 0)
		for (int n = 0; n < scale; ++n)
			d /= 10;
	else if (scale < 0)
		for (int n = 0; n > scale; --n)
			d *= 10;
	***/
	
	if (scale > 0)
		{
		for (; n > 10; n -= 10)
			d /= powersOfTen[10];
		
		d /= powersOfTen[n];
		}
	else if (scale < 0)
		{
		for (; n < -10; n += 10)
			d *= powersOfTen[10];
		
		d *= powersOfTen[-n];
		}

	return d;
}

bool Value::isNumber()
{
	switch (type)
		{
		case Short:
		case Int32:
		case Float:
		case Double:
		case Int64:
			return true;

		case Char:
		case String:
		case Varchar:
			{
			for (int n = 0; n < data.string.length; ++n)
				{
				char c = data.string.string [n];

				if (c != ' ' && c != '.' && !(c >= '0' && c <= '9'))
					return false;
				}

			return true;
			}

		default:
			return false;
		}
}

void Value::setValue(Time value)
{
	clear();
	type = TimeType;
	data.time = value;
}

void Value::setValue(Type toType, Value *value)
{
	clear();

	switch (toType)
		{
		case Null:
			break;

		case Char:
		case Varchar:
			{
			char *temp;
			setString(value->getString(&temp), true);
			delete [] temp;

			return;
			}

		case Short:
			scale = value->getScale();
			data.integer = value->getShort(scale);
			break;

		case Int32:
			scale = value->getScale();
			data.integer = value->getInt(scale);
			break;

		case Int64:
			scale = value->getScale();
			data.quad = value->getQuad(scale);
			break;

		case Double:
			data.dbl = value->getDouble();
			scale = 0;
			break;

		/***
		case BlobPtr:
			data.blob = value->data.blob;
			data.blob->addRef();
			break;

		case ClobPtr:
			data.clob = value->data.clob;
			data.clob->addRef();
			break;
		***/

		case Date:
			data.date = value->getDate();
			break;

		case Timestamp:
			data.timestamp = value->getTimestamp();
			break;

		case TimeType:
			data.time = value->getTime();
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	type = toType;
}

void Value::setAsciiBlob(int32 blobId)
{
	clear();
	type = Asciiblob;
	data.integer = blobId;

}

void Value::setBinaryBlob(int32 blobId)
{
	clear();
	type = Binaryblob;
	data.integer = blobId;
}

void Value::setValue(BigInt *value)
{
	clear();
	type = Biginteger;
	data.bigInt = new BigInt;
	data.bigInt->set(value);
}

void Value::getBigInt(BigInt *bigInt)
{
	switch(type)
		{
		case Biginteger:
			bigInt->set(data.bigInt);
			break;

		case Short:
			bigInt->set(data.smallInt, -scale);
			break;

		case Int32:
			bigInt->set(data.integer, -scale);
			break;

		case Int64:
			bigInt->set(data.quad, -scale);
			break;

		case String:
		case Char:
		case Varchar:
			bigInt->setString(data.string.length, data.string.string);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

}

BigInt* Value::setBigInt(void)
{
	clear();
	data.bigInt = new BigInt;
	type = Biginteger;

	return data.bigInt;
}

void Value::truncateString(int maxLength)
{
	switch (type)
		{
		case String:
		case Char:
		case Varchar:
			data.string.length = MIN(data.string.length, maxLength);
			break;

		default:
			;
		}
}

int Value::getTruncatedString(int bufferSize, char* buffer)
{
	switch (type)
		{
		case String:
		case Char:
		case Varchar:
			{
			int len = MIN(bufferSize, data.string.length);
			memcpy(buffer, data.string.string, len);

			return len;
			}

		case BlobPtr:
			data.blob->getBytes(0, bufferSize, buffer);
			break;

		case ClobPtr:
			data.clob->getSubString(0, bufferSize, buffer);
			break;

		default:
			{
			char temp[256];
			int len = getString(sizeof(temp), temp);

			if (bufferSize < len)
				len = bufferSize;

			memcpy(buffer, temp, len);

			return len;
			}
		}

	return -1;
}

int64 Value::getSeconds(void)
{
	switch (type)
		{
		case Date:
			return data.date.getSeconds();
		
		case TimeType:
			return data.time.getSeconds();
		
		case Timestamp:
			return data.timestamp.getSeconds();
		
		default:
			NOT_YET_IMPLEMENTED;
		}
}

int64 Value::getMilliseconds(void)
{
	switch (type)
		{
		case Date:
			return data.date.getMilliseconds();
		
		case TimeType:
			return data.time.getMilliseconds();
		
		case Timestamp:
			return data.timestamp.getMilliseconds();
		
		default:
			NOT_YET_IMPLEMENTED;
		}
}

int Value::getNanos(void)
{
	if (type == Timestamp)
		return data.timestamp.getNanos();
		
	return 0;
}

BigInt* Value::getBigInt(void)
{
	if (type == Biginteger)
		return data.bigInt;
	
	NOT_YET_IMPLEMENTED;
}
