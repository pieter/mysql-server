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

// CollationManager.h: interface for the CollationManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLLATIONMANAGER_H__1F63A287_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_COLLATIONMANAGER_H__1F63A287_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

static const int COLLATION_HASH_SIZE = 101;

class Collation;

class CollationManager  
{
public:
	CollationManager();
	virtual ~CollationManager();

	Collation*			find(const char* collationName);

	static Collation*	getCollation (const char *name);
	static Collation*	findCollation(const char* collationName);
	static void			addCollation(Collation* collation);
	
protected:
	void				add(Collation* collation);

public:	
	Collation	*hashTable[COLLATION_HASH_SIZE];
	SyncObject	syncObject;
	void flush(void);
};

#endif // !defined(AFX_COLLATIONMANAGER_H__1F63A287_9414_11D5_899A_CC4599000000__INCLUDED_)
