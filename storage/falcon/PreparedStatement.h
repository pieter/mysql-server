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

// PreparedStatement.h: interface for the PreparedStatement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREPAREDSTATEMENT_H__02AD6A45_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_PREPAREDSTATEMENT_H__02AD6A45_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Statement.h"

#ifndef __ENGINE_H
typedef __int64			int64;
#endif

class Database;
class Blob;
class DateTime;
class TimeStamp;
class Time;
class Blob;
class Clob;

class PreparedStatement : public Statement//,  public DbPreparedStatement
{
public:
	virtual void setTime (int index, Time * value);
	virtual bool isPreparedResultSet();
	void setString (int index, const WCString *string);
	void setSqlString (const WCString *sqlString);
	virtual void setShort (int index, short value);
	int32 getTerLong (const char** p);
	virtual void setLong (int index, int64 value);
	virtual void setDate (int index, DateTime *value);
	virtual void setBoolean (int index, int value);
	virtual void setByte (int index, char value);
	virtual void setBytes (int index, int length, const char *bytes);
	virtual void setValue (int index, Value *value);
	virtual bool execute();
	virtual void setClob (int index, Clob *blob);
	//virtual Blob* setBlob (int index);
	virtual void setDouble (int index, double value);
	virtual ResultSet* executeQuery();
	virtual int executeUpdate();
	virtual void setInt (int index, int value);
	virtual void setString (int index, const char *string);
	virtual void setSqlString (const char *sqlStr);
	virtual void setNull (int index, int type);
	virtual void setRecord (int length, const char *bytes);
	virtual void setTimestamp (int index, TimeStamp *value);
	virtual void setFloat (int index, float value);
	virtual void setBlob (int index, Blob *value);
	PreparedStatement(Connection *connection, Database *dbb);

protected:
	virtual ~PreparedStatement();

};

#endif // !defined(AFX_PREPAREDSTATEMENT_H__02AD6A45_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
