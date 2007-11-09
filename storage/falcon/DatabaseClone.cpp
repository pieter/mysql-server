/* Copyright (C) 2007 MySQL AB

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

#include "Engine.h"
#include "DatabaseClone.h"
#include "Dbb.h"
#include "Bitmap.h"
#include "Sync.h"
#include "Hdr.h"
#include "PageInventoryPage.h"
#include "BDB.h"
#include "Cache.h"
#include "SerialLog.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

DatabaseClone::DatabaseClone(Dbb *dbb) : DatabaseCopy(dbb)
{
	shadow = NULL;
}

DatabaseClone::~DatabaseClone(void)
{
	delete shadow;
}

void DatabaseClone::close(void)
{
	if (shadow)
		shadow->closeFile();
}

void DatabaseClone::createFile(const char* fileName)
{
	shadow = new IO;
	shadow->pageSize = dbb->pageSize;
	shadow->dbb = dbb;
	shadow->createFile(fileName, 0);
}

const char* DatabaseClone::getFileName(void)
{
	return (shadow) ? shadow->fileName : "unknown";
}

void DatabaseClone::readHeader(Hdr* header)
{
	if (shadow)
		shadow->readHeader(header);
}

void DatabaseClone::writeHeader(Hdr* header)
{
	if (shadow)
		shadow->writeHeader(header);
}

void DatabaseClone::writePage(Bdb* bdb)
{
	if (shadow)
		shadow->writePage(bdb, WRITE_TYPE_CLONE);
}

void DatabaseClone::clone(void)
{
	Sync sync(&syncObject, "DatabaseClone::clone");
	int n = 0;

	for (;;)
		{
		int lastPage = PageInventoryPage::getLastPage(dbb);

		// If we've copied all pages, maybe we can finish up the process
		
		if (n >= lastPage)
			{
			// If any pages were written again after we copied them, copy them again
			
			if (rewrittenPages)
				{
				sync.lock(Exclusive);
				
				for (int32 pageNumber; (pageNumber = rewrittenPages->nextSet(0)) >= 0;)
					{
					rewrittenPages->clear(pageNumber);
					sync.unlock();
					Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, Shared);
					BDB_HISTORY(bdb);
					shadow->writePage(bdb, WRITE_TYPE_CLONE);
					bdb->release(REL_HISTORY);
					sync.lock(Exclusive);
					}

				sync.unlock();
				}
			
			//  In theory, we're done.  Lock the cache against changes, and check again
			
			Sync syncCache(&dbb->cache->syncObject, "Dbb::cloneFile");
			syncCache.lock(Exclusive);
			lastPage = PageInventoryPage::getLastPage(dbb);

			// Check one last time for done.  If anything snuck in, punt and try again
			
			if (n < lastPage || (rewrittenPages && rewrittenPages->nextSet(0) >= 0))
				continue;

			// We got all pages.  Next, update the clone header page
			
			Hdr	header;
			shadow->readHeader(&header);
			header.logOffset = lastPage + 1;
			header.logLength = dbb->serialLog->appendLog(shadow, header.logOffset);
			shadow->writeHeader(&header);

			break;
			}

		for (; n < lastPage; ++n)
			{
			Bdb *bdb = dbb->fetchPage(n, PAGE_any, Shared);
			BDB_HISTORY(bdb);
			highWater = bdb->pageNumber;
			shadow->writePage(bdb, WRITE_TYPE_CLONE);
			bdb->release(REL_HISTORY);
			}
		
		atEnd = true;
		}

}
