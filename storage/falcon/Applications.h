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

// Applications.h: interface for the Applications class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APPLICATIONS_H__CD175F81_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_)
#define AFX_APPLICATIONS_H__CD175F81_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "TableAttachment.h"

#define APP_HASH_SIZE	101

class Database;
class Application;
class Table;
class Record;
class Manifest;

class Applications : public TableAttachment
{
public:
	Application* getApplication (Table *table, Record *record);
	//virtual void updateCommit (Table *table, RecordVersion *record);
	//virtual void insertCommit (Table *table, RecordVersion *record);
	virtual void deleteCommit (Table *table, Record *record);
	void tableAdded (Table *table);
	//void create();
	//void zapLinkages();
	void insert (Application *application);
	Application* getApplication (const char *applicationName);
	Applications(Database *db);
	virtual ~Applications();

	Application	*applications [APP_HASH_SIZE];
	Database	*database;
};

#endif // !defined(AFX_APPLICATIONS_H__CD175F81_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_)
