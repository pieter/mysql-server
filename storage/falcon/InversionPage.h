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

// InversionPage.h: interface for the InversionPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INVERSIONPAGE_H__84FD1975_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_INVERSIONPAGE_H__84FD1975_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Page.h"

#define NEXT_INV(node)		((Inv*) (node->key + node->length))
#define INV_SIZE			(sizeof (Inv) - 1)

struct Inv {
	UCHAR	offset;
    UCHAR	length;
	UCHAR	key [1];
public:
	static bool validate (Inv *node, int keyLength, UCHAR *expandedKey);
	static void printKey (int length, UCHAR *key);
	static void encode (ULONG number, UCHAR **ptr);
	static int32 decode (UCHAR **ptr);
	void printKey (UCHAR *expandedKey);
	};

class Dbb;
class Bdb;
class Bitmap;
class IndexKey;

class InversionPage : public Page  
{
public:
	static void logPage (Bdb *bdb);
	void analyze (int pageNumber);
	void removeNode (Dbb * dbb, int keyLength, UCHAR * key);
	void validate (Dbb *dbb, Validation *validation, Bitmap *pages);
	void validate();
	void printPage(Bdb *bdb);
	Bdb* splitInversionPage (Dbb * dbb, Bdb *bdb, IndexKey *indexKey, TransId transId);
	Inv* findNode (int keyLength, UCHAR *key, UCHAR* expandedKey, int *expandedKeyLength);
	bool addNode (Dbb *dbb, IndexKey *indexKey);
	int  computePrefix (int l1, UCHAR *v1, int l2, UCHAR *v2);
	//InversionPage();
	//virtual ~InversionPage();

	int32	parentPage;
	int32	priorPage;
	int32	nextPage;
	short	length;
	Inv		nodes [1];
};

#endif // !defined(AFX_INVERSIONPAGE_H__84FD1975_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
