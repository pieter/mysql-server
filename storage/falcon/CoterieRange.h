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

// CoterieRange.h: interface for the CoterieRange class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COTERIERANGE_H__8416D513_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
#define AFX_COTERIERANGE_H__8416D513_0540_11D6_B8F7_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CoterieRange  
{
public:
	bool validateAddress (int32 address);
	CoterieRange (const char *from, const char *to);
	CoterieRange(const char *from);
	virtual ~CoterieRange();

	CoterieRange	*next;
	JString			fromString;
	JString			toString;
	int32			fromIPaddress;
	int32			toIPaddress;
};

#endif // !defined(AFX_COTERIERANGE_H__8416D513_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
