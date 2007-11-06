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
 *	MODULE:			SQLException.h
 *	DESCRIPTION:	SQL Exception object
 *
 * copyright (c) 1997 by James A. Starkey
 */

#ifndef __SQLEXCEPTION_H
#define __SQLEXCEPTION_H

#ifdef _WIN32xxx
#define DllExport	__declspec( dllexport )
#else
#define DllExport
#endif


enum SqlCode {
	SYNTAX_ERROR				= -1,
	FEATURE_NOT_YET_IMPLEMENTED = -2,
	BUG_CHECK					= -3,
	COMPILE_ERROR				= -4,
	RUNTIME_ERROR				= -5,
	IO_ERROR					= -6,
	NETWORK_ERROR				= -7,
	CONVERSION_ERROR			= -8,
	TRUNCATION_ERROR			= -9,
	CONNECTION_ERROR			= -10,
	DDL_ERROR					= -11,
	APPLICATION_ERROR			= -12,
	SECURITY_ERROR				= -13,
	DATABASE_CORRUPTION			= -14,
	VERSION_ERROR				= -15,
	LICENSE_ERROR				= -16,
	INTERNAL_ERROR				= -17,
	DEBUG_ERROR					= -18,
	LOST_BLOB					= -19,
	INCONSISTENT_BLOB			= -20,
	DELETED_BLOB				= -21,
	LOG_ERROR					= -22,
	DATABASE_DAMAGED			= -23,
	UPDATE_CONFLICT				= -24,
	NO_SUCH_TABLE				= -25,
	INDEX_OVERFLOW				= -26,
	UNIQUE_DUPLICATE			= -27,
	UNCOMMITTED_UPDATES			= -28,
	DEADLOCK					= -29,
	OUT_OF_MEMORY_ERROR			= -30,
	OUT_OF_RECORD_MEMORY_ERROR	= -31,
	DDL_TABLESPACE_EXIST_ERROR	= -32
	};

class DllExport SQLException {
public:
	SQLException() {};
	virtual int			getSqlcode () = 0;
	virtual const char	*getText() = 0;
	virtual const char	*getTrace() = 0;
	virtual const char	*getObjectSchema()	{ return NULL; };
	virtual const char	*getObjectName()	{ return NULL; };
	virtual ~SQLException() {};
    };

#endif
