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

// Btn.h: interface for the Btn class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BTN_H__5DD7F232_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_BTN_H__5DD7F232_A406_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "IndexKey.h"

class Btn
{
public:
	//int32 getPageNumber();
	//void printKey (const char *msg, UCHAR *key, bool inversion);
	static void printKey (const char *msg, int length, UCHAR *key, int prefix, bool inversion);
	static void printKey (const char *msg, IndexKey *key, int prefix, bool inversion);

	// <offset> <length> <key> <number>
	//UCHAR	offset;
	//UCHAR	length;
	//UCHAR	number [4];
	//UCHAR	key[1];
};

#endif // !defined(AFX_BTN_H__5DD7F232_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
