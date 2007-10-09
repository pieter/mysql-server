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

// TableSpace.cpp: implementation of the TableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TableSpace.h"
#include "Dbb.h"
#include "Database.h"
#include "SQLError.h"
#include "Hdr.h"
#include "Cache.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableSpace::TableSpace(Database *db, const char *spaceName, int spaceId, const char *spaceFilename, uint64 allocation)
{
	database = db;
	name = spaceName;
	tableSpaceId = spaceId;
	filename = spaceFilename;
	dbb = new Dbb(database->dbb, tableSpaceId);
	active = false;
	initialAllocation = allocation;
}

TableSpace::~TableSpace()
{
	dbb->close();
	delete dbb;
}

void TableSpace::open()
{
	try
		{
		dbb->openFile(filename, false);
		active = true;
		}
	catch (SQLException&)
		{
		//if (dbb->doesFileExits(fileName))
			throw;

		create();

		return;
		}

	Hdr	header;

	try
		{
		dbb->readHeader (&header);

		if (header.fileType != HdrTableSpace)
			throw SQLError (RUNTIME_ERROR, "table space file \"%s\" has wrong page type (expeced %d, got %d)\n", 
							(const char*) filename, HdrRepositoryFile, header.fileType);

		if (header.pageSize != dbb->pageSize)
			throw SQLError (RUNTIME_ERROR, "table space file \"%s\" has wrong page size (expeced %d, got %d)\n", 
							(const char*) filename, dbb->pageSize, header.pageSize);


		dbb->initRepository(&header);
		}
	catch (...)
		{
		/***
		isOpen = false;
		isWritable = false;
		JString name = getName();
		***/
		dbb->closeFile();
		throw;
		}
}

void TableSpace::create()
{
	dbb->createPath(filename);
	dbb->create(filename, dbb->pageSize, 0, HdrTableSpace, 0, NULL, initialAllocation);
	active = true;
	dbb->flush();
}

void TableSpace::shutdown(TransId transId)
{
	dbb->shutdown(transId);
}

void TableSpace::dropTableSpace(void)
{
	dbb->dropDatabase();
}

bool TableSpace::fileNameEqual(const char* file)
{
	//char expandedName[1024];
	//IO::expandFileName(file, sizeof(expandedName), expandedName);
	
	return filename == file;
}

void TableSpace::sync(void)
{
	database->cache->syncFile(dbb, "sync");
}
