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

// BinaryBlob.cpp: implementation of the BinaryBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */


#include "Engine.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
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


BinaryBlob::BinaryBlob()
{
	init (true);
}

BinaryBlob::BinaryBlob(int minSegmentSize) : Stream (minSegmentSize)
{
	init (true);
}

#ifdef ENGINE
BinaryBlob::BinaryBlob(Dbb * db, int32 recNumber, Section *blobSection)
{
	init (false);
	dbb = db;
	recordNumber = recNumber;
	section = blobSection;
}
#endif

BinaryBlob::BinaryBlob(Clob * blob)
{
	init (true);

	if (blob->dataUnset)
		unsetData();
	else
		Stream::putSegment (blob);

	copy (blob);
}

BinaryBlob::BinaryBlob(Blob *blob)
{
	init (true);

	//Stream::putSegment (blob);
	if (blob->dataUnset)
		unsetData();
	else
		Stream::putSegment (blob);

	copy (blob);
}

void BinaryBlob::init(bool pop)
{
	useCount = 1;
	offset = 0;
	populated = pop;
	dbb = NULL;
}

BinaryBlob::~BinaryBlob()
{

}

void BinaryBlob::addRef()
{
	++useCount;
}

int BinaryBlob::release()
{
	if (--useCount == 0)
		{
		delete this;
		return 0;
		}

	return useCount;
}

void BinaryBlob::getBytes(int32 pos, int32 length, void * address)
{
	if (!populated)
		populate();

	Stream::getSegment (pos, length, address);
}

int BinaryBlob::length()
{
	if (dataUnset)
		return -1;

	if (!populated)
		populate();

	return totalLength;
}

void BinaryBlob::putSegment(int length, const char * data, bool copyFlag)
{
	Stream::putSegment (length, data, copyFlag);
}


int BinaryBlob::getSegmentLength(int pos)
{
	if (!populated)
		populate();

	return Stream::getSegmentLength (pos);
}

void* BinaryBlob::getSegment(int pos)
{
	return Stream::getSegment (pos);
}


void BinaryBlob::populate()
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

void BinaryBlob::putSegment(Blob * blob)
{
	Stream::putSegment (blob);
}

void BinaryBlob::putSegment(Clob * blob)
{
	Stream::putSegment(blob);
}

void BinaryBlob::putSegment(int32 length, const void *buffer)
{
	putSegment(length, (const char*) buffer, true);
}

Stream* BinaryBlob::getStream()
{
	if (dataUnset)
		return NULL;

	if (!populated)
		populate();

	return this;
}

void BinaryBlob::unsetData()
{
	dataUnset = true;
}

