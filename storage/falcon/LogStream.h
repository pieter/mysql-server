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

// LogStream.h: interface for the LogStream class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LOGSTREAM_H__7C17F6FD_E456_4465_A94A_91702DFC1C3B__INCLUDED_)
#define AFX_LOGSTREAM_H__7C17F6FD_E456_4465_A94A_91702DFC1C3B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Stream.h"

class LogStream : public Stream  
{
public:
	virtual void format (const char *pattern, ...);
	virtual void putSegment (const char *string);
	LogStream();
	virtual ~LogStream();

};

#endif // !defined(AFX_LOGSTREAM_H__7C17F6FD_E456_4465_A94A_91702DFC1C3B__INCLUDED_)
