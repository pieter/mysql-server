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

// PreparedStatement.cpp: implementation of the PreparedStatement class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "PreparedStatement.h"
#include "Database.h"
#include "Table.h"
#include "Value.h"
#include "Index.h"
#include "Syntax.h"
#include "SQLParse.h"
#include "CompiledStatement.h"
#include "NNode.h"
#include "NSelect.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "SQLError.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PreparedStatement::PreparedStatement(Connection *connection, Database *db) :
	Statement (connection, db)
{
}

PreparedStatement::~PreparedStatement()
{

}

void PreparedStatement::setSqlString(const char * sqlString)
{
	statement = database->getCompiledStatement (connection, sqlString);
	statement->addInstance (this);
	prepareStatement();
}


void PreparedStatement::setSqlString(const WCString *sqlString)
{
	statement = database->getCompiledStatement (connection, sqlString);
	statement->addInstance (this);
	prepareStatement();
}

void PreparedStatement::setString(int index, const char * string)
{
	getParameter (index - 1)->setString (string, true);
}

void PreparedStatement::setInt(int index, int value)
{
	getParameter (index - 1)->setValue (value);
}

int PreparedStatement::executeUpdate()
{
	if (!statement->node)
		throw SQLEXCEPTION (RUNTIME_ERROR, "statement is non-executable");

	start (statement->node);
	active = false;

	return recordsUpdated;
}

ResultSet* PreparedStatement::executeQuery()
{
	if (!statement->node || statement->node->type != Select)
		throw SQLEXCEPTION (RUNTIME_ERROR, "statement is not a Select");

	clearResults (false);
	NSelect *select = (NSelect*) statement->node;
	start (select);

	return getResultSet();
}

void PreparedStatement::setDouble(int index, double value)
{
	getParameter (index - 1)->setValue (value);
}

void PreparedStatement::setFloat(int index, float value)
{
	getParameter (index - 1)->setValue (value);
}


bool PreparedStatement::execute()
{
	if (statement->ddl)
		{
		executeDDL();
		return false;
		}

	switch (statement->node->type)
		{
		case Insert:
		case Update:
		case Delete:
		case Replace:
			executeUpdate();
			
			return false;

		case Select:
			//executeQuery();
			start (statement->node);
			
			return true;

		default:
			throw SQLEXCEPTION (RUNTIME_ERROR, "not an executable statement");
		}

	return false;
}

void PreparedStatement::setValue(int index, Value * value)
{
	getParameter (index - 1)->setValue (value, true);
}

void PreparedStatement::setNull(int index, int type)
{
	getParameter (index - 1)->setNull();
}

void PreparedStatement::setBytes(int index, int length, const char * bytes)
{
	BinaryBlob *blob = new BinaryBlob();
	getParameter (index - 1)->setValue (blob);
	blob->putSegment (length, bytes, true);
	blob->release();
}

void PreparedStatement::setByte(int index, char value)
{
	getParameter (index - 1)->setValue ((int) value);
}

void PreparedStatement::setBoolean(int index, int value)
{
	getParameter (index - 1)->setValue (value);
}

void PreparedStatement::setDate(int index, DateTime * value)
{
	getParameter (index - 1)->setValue (*value);
}

void PreparedStatement::setTime(int index, Time *value)
{
	getParameter (index - 1)->setValue (*value);
}

void PreparedStatement::setTimestamp(int index, TimeStamp *value)
{
	getParameter (index - 1)->setValue (*value);
}

void PreparedStatement::setLong(int index, int64 value)
{
	getParameter (index - 1)->setValue (value);
}

void PreparedStatement::setRecord(int length, const char * bytes)
{
	const char *end = bytes + length;
	const char *p = bytes;
	//int format = 
	*p++;
	//int encoding = 
	*p++;
	int count = getTerLong (&p);

	if (count > statement->numberParameters)
		throw SQLEXCEPTION (RUNTIME_ERROR, "more fields (%d) in record than parameters in statement (%d):\n%s",
							count, statement->numberParameters, (const char*) statement->sqlString);

	for (int n = 1; n <= count; ++n)
		{
		if (p >= end)
			throw SQLEXCEPTION (RUNTIME_ERROR, "overflow in setRecord");
			
		TerType type = (TerType) *p++;
		
		switch (type)
			{
			case terNull:
				setNull (n, INTEGER);
				break;

			case terString:
				{
				int length = getTerLong (&p);
				setString (n, p);
				p += length + 1;
				}
				break;

			case terShort:
				{
				short value;
				memcpy (&value, p, sizeof (value));
				setShort (n, value);
				p += sizeof (value);
				}
				break;

			case terScaledShort:
				{
				char scale = *p++;
				short value;
				memcpy (&value, p, sizeof (value));
				p += sizeof (value);
				Value val;
				val.setValue (value, scale);
				setValue (n, &val);
				}
				break;

			case terLong:
				{
				int32 value;
				memcpy (&value, p, sizeof (value));
				setInt (n, value);
				p += sizeof (value);
				}
				break;

			case terScaledLong:
				{
				char scale = *p++;
				int value;
				memcpy (&value, p, sizeof (value));
				p += sizeof (value);
				Value val;
				val.setValue (value, scale);
				setValue (n, &val);
				}
				break;

			case terQuad:
				{
				int64 value;
				memcpy (&value, p, sizeof (value));
				setLong (n, value);
				p += sizeof (value);
				}
				break;

			case terScaledQuad:
				{
				char scale = *p++;
				int64 value;
				memcpy (&value, p, sizeof (value));
				p += sizeof (value);
				Value val;
				val.setValue (value, scale);
				setValue (n, &val);
				}
				break;

			case terDouble:
				{
				double value;
				memcpy (&value, p, sizeof (value));
				setDouble (n, value);
				p += sizeof (value);
				}
				break;

			case terDate:
				{
				int32 seconds;
				memcpy (&seconds, p, sizeof (seconds));
				DateTime value;
				value.setSeconds (seconds);
				setDate (n, &value);
				p += sizeof (seconds);
				}
				break;

			case terTime:
				{
				int64 milliseconds;
				memcpy (&milliseconds, p, sizeof (milliseconds));
				Time value;
				value.setMilliseconds (milliseconds);
				setTime (n, &value);
				p += sizeof (milliseconds);
				}
				break;

			case terBigDate:
				{
				int64 milliseconds;
				memcpy (&milliseconds, p, sizeof (milliseconds));
				DateTime value;
				value.setMilliseconds (milliseconds);
				setDate (n, &value);
				p += sizeof (milliseconds);
				}
				break;

			case terTimestamp:
				{
				TimeStamp value;
				int32 seconds;
				memcpy (&seconds, p, sizeof (seconds));
				value.setSeconds (seconds);
				p += sizeof (seconds);
				int nanos;
				memcpy (&nanos, p, sizeof (nanos));
				value.setNanos (nanos);
				p += sizeof (nanos);
				setDate (n, &value);
				}
				break;

			case terBigTimestamp:
				{
				TimeStamp value;
				int64 milliseconds;
				memcpy (&milliseconds, p, sizeof (milliseconds));
				value.setMilliseconds (milliseconds);
				p += sizeof (milliseconds);
				int nanos;
				memcpy (&nanos, p, sizeof (nanos));
				value.setNanos (nanos);
				p += sizeof (nanos);
				//setDate (n, &value);
				setTimestamp (n, &value);
				}
				break;

			case terBinaryBlob:
				{
				int length = getTerLong (&p);
				setBytes (n, length, p);
				p += length;
				}
				break;

			case terRepositoryBlob:
				{
				BinaryBlob *blob = new BinaryBlob;
				int repoLength = getTerLong (&p);
				blob->setReference (repoLength, (UCHAR*) p);
				p += repoLength;
				int length = getTerLong (&p);
				
				if (length >= 0)
					{
					blob->putSegment (length, p, true);
					p += length;
					}
				else
					blob->unsetData();
					
				setBlob (n, blob);
				blob->release();
				}
				break;

			case terRepositoryClob:
				{
				AsciiBlob *blob = new AsciiBlob;
				int repoLength = getTerLong (&p);
				blob->setReference (repoLength, (UCHAR*) p);
				p += repoLength;
				int length = getTerLong (&p);
				
				if (length >= 0)
					{
					blob->putSegment (length, p, true);
					p += length;
					}
				else
					blob->unsetData();
					
				setClob (n, blob);
				blob->release();
				}
				break;

			default:
				NOT_YET_IMPLEMENTED;
			}
		}

	if (p < end)
		throw SQLEXCEPTION (RUNTIME_ERROR, "underflow in setRecord");
}

int32 PreparedStatement::getTerLong(const char **p)
{
	int32 value;
	memcpy (&value, *p, sizeof (value));
	(*p) += sizeof (value);

	return value;
}

void PreparedStatement::setShort(int index, short value)
{
	getParameter (index - 1)->setValue (value);
}

void PreparedStatement::setString(int index, const WCString *string)
{
	getParameter (index - 1)->setString (string);
}

bool PreparedStatement::isPreparedResultSet()
{
	return true;
}

void PreparedStatement::setClob(int index, Clob *value)
{
	getParameter (index - 1)->setValue (value);
}

void PreparedStatement::setBlob(int index, Blob *value)
{
	getParameter (index - 1)->setValue (value);
}
