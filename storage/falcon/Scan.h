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

// Scan.h: interface for the Scan class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCAN_H__84FD197A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_SCAN_H__84FD197A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ScanType.h"

#define TABLE_ID		0
#define RECORD_NUMBER	1
#define FIELD_ID		2
#define WORD_NUMBER		3

class Dbb;
class Bdb;
class InversionPage;
class Database;
class SearchWords;

struct Inv;

class Scan  
{
public:
	bool validateWord (int32 *numbers);
	int32* getNextValid();
	bool validate(Scan *prior);
	int32* getNumbers();
	Scan(SearchWords *searchWords);
	Scan (ScanType typ, const char *word, SearchWords *searchWords);
	virtual ~Scan();

	bool hit (int32 wordPosition);
	int32* fetch ();
	void addWord (const char *word);
	void setKey (const char *word);
	int compare (int count, int32 *nums);
	bool hit();
	void print();
	int32* getNext ();
	void fini();
	bool start (Database *database);

	Bdb			*bdb;
	Inv			*node;
	int			keyLength;
	int			expandedLength;
	InversionPage	*page;
	UCHAR		caseMask;
	UCHAR		searchMask;
	UCHAR		key [MAX_INV_WORD];
	UCHAR		expandedKey [MAX_INV_WORD];
	bool		eof;
	bool		frequentWord;
	bool		frequentTail;
	UCHAR		*rootEnd;
	Scan		*next;
	ScanType	type;
	Inversion	*inversion;
	SearchWords	*searchWords;
	Dbb			*dbb;
	int32		numbers [4];
	int			hits;
	UCHAR		guardByte;
};

#endif // !defined(AFX_SCAN_H__84FD197A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
