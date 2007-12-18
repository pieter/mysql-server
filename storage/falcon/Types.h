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

#ifndef __TYPE_H
#define __TYPE_H

enum Type {
	Null,			// 0
	String,			// 1 generic, null terminated
	Char,			// 2 fixed length string, also null terminated
	Varchar,		// 3 variable length, counted string

	Short,			// 4
	Long,			// 5
	Int32 = Long,
	Quad,			// 6
	Int64 = Quad,

	Float,			// 7
	Double,			// 8

	Date,			// 9
	Timestamp,		// 10
	TimeType,		// 11

	Asciiblob,		// 12 on disk blob
	Binaryblob,		// 13 on disk blob
	BlobPtr,		// 14 pointer to Blob object
	SqlTimestamp,	// 15 64 bit version
	ClobPtr,		// 16
	Biginteger		// 17
	};

#ifdef ENGINE
#define JDBC_TYPES
#endif

#ifdef JDBC_TYPES
enum JdbcType {
	jdbcNULL	= 0,

	TINYINT		= -7,	// byte / C char
	SMALLINT	= 5,	// short
	INTEGER		= 4,
	BIGINT		= -5,	// int64 (64 bit)

	jdbcFLOAT	= 6,
	jdbcDOUBLE		= 8,

	jdbcCHAR	= 1,
	VARCHAR		= 12,
	LONGVARCHAR	= -1,

	jdbcDATE	= 91,
	jdbcTIME	= 92,
	TIMESTAMP	= 93,

	jdbcBLOB	= 2004,
	jdbcCLOB	= 2005,
	CLOB		= 2005,
	NUMERIC		= 2,
	};
#endif

// Type Encoded Record Types

#define TER_ENCODING_INTEL			1	// Intel encoding
#define TER_FORMAT_VERSION_1	1
#define TER_FORMAT				TER_FORMAT_VERSION_1

enum TerType {
	terNull,
	terString,			// 32 bit count followed by string followed by null byte
	terUnicode,			// 32 bit count followed by string followed by null character

	terShort,
	terLong,
	terQuad,

	terFloat,
	terDouble,

	terDate,
	terTimestamp,
	terTime,

	terBinaryBlob,		// 32 bit length followed by blob
	terBigDate,			// Quad date with millisecond accuracy
	terBigTimestamp,	// Quad date with millisecond accuracy
	terScaledShort,
	terScaledLong,
	terScaledQuad,
	terRepositoryBlob,
	terRepositoryClob,
	};


struct FieldType {
	Type type;
	int length;
	int precision;
	int scale;
	};

#endif

