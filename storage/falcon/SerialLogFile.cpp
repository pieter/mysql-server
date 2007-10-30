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

// SerialLogFile.cpp: implementation of the SerialLogFile class.
//
//////////////////////////////////////////////////////////////////////

#define _FILE_OFFSET_BITS	64

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define WRITE_MODE			0
#else

#ifdef STORAGE_ENGINE
#include "config.h"
#endif

#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "Engine.h"
#include "SerialLogFile.h"
#include "SerialLog.h"
#include "Sync.h"
#include "SQLError.h"
#include "Database.h"
#include "Log.h"


#ifndef O_BINARY
#define O_BINARY	0
#endif

#ifndef WRITE_MODE
#define WRITE_MODE			O_DIRECT
//#define WRITE_MODE			O_SYNC
#endif

extern uint	falcon_serial_log_priority;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


SerialLogFile::SerialLogFile(Database *db)
{
	database = db;
	handle = 0;
	offset = 0;
	highWater = 0;
	writePoint = 0;
}

SerialLogFile::~SerialLogFile()
{
	close();
}

void SerialLogFile::open(JString filename, bool create)
{
#ifdef _WIN32
	handle = 0;
	char pathName[1024];
	char *ptr;
	int n = GetFullPathName(filename, sizeof(pathName), pathName, &ptr);

	handle = CreateFile(pathName,
						GENERIC_READ | GENERIC_WRITE,
						0,							// share mode
						NULL,						// security attributes
						(create) ? CREATE_ALWAYS : OPEN_ALWAYS,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
						0);

	if (handle == INVALID_HANDLE_VALUE)
		throw SQLError(IO_ERROR, "can't open serial log file \"%s\", error code %d", (const char*) pathName, GetLastError());

	fileName = pathName;
	char *p = strchr(pathName, '\\');
	
	if (p)
		p[1] = 0;

	DWORD	sectorsPerCluster;
	DWORD	bytesPerSector;
	DWORD	numberFreeClusters;
	DWORD	numberClusters;

	if (!GetDiskFreeSpace(pathName, &sectorsPerCluster, &bytesPerSector, &numberFreeClusters, &numberClusters))
		throw SQLError(IO_ERROR, "GetDiskFreeSpace failed for \"%s\"", (const char*) pathName);

	sectorSize = bytesPerSector;
	//fileLength = ROUNDUP(fileLength, sectorSize);
#else

	if (create)
		handle = ::open(filename,  WRITE_MODE | O_RDWR | O_BINARY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	else
		handle = ::open(filename, WRITE_MODE | O_RDWR | O_BINARY);

	if (handle <= 0)		
		throw SQLEXCEPTION (IO_ERROR, "can't open file \"%s\": %s (%d)", 
							(const char*) filename, strerror (errno), errno);

	fileName = filename;
	sectorSize = 4096;
#endif

	if (create)
		zap();
}

void SerialLogFile::close()
{
#ifdef _WIN32
	if (handle)
		{
		CloseHandle(handle);
		handle = 0;
		}
#else
	if (handle > 0)
		{
		::close(handle);
		handle = 0;
		}
#endif
}

void SerialLogFile::write(int64 position, uint32 length, const SerialLogBlock *data)
{
	uint32 effectiveLength = ROUNDUP(length, sectorSize);
    time_t start = database->timestamp;
	Sync syncIO(&database->syncSerialLogIO, "SerialLogFile::write");
	
	if (!(position == writePoint || position == 0 || writePoint == 0))
		throw SQLError(IO_ERROR, "serial log left in inconsistent state");
	
	if (falcon_serial_log_priority)
		syncIO.lock(Exclusive);
		
#ifdef _WIN32
	
	Sync sync(&syncObject, "SerialLogFile::write");
	sync.lock(Exclusive);
	
	if (position != offset)
		{
		LARGE_INTEGER pos;
		pos.QuadPart = position;

		if (!SetFilePointerEx(handle, pos, NULL, FILE_BEGIN))
			throw SQLError(IO_ERROR, "serial log SetFilePointerEx failed with %d", GetLastError());
		}

	DWORD ret;
	
	if (!WriteFile(handle, data, effectiveLength, &ret, NULL))
		throw SQLError(IO_ERROR, "serial log WriteFile failed with %d", GetLastError());

#else

#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	uint32 n = ::pwrite (handle, data, effectiveLength, offset);
#else
	Sync sync (&syncObject, "IO::pwrite");
	sync.lock (Exclusive);

	longSeek(offset);
	uint32 n = ::write(handle, data, effectiveLength);

	if (position != offset)
		{
		off_t loc = lseek(handle, position, SEEK_SET);
		
		if (loc != position)
			throw SQLEXCEPTION (IO_ERROR, "serial lseek error on \"%s\": %s (%d)", 
								(const char*) fileName, strerror (errno), errno);

		}

	uint32 n = ::write(handle, data, effectiveLength);
#endif

	if (n != effectiveLength)
		throw SQLEXCEPTION (IO_ERROR, "serial write error on \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);
#endif

	time_t delta = database->timestamp - start;

	if (delta > 1)
		Log::debug("Serial log write took %d seconds\n", delta);

	offset = position + effectiveLength;
	writePoint = offset;
	highWater = offset;
}

uint32 SerialLogFile::read(int64 position, uint32 length, UCHAR *data)
{
	uint32 effectiveLength = ROUNDUP(length, sectorSize);

#ifdef _WIN32
	Sync sync(&syncObject, "SerialLogFile::read");
	sync.lock(Exclusive);
	ASSERT(position < writePoint || writePoint == 0);
	LARGE_INTEGER pos;
	pos.QuadPart = position;
	
	if (!SetFilePointerEx(handle, pos, NULL, FILE_BEGIN))
		throw SQLError(IO_ERROR, "serial log SetFilePointer failed with %d", GetLastError());

	DWORD ret;

	if (!ReadFile(handle, data, effectiveLength, &ret, NULL))
		throw SQLError(IO_ERROR, "serial log ReadFile failed with %d", GetLastError());

	offset = position + effectiveLength;
	highWater = MAX(offset, highWater);
	
	return ret;
#else

#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	int n = ::pread (handle, data, effectiveLength, position);
#else
	Sync sync(&syncObject, "SerialLogFile::read");
	sync.lock(Exclusive);
	ASSERT(position < writePoint || writePoint == 0);
	off_t loc = lseek(handle, position, SEEK_SET);

	if (loc != position)
		throw SQLEXCEPTION (IO_ERROR, "serial lseek error on \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);
		
	int n = ::read(handle, data, effectiveLength);
#endif

	if (n < 0)
		throw SQLEXCEPTION (IO_ERROR, "serial read error on \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);

	offset = position + n;
	highWater = MAX(offset, highWater);

	return n;
#endif
}


void SerialLogFile::zap()
{
	UCHAR *junk = new UCHAR[sectorSize];
	memset(junk, 0, sectorSize);
	//write(0, sectorSize, (SerialLogBlock*) junk);
	delete junk;
}

void SerialLogFile::dropDatabase()
{
	close();
	unlink(fileName);
}
