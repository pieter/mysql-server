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

// SequenceManager.h: interface for the SequenceManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEQUENCEMANAGER_H__52A2DA14_7937_11D4_98F0_0000C01D2301__INCLUDED_)
#define AFX_SEQUENCEMANAGER_H__52A2DA14_7937_11D4_98F0_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

#define SEQUENCE_HASH_SIZE	101

class Database;
class Sequence;

class SequenceManager  
{
public:
	Sequence* getSequence (const char *schema, const char *name);
	void deleteSequence (const char *schema, const char *name);
	Sequence* createSequence (const char *schema, const char *name, int64 initialValue);
	Sequence* findSequence (const char *schema, const char *name);
	void initialize();
	SequenceManager(Database *db);
	virtual ~SequenceManager();

protected:
	Database	*database;
	Sequence	*sequences [SEQUENCE_HASH_SIZE];
	SyncObject	syncObject;
public:
	void renameSequence(Sequence* sequence, const char* newName);
	Sequence *recreateSequence(Sequence *oldSequence);
};

#endif // !defined(AFX_SEQUENCEMANAGER_H__52A2DA14_7937_11D4_98F0_0000C01D2301__INCLUDED_)
