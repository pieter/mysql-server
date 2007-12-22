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


// copyright (c) 1999 - 2000 by James A. Starkey


#ifndef __ENGINE_H
#define __ENGINE_H

#include <time.h>

#ifdef ENGINE
#define MEMORY_MANAGER
#endif


#ifdef _LEAKS
#include <AFX.h>
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#ifdef _WIN32
const char SEPARATOR = '\\';
#else
const char SEPARATOR = '/';
#endif

#ifdef MEMORY_MANAGER
#include "MemoryManager.h"
#endif

#ifdef NAMESPACE
namespace NAMESPACE{}		// declare namespace before use
using namespace NAMESPACE;
#define START_NAMESPACE		namespace NAMESPACE {
#define CLASS(cls)			namespace NAMESPACE { class cls; };
#define END_NAMESPACE		}
#else
#define START_NAMESPACE
#define CLASS(cls)			class cls;
#define END_NAMESPACE
#endif

#ifndef NULL
#define NULL		0
#endif

#define OFFSET(type,fld)	(IPTR)&(((type)0)->fld)
#define MAX(a,b)			((a > b) ? a : b)
#define MIN(a,b)			((a < b) ? a : b)
#define ABS(n)				(((n) >= 0) ? (n) : -(n))
#define MASK(n)				(1 << (n))
#define ISLOWER(c)			(c >= 'a' && c <= 'z')
#define ISUPPER(c)			(c >= 'A' && c <= 'Z')
#define ISDIGIT(c)			(c >= '0' && c <= '9')
#define UPPER(c)			((ISLOWER (c)) ? c - 'a' + 'A' : c)
#define ROUNDUP(n,b)		(((n) + b - 1) & ~(b - 1))
#define ALIGN(ptr,b)		((UCHAR*) (((UIPTR) ptr + b - 1) / b * b))
#define SQLEXCEPTION		SQLError

typedef int				int32;
typedef unsigned int	uint32;
typedef unsigned int	uint;

#ifdef _WIN32

#ifndef strcasecmp
#define strcasecmp		stricmp
#define strncasecmp		strnicmp
#define snprintf		_snprintf
#define vsnprintf		_vsnprintf
#define QUAD_CONSTANT(x)	x##i64
#define I64FORMAT			"%I64d"
#endif

#ifdef _WIN64
typedef __int64				IPTR;
typedef unsigned __int64	UIPTR;
#define HAVE_IPTR
#endif

#define INTERLOCK_TYPE		long

#else

#define __int64			long long
#define _stdcall
#define QUAD_CONSTANT(x)	x##LL
#define I64FORMAT			"%lld"
#endif

#ifndef HAVE_IPTR
typedef long			IPTR;
typedef unsigned long	UIPTR;
#endif

typedef unsigned char	UCHAR;
typedef unsigned long	ULONG;
typedef unsigned short	USHORT;

typedef short				int16;
typedef unsigned short		uint16;
typedef __int64				QUAD;
typedef unsigned __int64	UQUAD;
typedef __int64				int64;
typedef unsigned __int64	uint64;

// Standard Falcon engine type definitions

typedef uint32				TransId;		// Transaction ID
typedef int64				RecordId;

#define TXIDFORMAT			"%ld"

#ifndef INTERLOCK_TYPE
#define INTERLOCK_TYPE	int
#endif

#ifdef ENGINE
#include "Error.h"
#endif

#include "JString.h"


#endif
