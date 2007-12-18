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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			SQLError.h
 *	DESCRIPTION:	SQL Exception object
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#ifndef __SQLERROR_H
#define __SQLERROR_H

#include "SQLException.h"
#include "JString.h"

class Stream;

class SQLError : public SQLException
{
public:
	SQLError (int sqlcode, const char *text, ...);
	SQLError (SqlCode sqlcode, const char *text, ...);
	SQLError (const char *trace, int traceLength, SqlCode code, const char *txt,...);
	virtual ~SQLError();

	virtual const char* getTrace();
	virtual int			getSqlcode ();
	virtual const char	*getText();
	virtual const char* getObjectName();
	virtual const char* getObjectSchema();
	virtual void		setObject(const char *schema, const char *name);

	operator	const char*();

	int			sqlcode;
	JString		text;
	JString		stackTrace;
	JString		objectSchema;
	JString		objectName;

private:
	void error (const char *string);
    };

#endif
