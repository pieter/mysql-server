/* Copyright (C) 2008 MySQL AB

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
#include "BackLog.h"
#include "Database.h"
#include "Dbb.h"
#include "Section.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "Transaction.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

BackLog::BackLog(Database *db)
{
	database = db;
	dbb = new Dbb(database);
	section = new Section(dbb, 0, NO_TRANSACTION);
}

BackLog::~BackLog(void)
{
	delete section;
	delete dbb;
}
