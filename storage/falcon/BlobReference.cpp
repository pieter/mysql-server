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

// BlobReference.cpp: implementation of the BlobReference class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BlobReference.h"

#ifdef ENGINE
#include "Stream.h"
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BlobReference::BlobReference()
{
	repository = NULL;
	dataUnset = false;
}

BlobReference::~BlobReference()
{

}

void BlobReference::unsetData()
{
	dataUnset = true;
}

bool BlobReference::isBlobReference()
{
	return !repositoryName.IsEmpty();
}

void BlobReference::setBlobReference(JString repo, int volume, int64 id)
{
	repositoryName = repo;
	repositoryVolume = volume;
	blobId = id;
}

void BlobReference::unsetBlobReference()
{
	repositoryName = "";
}


#ifdef ENGINE
void BlobReference::getReference(Stream *stream)
{
	stream->putSegment ((const char*) repositoryName);
	int n;

	for (n = 0; n < 32; n += 8)
		stream->putCharacter ((char) (repositoryVolume >> n));

	for (n = 0; n < 64; n += 8)
		stream->putCharacter ((char) (blobId >> n));
}

void BlobReference::setReference(int length, Stream *stream)
{
	int l = length - (4 + 8);
	char *p = repositoryName.getBuffer (l);
	stream->getSegment (0, l, p);
	repositoryName.releaseBuffer();
	char temp [12];
	stream->getSegment (l, l + sizeof (temp), temp);
	p = temp;
	repositoryVolume = 0;
	blobId = 0;
	int n;

	for (n = 0; n < 32; n += 8)
		repositoryVolume |= (*p++ & 0xff) << n;

	for (n = 0; n < 64; n += 8)
		blobId |= (*p++ & 0xff) << n;
}


Stream* BlobReference::getStream()
{
	return NULL;
}

void BlobReference::setRepository(Repository *repo)
{
	repository = repo;
}
#endif

void BlobReference::setReference(int length, UCHAR *buffer)
{
	int l = length - (4 + 8);
	repositoryName = JString ((char*)buffer, l);
	UCHAR *p = buffer + l;
	blobId = 0;
	repositoryVolume = 0;
	int n;

	for (n = 0; n < 32; n += 8)
		repositoryVolume |= (*p++ & 0xff) << n;

	for (n = 0; n < 64; n += 8)
		blobId |= (*p++ & 0xff) << n;
}

void BlobReference::copy(BlobReference *source)
{
	repository = source->repository;
	repositoryName = source->repositoryName;
	repositoryVolume = source->repositoryVolume;
	blobId = source->blobId;
}

int BlobReference::getReferenceLength()
{
	return repositoryName.length() + 4 + 8;
}

int BlobReference::getReference(int size, UCHAR *buffer)
{
	if (repositoryName.IsEmpty())
		return 0;

	UCHAR *q = buffer;
	UCHAR *end = buffer + size;

	for (const char *p = repositoryName; *p && q < end;)
		*q++ = *p++;
	int n;

	if (q + sizeof (int) + sizeof (int64) >= end)
		return -1;

	for (n = 0; n < 32; n += 8)
		*q++ = (UCHAR) (repositoryVolume >> n);

	for (n = 0; n < 64; n += 8)
		*q++ = (UCHAR) (blobId >> n);

	return q - buffer;
}
