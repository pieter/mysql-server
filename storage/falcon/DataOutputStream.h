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

// DataOutputStream.h: interface for the DataOutputStream class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATAOUTPUTSTREAM_H__E4CF0405_0ECC_11D3_AB71_0000C01D2301__INCLUDED_)
#define AFX_DATAOUTPUTSTREAM_H__E4CF0405_0ECC_11D3_AB71_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Stream.h"
#include "WString.h"

struct JavaName;

class DataOutputStream : public Stream  
{
public:
	void writeUTF (const char *string);
	//void writeUTF (JavaName *name);
	void writeUTF (int length, WCHAR *string);
	void writeShort (short value);
	void writeInt (int32 value);
	void writeLong (QUAD value);
	DataOutputStream();
	virtual ~DataOutputStream();

};

#endif // !defined(AFX_DATAOUTPUTSTREAM_H__E4CF0405_0ECC_11D3_AB71_0000C01D2301__INCLUDED_)
