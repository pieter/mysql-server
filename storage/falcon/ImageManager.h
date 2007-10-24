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

// ImageManager.h: interface for the ImageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IMAGEMANAGER_H__42BED351_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
#define AFX_IMAGEMANAGER_H__42BED351_8E56_11D3_AB7E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "TableAttachment.h"

#define IMAGES_HASH_SIZE		101


class Images;
class Image;
class Database;
class Application;


class ImageManager : public TableAttachment
{
public:
	virtual void preDelete(Table *table, RecordVersion *record);
	void checkAccess (Table *table, RecordVersion *record);
	virtual void preUpdate (Table *table, RecordVersion *record);
	virtual void updateCommit (Table *table, RecordVersion *record);
	virtual void deleteCommit (Table *table, Record *record);
	Images* findImages (Table *table, Record *record);
	Images* findImages (const char *name);
	virtual void insertCommit(Table * table, RecordVersion * record);
	void assignAlias (Table *table, RecordVersion *record);
	virtual void preInsert (Table *table, RecordVersion *record);
	void tableAdded (Table *table);
	Images* getImages (const char *name, Application *extends);
	ImageManager(Database *db);
	virtual ~ImageManager();

	Database	*database;
	Images		*hashTable [IMAGES_HASH_SIZE];
	int			applicationId;
	int			nameId;
	int			typeId;
	int			imageId;
	int			aliasId;
	int			widthId;
	int			heightId;
	int			nextAlias;
};

#endif // !defined(AFX_IMAGEMANAGER_H__42BED351_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
