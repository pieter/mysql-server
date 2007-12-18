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

// Sequence.h: interface for the Sequence class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEQUENCE_H__52A2DA15_7937_11D4_98F0_0000C01D2301__INCLUDED_)
#define AFX_SEQUENCE_H__52A2DA15_7937_11D4_98F0_0000C01D2301__INCLUDED_

//#include "Engine.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Schema;
class Database;
class Transaction;


class Sequence  
{
public:
	int64 updatePhysical (int64 delta, Transaction *transaction);
	int64 update (int64 delta, Transaction *transaction);
	Sequence(Database *db, const char *sequenceSchema, const char *sequenceName, int sequenceId);
	virtual ~Sequence();

	const char	*name;
	const char	*schemaName;
	int			id;
	Sequence	*collision;
	Schema		*schema;
	Database	*database;
	void rename(const char* newName);
	Sequence	*recreate(void);
};

#endif // !defined(AFX_SEQUENCE_H__52A2DA15_7937_11D4_98F0_0000C01D2301__INCLUDED_)
