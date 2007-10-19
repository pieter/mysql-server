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
 *	MODULE:			SQLError.cpp
 *	DESCRIPTION:	SQL Exception object
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "Engine.h"
#include "SQLError.h"
#include "Stream.h"
#include "Log.h"
#include "LogLock.h"
#include "Sync.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


SQLError::SQLError (SqlCode code, const char *txt, ...)
{
/**************************************
 *
 *		S Q L E x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *		SQL exception -- quite generic.
 *
 **************************************/
	va_list		args;
	va_start	(args, txt);
	char		temp [1024];

	stackTrace = NULL;

	if (vsnprintf (temp, sizeof (temp) - 1, txt, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	text = temp;
	error (temp);
	sqlcode = (int) code;
}

SQLError::SQLError(const char *trace, int traceLength, SqlCode code, const char * txt, ...)
{
/**************************************
 *
 *		S Q L E x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *		SQL exception -- quite generic.
 *
 **************************************/
	va_list		args;
	va_start	(args, txt);
	char		temp [1024];

	char *buffer = stackTrace.getBuffer (traceLength);
	memcpy (buffer, trace, traceLength);
	stackTrace.releaseBuffer();

	if (vsnprintf (temp, sizeof (temp) - 1, txt, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	text = temp;
	error (temp);
	sqlcode = (int) code;
}

SQLError::~SQLError ()
{
/**************************************
 *
 *		~ S Q L E x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *		Object destructor.
 *
 **************************************/
}

int SQLError::getSqlcode ()
{
/**************************************
 *
 *		g e t S q l c o d e
 *
 **************************************
 *
 * Functional description
 *		Get standard sql code.
 *
 **************************************/

return sqlcode;
}

const char *SQLError::getText ()
{
/**************************************
 *
 *		g e t T e x t
 *
 **************************************
 *
 * Functional description
 *		Get text of exception.
 *
 **************************************/

return text;
}

SQLError::operator const char* ()
{
/**************************************
 *
 *		o p e r a t o r   c h a r *
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/

return getText();
}

SQLError::SQLError(int code, const char * txt, ...)
{
	va_list		args;
	va_start	(args, txt);
	char		temp [1024];

	stackTrace = NULL;

	if (vsnprintf (temp, sizeof (temp) - 1, txt, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	text = temp;
	error (temp);
	sqlcode = (int) code;
}

const char* SQLError::getTrace()
{
	return stackTrace;
}

void SQLError::error(const char *string)
{
#ifdef ENGINE
	LogLock logLock;
	Log::log(LogException, "Exception: %s\n", string);
#endif
}

const char* SQLError::getObjectSchema()
{
	return objectSchema;
}

const char* SQLError::getObjectName()
{
	return objectName;
}

void SQLError::setObject(const char *schema, const char *name)
{
	objectName = name;
	objectSchema = schema;
}
