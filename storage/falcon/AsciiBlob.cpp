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

// AsciiBlob.cpp: implementation of the AsciiBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */


#include "Engine.h"
#include "AsciiBlob.h"
#include "BinaryBlob.h"
#include "SQLException.h"
#include "Dbb.h"

#ifdef ENGINE
#include "Dbb.h"
#include "Repository.h"
#include "Section.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AsciiBlob::AsciiBlob()
{
	init (true);
}


AsciiBlob::AsciiBlob(int minSegmentSize) : Stream (minSegmentSize)
{
	init (true);
}

AsciiBlob::AsciiBlob(Blob * blob)
{
	init (true);

	if (blob->dataUnset)
		unsetData();
	else
		Stream::putSegment (blob);

	copy (blob);
}

AsciiBlob::AsciiBlob(Clob *clob)
{
	init (true);
	Stream::putSegment (clob);
	copy (clob);
}

#ifdef ENGINE
AsciiBlob::AsciiBlob(Dbb * db, int32 recNumber, Section *blobSection)
{
	init (false);
	dbb = db;
	recordNumber = recNumber;
	section = blobSection;
}
#endif


void AsciiBlob::init(bool pop)
{
	useCount = 1;
	populated = pop;
	dbb = NULL;
}

AsciiBlob::~AsciiBlob()
{

}

void AsciiBlob::addRef()
{
	++useCount;
}

int AsciiBlob::release()
{
	if (--useCount == 0)
		{
		delete this;
		return 0;
		}

	return useCount;
}

int AsciiBlob::length()
{
	if (dataUnset)
		return -1;

	if (!populated)
		populate();

	return totalLength;
}

void AsciiBlob::getSubString(int32 pos, int32 length, char * address)
{
	if (!populated)
		populate();

	Stream::getSegment (pos, length, address);
}

void AsciiBlob::putSegment(int length, const char * data, bool copyFlag)
{
	Stream::putSegment (length, data, copyFlag);
}

int AsciiBlob::getSegmentLength(int pos)
{
	if (!populated)
		populate();

	return Stream::getSegmentLength (pos);
}

const char* AsciiBlob::getSegment(int pos)
{
	if (!populated)
		populate();

	return (const char*) Stream::getSegment (pos);
}

void AsciiBlob::populate()
{
	if (populated)
		return;

	populated = true;

#ifdef ENGINE
	if (repository && isBlobReference())
		try
			{
			repository->getBlob (this);
			}
		catch (SQLException &exception)
			{
			totalLength = exception.getSqlcode();
			}
	else if (dbb)
		dbb->fetchRecord(section, recordNumber, this);
#endif
}

void AsciiBlob::putSegment(const char *string)
{
	Stream::putSegment (string);
}

void AsciiBlob::putSegment(int32 length, const char *segment)
{
	putSegment(length, segment, true);
}

Stream* AsciiBlob::getStream()
{
	if (dataUnset)
		return NULL;

	if (!populated)
		populate();

	return this;
}

void AsciiBlob::unsetData()
{
	dataUnset = true;
}

