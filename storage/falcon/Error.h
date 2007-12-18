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

// Error.h: interface for the Error class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#if !defined(AFX_ERROR_H__6A019C1E_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_ERROR_H__6A019C1E_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#undef ERROR
#undef ASSERT
#define FATAL	Error::error
#define ASSERT(f)	while (!(f)) Error::assertionFailed (__FILE__, __LINE__)
#define NOT_YET_IMPLEMENTED	Error::notYetImplemented (__FILE__, __LINE__)

class Error  
{
public:
	static void notYetImplemented(const char *fileName, int line);
	static void debugBreak();
	static void validateHeap (const char *where);
	static void assertionFailed (const char *fileName, int line);
	static void error (const char *text, ...);
};

#endif // !defined(AFX_ERROR_H__6A019C1E_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
