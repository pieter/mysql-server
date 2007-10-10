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

// Protocol.cpp: implementation of the Protocol class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Protocol.h"
#include "Socket.h"
#include "SQLError.h"
#include "Value.h"
#include "Blob.h"
#include "DateTime.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "BigInt.h"
//#include "Log.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


Protocol::~Protocol()
{
}


int32 Protocol::openDatabase(const char * fileName)
{
	putMsg (OpenDatabase);
	putString (fileName);

	if (!getResponse())
		return 0;

	return getHandle();
}


void Protocol::putMsg(MsgType type)
{
	if (this == NULL)
		throw SQLEXCEPTION (CONNECTION_ERROR, "database is not connected");
	putLong (type);
}

MsgType Protocol::getMsg()
{
	return (MsgType) getLong();
}

bool Protocol::getResponse()
{
	flush();
	MsgType type = getMsg();

	if (type == Success)
		return true;

	SqlCode sqlcode = (SqlCode) getLong();
	char *string = getString();
	throw SQLEXCEPTION (sqlcode, string);
	delete [] string;

	return false;
}

void Protocol::sendSuccess()
{
	putMsg (Success);
}

void Protocol::sendFailure(SQLException *exception)
{
	putMsg (Failure);
	putLong (exception->getSqlcode());
	putString (exception->getText());
}

void Protocol::shutdown()
{
	try
		{
		putMsg (Shutdown);
		flush();
		close();
		}
	catch (...)
		{
		}
}


void Protocol::putValue(Database *database, Value *value)
{
	Type type = value->getType();
	putLong (type);
	
	switch (type)
		{
		case Null:
			break;

		case Char:
		case Varchar:
		case String:
			putString(value->getStringLength(), value->getString());
			break;

		case Short:
			{
			int scale = value->getScale();
			
			if (protocolVersion >= PROTOCOL_VERSION_3)
				putShort(scale);
				
			putShort (value->getShort(scale));
			}
			break;

		case Long:
			{
			int scale = value->getScale();
			
			if (protocolVersion >= PROTOCOL_VERSION_3)
				putShort (scale);
				
			putLong (value->getInt(scale));
			}
			break;

		case Quad:
			{
			int scale = value->getScale();
			
			if (protocolVersion >= PROTOCOL_VERSION_3)
				putShort (scale);
				
			putQuad (value->getQuad(scale));
			}
			break;

		case Float:
		case Double:
			putDouble (value->getDouble());
			break;

		case Date:
			if (protocolVersion >= PROTOCOL_VERSION_4)
				putQuad (value->getMilliseconds());
			else
				putLong ((int32) value->getSeconds());
			break;

		case TimeType:
			putLong ((int32) value->getSeconds());
			break;

		case Timestamp:
			if (protocolVersion >= PROTOCOL_VERSION_4)
				putQuad (value->getMilliseconds());
			else
				putLong ((int32) value->getSeconds());
				
			putLong (value->getNanos());
			break;

		case BlobPtr:
			{
			Blob *blob = value->getBlob();
			
			if (protocolVersion >= PROTOCOL_VERSION_11)
				{
				UCHAR temp [256];
				int length = blob->getReference (sizeof (temp), temp);
				putLong (length);
				
				if (length > 0)
					putBytes (length, temp);
				}
				
			int length = blob->length();
			putLong (length);
			
			if (length > 0)
				for (Segment *segment = blob->getStream()->segments; segment; segment = segment->next)
					putBytes(segment->length, segment->address);

			blob->release();
			}
			break;

		case ClobPtr:
			{
			Clob *clob = value->getClob();
			
			if (protocolVersion >= PROTOCOL_VERSION_11)
				{
				UCHAR temp [256];
				int length = clob->getReference (sizeof (temp), temp);
				putLong (length);
				
				if (length > 0)
					putBytes (length, temp);
				}
				
			int length = clob->length();
			putLong (length);
			
			if (length > 0)
				for (Segment *segment = clob->getStream()->segments; segment; segment = segment->next)
					putBytes(segment->length, segment->address);

			clob->release();
			}
			break;

		case Biginteger:
			{
			BigInt *bigInt = value->getBigInt();
			putByte((char) bigInt->neg);
			putShort(bigInt->scale);
			putShort(bigInt->length);
			
			for (int n = 0; n < bigInt->length; ++n)
				putLong(bigInt->words[n]);
			}
			break;
			
#ifdef ENGINE
		default:
			ASSERT (false);
#endif
		}
}

void Protocol::getValue(Value * value)
{
	Type type = (Type) getLong();
	int scale = 0;

	switch (type)
		{
		case Null:
			value->clear();
			break;

		case Char:
		case Varchar:
		case String:
			{
			int32 length = getLong();
			
			if (length < 0)
				value->clear();
			else
				{
				char *string = value->allocString (type, length);
				getBytes(length, string);
				}
			}
			break;

		case Short:
			if (protocolVersion >= PROTOCOL_VERSION_3)
				scale = getShort();
			value->setValue (getShort(), scale);
			break;

		case Long:
			if (protocolVersion >= PROTOCOL_VERSION_3)
				scale = getShort();
			value->setValue (getLong(), scale);
			break;

		case Quad:
			if (protocolVersion >= PROTOCOL_VERSION_3)
				scale = getShort();
			value->setValue (getQuad(), scale);
			break;

		case Float:
		case Double:
			value->setValue (getDouble());
			break;

		case Date:
			{
			DateTime date;
			if (protocolVersion >= PROTOCOL_VERSION_4)
				date.setMilliseconds (getQuad());
			else
				date.setSeconds (getLong());
			value->setValue (date);
			}
			break;

		case Timestamp:
			{
			TimeStamp date;
			if (protocolVersion >= PROTOCOL_VERSION_4)
				date.setMilliseconds (getQuad());
			else
				date.setSeconds (getLong());
			date.setNanos (getLong());
			value->setValue (date);
			}
			break;

		case TimeType:
			{
			Time date;
			//date.date = getLong();
			date.setSeconds (getLong());
			value->setValue (date);
			}
			break;

		//case Asciiblob:
		case ClobPtr:
			{
			AsciiBlob *clob = new AsciiBlob;
			if (protocolVersion >= PROTOCOL_VERSION_11)
				getRepository (clob);
			int32 length = getLong();
			if (length >= 0)
				{
				char *p = clob->alloc (length);
				getBytes (length, p);
				}
			else
				clob->unsetData();
			value->setValue (clob);
			clob->release();
			}
			break;

		case BlobPtr:
			{
			BinaryBlob *blob = new BinaryBlob;
			
			if (protocolVersion >= PROTOCOL_VERSION_11)
				getRepository (blob);
				
			int32 length = getLong();
			
			if (length >= 0)
				{
				char *p = blob->alloc (length);
				getBytes (length, p);
				}
			else
				blob->unsetData();
				
			value->setValue (blob);
			blob->release();
			}
			break;

		case Biginteger:
			{
			BigInt *bigInt = value->setBigInt();
			bigInt->neg = getByte();
			bigInt->scale = getShort();
			bigInt->length = getShort();
			
			for (int n = 0; n < bigInt->length; ++n)
				bigInt->words[n] = getLong();
			}
			break;
			
#ifdef ENGINE
		default:
			ASSERT (false);
#endif
		}
}

Protocol::Protocol(socket_t sock, sockaddr_in * addr) : Socket (sock, addr)
{
	protocolVersion = 0;
}

void Protocol::putMsg(MsgType type, int32 handle)
{
	putMsg (type);
	putHandle (handle);
}

Protocol::Protocol()
{
	protocolVersion = 0;
}

int32 Protocol::createStatement()
{
	putMsg (CreateStatement);

	if (!getResponse())
		return 0;

	return getHandle();
}

void Protocol::putValues(Database *database, int count, Value * values)
{
	putLong (count);

	for (int n = 0; n < count; ++n)
		putValue (database, values + n);
}

void Protocol::getRepository(BlobReference *blob)
{
	int length = getLong();

	if (length <= 0)
		return;

	UCHAR temp [256];

	if (length > (int) sizeof (temp))
		{
		//Log::log ("Protocol::getRepository: invalid repository block (%d bytes)\n", length);
		for (; length > 0; --length)
			getByte();
		return;
		}

	getBytes (length, temp);
	blob->setReference (length, temp);
}
