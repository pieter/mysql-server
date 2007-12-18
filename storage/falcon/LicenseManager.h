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

// LicenseManager.h: interface for the LicenseManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LICENSEMANAGER_H__92E67AA5_5BF5_11D4_98EE_0000C01D2301__INCLUDED_)
#define AFX_LICENSEMANAGER_H__92E67AA5_5BF5_11D4_98EE_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TableAttachment.h"

#define HASH_SIZE	101

class Database;
class LicenseProduct;
class DateTime;
class License;

class LicenseManager : public TableAttachment  
{
public:
	void installLicense (License *license);
	License* installLicense (const char *licenseText);
	LicenseProduct* findProduct (Table * table, Record * record);
	void scavenge(DateTime *now);
	LicenseProduct* getProduct (const char *name);
	void initialize ();
	virtual void deleteCommit(Table * table, Record * record);
	virtual void updateCommit(Table * table, RecordVersion * record);
	virtual void insertCommit(Table * table, RecordVersion * record);
	virtual void tableAdded(Table * table);
	LicenseManager(Database *db);
	virtual ~LicenseManager();

	Database		*database;
	LicenseProduct	*hashTable [HASH_SIZE];
	LicenseProduct	*products;
	int				productId;
	int				licenseId;
	int				idId;
	//long			ipAddress;
};

#endif // !defined(AFX_LICENSEMANAGER_H__92E67AA5_5BF5_11D4_98EE_0000C01D2301__INCLUDED_)
