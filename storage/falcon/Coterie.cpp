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

// Coterie.cpp: implementation of the Coterie class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Coterie.h"
#include "CoterieRange.h"
#include "Database.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Coterie::Coterie(Database *db, const char *coterieName) : PrivilegeObject (db)
{
	name = coterieName;
	schemaName = db->getSymbol ("");
	ranges = NULL;
}

Coterie::~Coterie()
{
	clear();
}

bool Coterie::validateAddress(int32 address)
{
	for (CoterieRange *range = ranges; range; range = range->next)
		if (range->validateAddress (address))
			return true;

	//return true;
	return false;
}


void Coterie::clear()
{
	for (CoterieRange *range; (range = ranges);)
		{
		ranges = range->next;
		delete range;
		}
}

void Coterie::replaceRanges(CoterieRange *newRanges)
{
	clear();
	ranges = newRanges;
}

void Coterie::addRange(const char *from, const char *to)
{
	CoterieRange *range = new CoterieRange (from, to);
	range->next = ranges;
	ranges = range;
}

PrivObject Coterie::getPrivilegeType()
{
	return PrivCoterie;
}
