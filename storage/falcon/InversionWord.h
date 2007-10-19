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

// InversionWord.h: interface for the InversionWord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INVERSIONWORD_H__EB3207E3_D03A_11D4_98FF_0000C01D2301__INCLUDED_)
#define AFX_INVERSIONWORD_H__EB3207E3_D03A_11D4_98FF_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_INV_KEY		128
#define MAX_INV_WORD	100

class InversionWord  
{
public:
	bool isEqual (InversionWord *word2);
	int makeKey (UCHAR *key);
	InversionWord();
	virtual ~InversionWord();

	char	word [MAX_INV_KEY];
	ULONG	tableId;
	ULONG	fieldId;
	ULONG	recordNumber;
	ULONG	wordNumber;
	int		wordLength;
};

#endif // !defined(AFX_INVERSIONWORD_H__EB3207E3_D03A_11D4_98FF_0000C01D2301__INCLUDED_)
