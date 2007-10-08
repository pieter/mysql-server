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


// IO.cpp: implementation of the IO class.
//
//////////////////////////////////////////////////////////////////////

#define _FILE_OFFSET_BITS	64

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define LSEEK		_lseeki64
#define SEEK_OFFSET	int64
#define MKDIR(dir)			mkdir (dir)

#else
#include <sys/types.h>
#include <aio.h>
#include <unistd.h>
#include <signal.h>

#ifdef STORAGE_ENGINE
#include "config.h"
#endif

#ifdef TARGET_OS_LINUX
#include <linux/unistd.h>
#else
#define   LOCK_SH   1	/* shared lock */
#define   LOCK_EX   2	/* exclusive lock */
#define   LOCK_NB   4	/* don't block when locking */
#define   LOCK_UN   8	/* unlock */
#endif
#include <sys/file.h>
#define O_BINARY		0
#define O_RANDOM		0
#define MKDIR(dir)			mkdir (dir, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP)
#endif

#ifndef LSEEK
#define LSEEK		lseek
#define SEEK_OFFSET	off_t
#endif

#ifndef S_IRGRP
#define S_IRGRP		0
#define S_IWGRP		0
#endif

#ifndef PATH_MAX
#define PATH_MAX		256
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include "Engine.h"
#include <string.h>
#include <errno.h>
#include "IOx.h"
#include "BDB.h"
#include "Hdr.h"
#include "SQLError.h"
#include "Sync.h"
#include "Log.h"
#include "Debug.h"
#include "Synchronize.h"

#define TRACE_FILE	"falcon.trace"

static const int TRACE_SYNC_START	= -1;
static const int TRACE_SYNC_END		= -2;

static FILE	*traceFile;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IO::IO()
{
	fileId = -1;
	reads = writes = fetches = fakes = 0;
	priorReads = priorWrites = priorFetches = priorFakes = 0;
	dbb = NULL;
	fatalError = false;
	memset(writeTypes, 0, sizeof(writeTypes));
}

IO::~IO()
{
	traceClose();
	closeFile();
}

bool IO::openFile(const char * name, bool readOnly)
{
	fileName = name;
	fileId = ::open (fileName, (readOnly) ? O_RDONLY | O_BINARY : O_RDWR | O_BINARY);

	if (fileId < 0)
		throw SQLEXCEPTION (CONNECTION_ERROR, "can't open file \"%s\": %s (%d)", 
							name, strerror (errno), errno);

#ifndef _WIN32
	signal (SIGXFSZ, SIG_IGN);

#ifndef STORAGE_ENGINE
	if (flock (fileId, (readOnly) ? LOCK_SH : LOCK_EX))
		{
		::close (fileId);
		throw SQLEXCEPTION (CONNECTION_ERROR, "file \"%s\" in use by another process", name);
		}
#endif
#endif

	//Log::debug("IO::openFile %s (%d) fd: %d\n", (const char*) fileName, readOnly, fileId);
	
	return fileId != -1;
}

bool IO::createFile(const char *name, uint64 initialAllocation)
{
	Log::debug("IO::createFile: creating file \"%s\"\n", name);

	fileName = name;
	fileId = ::open (fileName,
					O_CREAT | O_RDWR | O_RANDOM | O_TRUNC | O_BINARY,
					S_IREAD | S_IWRITE | S_IRGRP | S_IWGRP);

	if (fileId < 0)
		throw SQLEXCEPTION (CONNECTION_ERROR, "can't create file \"%s\", %s (%d)", 
								name, strerror (errno), errno);

#ifndef _WIN32
#ifndef STORAGE_ENGINE
	flock(fileId, LOCK_EX);
#endif
#endif

	if (initialAllocation)
		{
		UCHAR *raw = new UCHAR[8192 * 257];
		UCHAR *aligned = (UCHAR*) (((UIPTR) raw + 8191) / 8192 * 8192);
		uint size = 8192 * 256;
		memset(aligned, 0, size);
		uint64 offset = 0;
		
		for (uint64 remaining = initialAllocation; remaining;)
			{
			uint n = (int) MIN(remaining, size);
			write(offset, n, aligned);
			offset += n;
			remaining -= n;
			}
		
		delete [] raw;
		sync();
		}

	return fileId != -1;
}

void IO::readPage(Bdb * bdb)
{
	if (fatalError)
		FATAL ("can't continue after fatal error");

	SEEK_OFFSET offset = (int64) bdb->pageNumber * pageSize;
	int length = pread (offset, pageSize, (UCHAR *)bdb->buffer);

	if (length != pageSize)
		{
		declareFatalError();
		FATAL ("read error on page %d of \"%s\": %s (%d)",
				bdb->pageNumber, (const char*) fileName, strerror (errno), errno);
		}

	++reads;
}

bool IO::trialRead(Bdb *bdb)
{
	Sync sync (&syncObject, "IO::trialRead");
	sync.lock (Exclusive);

	seek (bdb->pageNumber);
	int length = ::read (fileId, bdb->buffer, pageSize);

	if (length != pageSize)
		return false;

	++reads;

	return true;
}

void IO::writePage(Bdb * bdb, int type)
{
	if (fatalError)
		FATAL ("can't continue after fatal error");

	ASSERT(bdb->pageNumber != HEADER_PAGE || ((Page*)(bdb->buffer))->pageType == PAGE_header);
	tracePage(bdb);
	SEEK_OFFSET offset = (int64) bdb->pageNumber * pageSize;
	int length = pwrite (offset, pageSize, (UCHAR *)bdb->buffer);

	if (length != pageSize)
		{
		declareFatalError();
		FATAL ("write error on page %d (%d/%d/%d) of \"%s\": %s (%d)",
				bdb->pageNumber, length, pageSize, fileId,
				(const char*) fileName, strerror (errno), errno);
		}

	++writes;
	++writesSinceSync;
	++writeTypes[type];
}

void IO::readHeader(Hdr * header)
{
	int n = lseek (fileId, 0, SEEK_SET);
	n = ::read (fileId, header, sizeof (Hdr));

	if (n != sizeof (Hdr))
		FATAL ("read error on database header");
}

void IO::closeFile()
{
	if (fileId != -1)
		{
		::close (fileId);
		//Log::debug("IO::closeFile %s fd: %d\n", (const char*) fileName, fileId);
		fileId = -1;
		}
}

void IO::seek(int pageNumber)
{
	SEEK_OFFSET pos = (int64) pageNumber * pageSize;
	SEEK_OFFSET result = LSEEK (fileId, pos, SEEK_SET);

	if (result != pos)
		Error::error ("long seek failed on page %d of \"%s\"", 
					  pageNumber, (const char*) fileName);
}


void IO::longSeek(int64 offset)
{
	SEEK_OFFSET result = LSEEK (fileId, offset, SEEK_SET);

	if (result != offset)
		Error::error ("long seek failed on  \"%s\"", (const char*) fileName);
}

void IO::declareFatalError()
{
	fatalError = true;
}

void IO::createPath(const char *fileName)
{
	// First, better make sure directories exists

	char directory [256], *q = directory;

	for (const char *p = fileName; *p;)
		{
		char c = *p++;
		
		if (c == '/' || c == '\\')
			{
			*q = 0;
			
			if (q > directory && q [-1] != ':')
				if (MKDIR (directory) && errno != EEXIST)
					throw SQLError (IO_ERROR, "can't create directory \"%s\"\n", directory);
			}
		*q++ = c;
		}
}

void IO::expandFileName(const char *fileName, int length, char *buffer)
{
#ifdef _WIN32
	char expandedName [PATH_MAX], *baseName;
	GetFullPathName (fileName, sizeof (expandedName), expandedName, &baseName);
	const char *path = expandedName;
#else
	char expandedName [PATH_MAX];
	expandedName [0] = 0;
	const char *path = realpath (fileName, expandedName);

	if (!path)
		if (errno == ENOENT && expandedName [0])
			path = expandedName;
		else
			path = fileName;
#endif
	if ((int) strlen (path) >= length)
		throw SQLError (IO_ERROR, "expanded filename exceeds buffer length\n");

	strcpy (buffer, path);
}

bool IO::doesFileExits(const char *fileName)
{
	struct stat stats;

	return stat(fileName, &stats) == 0;
}

void IO::write(uint32 length, const UCHAR *data)
{
	uint32 len = ::write (fileId, data, length);

	if (len != length)
		throw SQLError(IO_ERROR, "bad write length, %d of %d requested", len, length);
}

int IO::read(int length, UCHAR *buffer)
{
	return ::read(fileId, buffer, length);
}

void IO::writeHeader(Hdr *header)
{
	int n = lseek (fileId, 0, SEEK_SET);
	n = ::write (fileId, header, sizeof (Hdr));

	if (n != sizeof (Hdr))
		FATAL ("write error on database clone header");
}

void IO::deleteFile()
{
	deleteFile(fileName);
}

int IO::pread(int64 offset, int length, UCHAR* buffer)
{
	int ret;

#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	ret = ::pread (fileId, buffer, length, offset);
#else
	Sync sync (&syncObject, "IO::pread");
	sync.lock (Exclusive);

	longSeek(offset);
	ret = (int) read(length, buffer);
#endif

	DEBUG_FREEZE;

	return ret;
}

void IO::read(int64 offset, int length, UCHAR* buffer)
{
	Sync sync (&syncObject, "IO::read");
	sync.lock (Exclusive);

	longSeek(offset);
	int len = read (length, buffer);

	if (len != length)
		throw SQLError(IO_ERROR, "read error at " I64FORMAT "%d of \"%s\": %s (%d)",
					   offset, (const char*) fileName, strerror (errno), errno);
}

int IO::pwrite(int64 offset, int length, const UCHAR* buffer)
{
	int ret;
	
#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	ret = ::pwrite (fileId, buffer, length, offset);
#else
	Sync sync (&syncObject, "IO::pwrite");
	sync.lock (Exclusive);

	longSeek(offset);
	ret = (int) ::write (fileId, buffer, length);
#endif

	DEBUG_FREEZE;

	return ret;
}

void IO::write(int64 offset, int length, const UCHAR* buffer)
{
	Sync sync (&syncObject, "IO::write");
	sync.lock (Exclusive);

	longSeek(offset);
	write (length, buffer);
}

void IO::sync(void)
{
	if (traceFile)
		traceOperation(TRACE_SYNC_START);
		
#ifdef _WIN32
	if (_commit(fileId))
		{
		declareFatalError();
		FATAL ("_commit failed on \"%s\": %s (%d)",
				(const char*) fileName, strerror (errno), errno);
		}
	
#else
#ifdef _POSIX_SYNCHRONIZED_IO_XXX
	aiocb ocb;
	bzero(&ocb, sizeof(ocb));
	ocb.aio_fildes = fileId;
	ocb.aio_sigevent.sigev_notify = SIGEV_NONE;
	int ret = aio_fsync(O_DSYNC, &ocb);

	if (ret == -1)
		{
		declareFatalError();
		FATAL ("aio_fsync error on \"%s\": %s (%d)",
				(const char*) fileName, strerror (errno), errno);
		}
		
	int iterations = 0;

	while ( (ret = aio_error(&ocb)) == EINPROGRESS)
		++iterations;

	if ( (ret = aio_return(&ocb)) )
		{
		int error = aio_error(&ocb);
		declareFatalError();
		FATAL ("aio_fsync final error on \"%s\": %s (%d)",
				(const char*) fileName, strerror(error), error);
		}
		
#else
	//Sync sync (&syncObject, "IO::sync");
	//sync.lock(Exclusive);
	fsync(fileId);
#endif
#endif

	writesSinceSync = 0;
	
	if (traceFile)
		traceOperation(TRACE_SYNC_END);
}

void IO::deleteFile(const char* fileName)
{
	unlink(fileName);
}

void IO::tracePage(Bdb* bdb)
{
	Page *page = bdb->buffer;
	int id = Debug::getPageId(page);
	trace(fileId, bdb->pageNumber, page->pageType, id);
}

void IO::trace(int fd, int pageNumber, int pageType, int pageId)
{
	if (traceFile)
		fprintf(traceFile, "%d %d %d %d\n", fd, pageNumber, pageType, pageId);
}

void IO::traceOpen(void)
{
#ifdef TRACE_FILE
	if (!traceFile)
		traceFile = fopen(TRACE_FILE, "w");
#endif
}

void IO::traceClose(void)
{
	if (traceFile)
		{
		fclose(traceFile);
		traceFile = NULL;
		}
}

void IO::traceOperation(int operation)
{
	trace(fileId, operation, 0, 0);
}

void IO::reportWrites(void)
{
	Log::debug("%s flush :%d, pure: %d, prec %d, reuse %d, pgwrt %d\n",
		(const char*) fileName,
		writeTypes[WRITE_TYPE_FLUSH],
		writeTypes[WRITE_TYPE_PURIFIER],
		writeTypes[WRITE_TYPE_PRECEDENCE],
		writeTypes[WRITE_TYPE_REUSE],
		writeTypes[WRITE_TYPE_PAGE_WRITER]);
}
