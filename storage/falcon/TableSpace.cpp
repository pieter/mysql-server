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
#include "PStatement.h"
#include "InfoTable.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableSpace::TableSpace(Database *db, const char *spaceName, int spaceId, const char *spaceFilename, uint64 allocation, int tsType)
{
	database = db;
	name = spaceName;
	tableSpaceId = spaceId;
	filename = spaceFilename;
	dbb = new Dbb(database->dbb, tableSpaceId);
	active = false;
	initialAllocation = allocation;
	needSave = false;
	type = tsType;
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
		dbb->readHeader(&header);

		if (header.pageSize < 1024)
			dbb->skewHeader(&header);

		switch (type)
			{
			case TABLESPACE_TYPE_TABLESPACE:
				if (header.fileType != HdrTableSpace)
					throw SQLError(RUNTIME_ERROR, "table space file \"%s\" has wrong page type (expeced %d, got %d)\n", 
									(const char*) filename, HdrTableSpace, header.fileType);
				break;
			
			case TABLESPACE_TYPE_REPOSITORY:
				if (header.fileType != HdrRepositoryFile)
					throw SQLError(RUNTIME_ERROR, "table space file \"%s\" has wrong page type (expeced %d, got %d)\n", 
									(const char*) filename, HdrRepositoryFile, header.fileType);
				break;
			
			default:
				NOT_YET_IMPLEMENTED;
			}

		if (header.pageSize != dbb->pageSize)
			throw SQLError(RUNTIME_ERROR, "table space file \"%s\" has wrong page size (expeced %d, got %d)\n", 
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

void TableSpace::save(void)
{
	PStatement statement = database->prepareStatement(
		"replace into system.tablespaces (tablespace,tablespace_id,filename,status) values (?,?,?,?)");
	int n = 1;
	statement->setString(n++, name);
	statement->setInt(n++, tableSpaceId);
	statement->setString(n++, filename);
	statement->setInt(n++, type);
	statement->executeUpdate();
	needSave = false;
}

void TableSpace::getIOInfo(InfoTable* infoTable)
{
	int n = 0;
	infoTable->putString(n++, name);
	infoTable->putInt(n++, dbb->pageSize);
	infoTable->putInt(n++, dbb->cache->numberBuffers);
	infoTable->putInt(n++, dbb->reads);
	infoTable->putInt(n++, dbb->writes);
	infoTable->putInt(n++, dbb->fetches);
	infoTable->putInt(n++, dbb->fakes);
	infoTable->putRecord();
}

void TableSpace::close(void)
{
	dbb->close();
	active = false;
}
