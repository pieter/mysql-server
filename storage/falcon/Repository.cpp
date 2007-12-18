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

// Repository.cpp: implementation of the Repository class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "Repository.h"
#include "RepositoryVolume.h"
#include "Database.h"
#include "Sequence.h"
#include "Database.h"
#include "RepositoryVolume.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "Field.h"
#include "Value.h"
#include "AsciiBlob.h"
#include "BinaryBlob.h"
#include "SQLError.h"
#include "Dbb.h"
#include "Sync.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Repository::Repository(const char *repositoryName, const char *repositorySchema, Database *db, Sequence *seq, const char *fileName, const char *rollovers, int volume)
{
	database = db;
	sequence = seq;
	name = repositoryName;
	schema = repositorySchema;
	filePattern = fileName;
	currentVolume = volume;
	maxSize = 0;
	rolloverPeriod = RolloverNone;

	if (rollovers)
		setRollover (rollovers);

	memset (volumes, 0, sizeof (volumes));
}

Repository::~Repository()
{
	for (int n = 0; n < VOLUME_HASH_SIZE; ++n)
		for (RepositoryVolume *volume; (volume = volumes [n]);)
			{
			volumes [n] = volume->collision;
			delete volume;
			}
}

Value* Repository::defaultRepository(Field *field, Value *value, Value *alt)
{
	if (field->type == Asciiblob)
		{
		Clob *clob;
		
		switch (value->getType())
			{
			case Null:
				return value;

			case ClobPtr:
				{
				Clob *valueClob = value->getClob();
				
				if (valueClob->isBlobReference())
					{
					valueClob->release();
					return value;
					}
					
				clob = new AsciiBlob (valueClob);
				valueClob->release();
				}
				break;

			case BlobPtr:
				{
				Blob *blob = value->getBlob();
				
				if (blob->isBlobReference())
					{
					blob->release();
					return value;
					}
					
				clob = new AsciiBlob (blob);
				blob->release();
				}
				break;

			case String:
				clob = new AsciiBlob;
				clob->putSegment (value->getStringLength(), value->getString());	
				break;

			default:
				NOT_YET_IMPLEMENTED;
				clob = new AsciiBlob; // fix GCC warning
			}
			
		int64 id = sequence->update (1, NULL);
		clob->setBlobReference (name, currentVolume, id);
		alt->setValue (clob);
		clob->release();	 
		}
	else if (field->type == Binaryblob)
		{
		Blob *blob;
		
		switch (value->getType())
			{
			case Null:
				return value;

			case BlobPtr:
				{
				Blob *valueBlob = value->getBlob();
				
				if (valueBlob->isBlobReference())
					{
					valueBlob->release();
					return value;
					}
				
				blob = new BinaryBlob(valueBlob);
				valueBlob->release();
				}
				break;

			case ClobPtr:
				{
				Clob *clob = value->getClob();
				
				if (clob->isBlobReference())
					{
					clob->release();
					return value;
					}
				
				blob = new BinaryBlob(clob);
				clob->release();
				}
				break;

			case String:
				blob = new BinaryBlob;
				blob->putSegment(value->getStringLength(), value->getString());	
				break;

			default:
				NOT_YET_IMPLEMENTED;
				blob = new BinaryBlob; // fix GCC warning
			}
		int64 id = sequence->update (1, NULL);
		blob->setBlobReference (name, currentVolume, id);
		alt->setValue (blob);
		blob->release();
		}
	else
		NOT_YET_IMPLEMENTED;

	return alt;
}

void Repository::storeBlob(BlobReference *blob, Transaction *transaction)
{
	Stream *stream = blob->getStream();

	if (!stream)
		return;

	if (stream->totalLength < 0)
		throw SQLError (stream->totalLength, "damaged blob used in assignment");

	int volumeNumber = blob->repositoryVolume;

	if (volumeNumber > currentVolume)
		{
		currentVolume = volumeNumber;
		save();
		}

	RepositoryVolume *volume = getVolume (volumeNumber);
	volume->storeBlob (blob, transaction);

	if (maxSize && volumeNumber == currentVolume && volume->getRepositorySize() > maxSize)
		{
		++currentVolume;
		save();
		}
}

RepositoryVolume* Repository::getVolume(int volumeNumber)
{
	Sync sync (&syncObject, "Repository::getVolume");
	sync.lock (Shared);
	RepositoryVolume *volume = findVolume (volumeNumber);

	if (volume)
		return volume;

	sync.unlock();
	sync.lock (Exclusive);

	if ( (volume = findVolume (volumeNumber)) )
		return volume;

	JString fileName = genFileName (volumeNumber);
	volume = new RepositoryVolume (this, volumeNumber, fileName);
	int slot = volumeNumber % VOLUME_HASH_SIZE;
	volume->collision = volumes [slot];
	volumes [slot] = volume;

	return volume;
}

RepositoryVolume* Repository::findVolume(int volumeNumber)
{
	for (RepositoryVolume *volume = volumes [volumeNumber % VOLUME_HASH_SIZE];
		 volume; volume = volume->collision)
		if (volume->volumeNumber == volumeNumber)
			return volume;

	return NULL;
}

JString Repository::genFileName(int volume)
{
	char temp [256], *q = temp;

	for (const char *p = filePattern; *p;)
		{
		char c = *p++;
		
		if (c == '%' && *p)
			{
			c = *p++;
			
			if (c == '%')
				*q++ = c;
			else if (c == 'd')
				{
				const char *path = database->dbb->fileName;
				char *last = NULL;
				
				while (*path)
					{
					if (*path == '.')
						last = q;
						
					*q++ = *path++;
					}
					
				if (last)
					q = last;
				}
			else if (c == 'v')
				{
				sprintf (q, "%d", volume);
				
				while (*q)
					++q;
				}
			else if (c == 's' || c == 'a')
				for (const char *s = schema; *s;)
					*q++ = *s++;
			else if (c == 'r')
				for (const char *s = name; *s;)
					*q++ = *s++;
			else
				throw SQLError (DDL_ERROR, "illegal substitution character \"%c\" in pattern \"%s\" in repository %s\n",
								c, (const char*) filePattern, name);
			}
		else
			*q++ = c;
		}

	*q = 0;

	return temp;
}

void Repository::setSequence(Sequence *seq)
{
	if (seq == sequence)
		return;

	sequence = seq;
}

void Repository::setFilePattern(const char *pattern)
{
	if (filePattern == pattern)
		return;

	filePattern = pattern;
}

void Repository::getBlob(BlobReference *reference)
{
	RepositoryVolume *volume = getVolume (reference->repositoryVolume);
	volume->getBlob (reference);
}

void Repository::setVolume(int volume)
{
	if (volume <= currentVolume)
		return;

	currentVolume = volume;
}


void Repository::save()
{
	Sync sync (&database->syncSysConnection, "Repository::update");
	sync.lock (Shared);
	PreparedStatement *statement = database->prepareStatement (
		"replace into system.repositories (repositoryName,schema,sequenceName,filename,rollovers,currentVolume)"
		"values (?,?,?,?,?,?)");
	int n = 1;
	statement->setString (n++, name);
	statement->setString (n++, schema);
	statement->setString (n++, sequence->name);
	statement->setString (n++, filePattern);
	statement->setString (n++, rolloverString);
	statement->setInt (n++, currentVolume);
	statement->executeUpdate();
	statement->close();
	sync.unlock();
	database->commitSystemTransaction();
}

void Repository::validateRollovers(const char *string)
{
	const char *ptr = string;
	char token [32];

	while (getToken (&ptr, sizeof (token), token))
		if (ISDIGIT (token [0]))
			getFileSize (token);
		else
			getRolloverPeriod (token);
}

bool Repository::getToken(const char **ptr, int sizeToken, char *token)
{
	const char *p = *ptr;

	while (*p == ' ')
		++p;

	if (!*p)
		return false;

	char *q = token;
	char *end = q + sizeToken - 1;

	for (; *p && *p != ' ' && q < end; ++p)
		*q++ = UPPER (*p);

	*q = 0;
	*ptr = p;

	return true;
}

int64 Repository::getFileSize(const char *token)
{
	const char *p = token;
	int64 size = 0;

	while (ISDIGIT (*p))
		size = size * 10 + *p++ - '0';

	if (strcmp (p, "GB") == 0)
		size *= 1000000000;
	else if (strcmp (p, "MB") == 0)
		size *= 1000000;
	else if (strcmp (p, "B") == 0)
		;
	else if (*p)
		throw SQLError (DDL_ERROR, "don't recognize file size string \"%s\"\n", token);

	return size;	
}

RolloverPeriod Repository::getRolloverPeriod(const char *token)
{
	if (strcmp (token, "MONTHLY") == 0)
		return RolloverMonthly;

	if (strcmp (token, "WEEKLY") == 0)
		return RolloverWeekly;

	throw SQLError (DDL_ERROR, "don't recognize rollover type \"%s\"\n", token);
}

void Repository::setRollover(const char *string)
{
	if (rolloverString == string)
		return;

	maxSize = 0;
	rolloverPeriod = RolloverNone;
	const char *ptr = string;
	char token [32];

	while (getToken (&ptr, sizeof (token), token))
		if (ISDIGIT (token [0]))
			maxSize = getFileSize (token);
		else
			rolloverPeriod = getRolloverPeriod (token);

	rolloverString = string;
}

void Repository::drop()
{
	Sync sync (&database->syncSysConnection, "Repository::drop");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.repositories where schema=? and repositoryName=?");
	statement->setString (1, schema);
	statement->setString (2, name);
	statement->executeUpdate();
	statement->close();
	sync.unlock();		
	database->commitSystemTransaction();	
}

void Repository::deleteBlob(int volumeNumber, int64 blobId, Transaction *transaction)
{
	RepositoryVolume *volume = getVolume (volumeNumber);
	volume->deleteBlob (blobId, transaction);
}

void Repository::scavenge()
{
	for (int n = 0; n < VOLUME_HASH_SIZE; ++n)
		for (RepositoryVolume *volume = volumes [n]; volume; volume = volume->collision)
			volume->scavenge();
}

void Repository::synchronize(const char *fileName, Transaction *transaction)
{
	RepositoryVolume *source = NULL;

	try
		{
		source = new RepositoryVolume (this, 0, fileName);
		source->synchronize(transaction);
		}
	catch (...)
		{
		delete source;
		throw;
		}

	delete source;
}

void Repository::reportStatistics()
{
	for (int n = 0; n < VOLUME_HASH_SIZE; ++n)
		for (RepositoryVolume *volume = volumes [n]; volume; volume = volume->collision)
			volume->reportStatistics();
}

void Repository::close()
{
	for (int n = 0; n < VOLUME_HASH_SIZE; ++n)
		for (RepositoryVolume *volume = volumes [n]; volume; volume = volume->collision)
			volume->close();
}
