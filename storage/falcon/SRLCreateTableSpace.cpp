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

// SRLCreateTableSpace.cpp: implementation of the SRLCreateTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SRLCreateTableSpace.h"
#include "TableSpace.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateTableSpace::SRLCreateTableSpace()
{

}

SRLCreateTableSpace::~SRLCreateTableSpace()
{

}

void SRLCreateTableSpace::append(TableSpace *tableSpace)
{
	START_RECORD(srlCreateTableSpace, "SRLData::append");
	putInt(tableSpace->tableSpaceId);
	const char *p = tableSpace->name;
	int len = (int) strlen(p);
	putInt(len);
	putData(len, (const UCHAR*) p);
	p = tableSpace->filename;
	len = (int) strlen(p);
	putInt(len);
	putData(len, (const UCHAR*) p);
}

void SRLCreateTableSpace::read()
{
	tableSpaceId = getInt();
	nameLength = getInt();
	name = (const char*) getData(nameLength);
	filenameLength = getInt();
	filename = (const char*) getData(filenameLength);
}

void SRLCreateTableSpace::pass1()
{

}

void SRLCreateTableSpace::pass2()
{

}

void SRLCreateTableSpace::commit()
{

}

void SRLCreateTableSpace::redo()
{

}
