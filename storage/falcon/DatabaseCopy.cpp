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
#include "DatabaseCopy.h"
#include "SyncObject.h"
#include "Bitmap.h"
#include "BDB.h"
#include "Sync.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

DatabaseCopy::DatabaseCopy(Dbb *db)
{
	dbb = db;
	rewrittenPages = NULL;
	highWater = 0;
	atEnd = false;
}

DatabaseCopy::~DatabaseCopy(void)
{
	delete rewrittenPages;
}

void DatabaseCopy::close(void)
{
}

void DatabaseCopy::rewritePage(Bdb* bdb)
{
	if (atEnd || (!highWater || bdb->pageNumber <= highWater))
		{
		Sync sync(&syncObject, "DatabaseCopy::rewritePage");
		sync.lock(Exclusive);
		
		if (!rewrittenPages)
			rewrittenPages = new Bitmap;
		
		rewrittenPages->set(bdb->pageNumber);
		}
}
