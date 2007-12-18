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

// PrettyPrint.h: interface for the PrettyPrint class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PRETTYPRINT_H__8416D515_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
#define AFX_PRETTYPRINT_H__8416D515_0540_11D6_B8F7_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Stream;

static const int FullTree	= 1;
static const int Plan		= 2;

class PrettyPrint  
{
public:
	void format (const char *pattern, ...);
	void putLine (const char *string);
	void put (const char *string);
	void indent (int level);
	PrettyPrint(int flags, Stream *stream);
	virtual ~PrettyPrint();

	int		flags;
	Stream	*stream;
};

#endif // !defined(AFX_PRETTYPRINT_H__8416D515_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
