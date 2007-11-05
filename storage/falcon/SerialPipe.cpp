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

// SerialPipe.cpp: implementation of the SerialPipe class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "Engine.h"
#include "SerialPipe.h"
#include "Sync.h"
#include "SQLError.h"
#include "Thread.h"

#ifndef O_SYNC
#define O_SYNC		0
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

static const int32 crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialPipe::SerialPipe(const char *filename, int filelength, int windowSize, int buffLength)
{
	fileName = filename;
	fileLength = filelength;
	bufferLength = buffLength + sizeof(SerialHeader);
	resetPositions();
	msgNumber = 0;
	buffer = NULL;
	msgLengths = 0;
	windowLength = windowSize;
	window = NULL;
	open();
}

SerialPipe::~SerialPipe()
{
	close();

#ifdef _WIN32

	if (buffer)
		VirtualFree(buffer, bufferLength, MEM_RELEASE);

	if (window)
		VirtualFree(window, windowLength, MEM_RELEASE);
#else
	close();
	delete [] buffer;
	delete [] window;
#endif

	delete [] msgLengths;
}

void SerialPipe::putMsg(bool waitFlag, int numberBuffers, ...)
{
	va_list		args;
	va_start	(args, numberBuffers);
	Sync sync(&syncObject, "SerialPipe::putMsg");
	sync.lock(Exclusive);

	if (readPosition == writePosition)
		resetPositions();

	uint32 totalLength = sizeof (SerialHeader);
	int segmentLengths[10];
	void *segments[10];
	int n;

	for (n = 0; n < numberBuffers; ++n)
		{
		int len = segmentLengths[n] = va_arg(args, int);
		totalLength += len;
		segments[n] = va_arg(args, void*);
		}

	va_end(args);

	if (totalLength > bufferLength)
		throw SQLError(IO_ERROR, "attempted buffer overrun for serial pipe");

	SerialMessage *message = buffer;
	
	if (writePosition >= windowOffset || 
		(writePosition + totalLength <= windowOffset + windowLength))
		message = (SerialMessage*) (window + writePosition - windowOffset);

	UCHAR *p = message->data;

	for (n = 0; n < numberBuffers; ++n)
		{
		memcpy(p, segments[n], segmentLengths[n]);
		p += segmentLengths[n];
		}

	message->msgNumber = ++msgNumber;
	message->readPosition = readPosition;
	uint32 extendedLength = ROUNDUP(totalLength, sectorSize);

	if (extendedLength > fileLength - writePosition)
		{
		if (writePosition < fileLength)
			{
			message->msgLength = sizeof(SerialHeader);
			message->msgCrc = computeCrc(message);
			write(writePosition, sectorSize, message);
			}
		writePosition = 0;
		}

	message->msgLength = totalLength;
	message->msgCrc = computeCrc(message);
	write(writePosition, extendedLength, message);
	writePosition += extendedLength;
	sync.unlock();
	readers.post();
}

void SerialPipe::open()
{
	bool sync = false;

#ifdef _WIN32
	handle = 0;
	char pathName[1024];
	char *ptr;
	int n = GetFullPathName(fileName, sizeof(pathName), pathName, &ptr);

	handle = CreateFile(fileName,
						GENERIC_READ | GENERIC_WRITE,
						0,							// share mode
						NULL,						// security attributes
						CREATE_NEW, //OPEN_ALWAYS,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
						0);

	if (handle == INVALID_HANDLE_VALUE)
		{
		sync = true;
		handle = CreateFile(fileName,
						GENERIC_READ | GENERIC_WRITE,
						0,							// share mode
						NULL,						// security attributes
						OPEN_ALWAYS,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
						0);
		}

	if (handle == INVALID_HANDLE_VALUE)
		throw SQLError(IO_ERROR, "can't open serial pipe \"%s\"", (const char*) fileName);

	char *p = strchr(pathName, '\\');
	
	if (p)
		p[1] = 0;

	DWORD	sectorsPerCluster;
	DWORD	bytesPerSector;
	DWORD	numberFreeClusters;
	DWORD	numberClusters;

	if (!GetDiskFreeSpace(pathName, &sectorsPerCluster, &bytesPerSector, &numberFreeClusters, &numberClusters))
		throw SQLError(IO_ERROR, "GetDiskFreeSpace failed for \"%s\"", (const char*) fileName);

	sectorSize = bytesPerSector;
	fileLength = ROUNDUP(fileLength, sectorSize);
	bufferLength = ROUNDUP(bufferLength, sectorSize);
	buffer = (SerialMessage*) VirtualAlloc(NULL, bufferLength, MEM_COMMIT, PAGE_READWRITE);

	if (buffer == NULL)
		throw SQLError(IO_ERROR, "VirtualAlloc failed for \"%s\"", (const char*) fileName);

	windowLength = ROUNDUP(windowLength, sectorSize);
	window = (UCHAR*) VirtualAlloc(NULL, windowLength, MEM_COMMIT, PAGE_READWRITE);

	if (window == NULL)
		throw SQLError(IO_ERROR, "VirtualAlloc failed for \"%s\"", (const char*) fileName);

#else
	
	if ((fd = ::open(fileName, O_SYNC | O_RDWR | O_BINARY)) > 0)
		sync = true;
	else if ((fd = ::open(fileName, 
							   O_SYNC | O_RDWR | O_BINARY | O_CREAT, 
							   S_IRUSR | S_IWUSR)) < 0)
		throw SQLEXCEPTION (IO_ERROR, "can't open file \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);

	sectorSize = 512;
	fileLength = ROUNDUP(fileLength, sectorSize);
	bufferLength = ROUNDUP(bufferLength, sectorSize);
	buffer = (SerialMessage*) new UCHAR[bufferLength];
	windowLength = ROUNDUP(windowLength, sectorSize);
	window = (UCHAR*) new UCHAR[windowLength];

	if (window == NULL)
		throw SQLError(IO_ERROR, "VirtualAlloc failed for \"%s\"", (const char*) fileName);

#endif

	msgLengths = new int [fileLength / sectorSize];
	memset(msgLengths, 0, sizeof(int) * fileLength / sectorSize);

	if (sync)
		syncFile();
	else
		{
		buffer->msgLength = 0;
		buffer->msgNumber = 0;
		buffer->readPosition = 0;
		write(0, sectorSize, buffer);
		}
}

void SerialPipe::close()
{
#ifdef _WIN32
	if (handle)
		{
		CloseHandle(handle);
		handle = 0;
		}
#else
	if (fd > 0)
		{
		::close(fd);
		fd = 0;
		}
#endif
}

void SerialPipe::write(uint32 position, uint32 length, SerialHeader *data)
{
	msgLengths[position / sectorSize] = length;

#ifdef _WIN32

	DWORD ret = SetFilePointer(handle, position, NULL, FILE_BEGIN);

	if (ret != position)
		throw SQLError(IO_ERROR, "serial pipe SetFilePointer failed with %d", GetLastError());


	if (!WriteFile(handle, data, length, &ret, NULL))
		throw SQLError(IO_ERROR, "serial pipe WriteFile failed with %d", GetLastError());
#else

	//off_t loc = 
	lseek(fd, position, SEEK_SET);
	uint32 n = ::write(fd, data, length);

	if (n != length)
		throw SQLEXCEPTION (IO_ERROR, "serial write error on \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);

#endif
}

void SerialPipe::syncFile()
{
	uint32 prior1 = 0;
	uint32 prior2 = 0;
	SerialMessage *message = buffer;

	for (;;)
		{
		try
			{
			read(writePosition, sectorSize, message);
			}
		catch (SQLException&)
			{
			break;
			}

		if (writePosition != 0 && message->msgNumber != (uint32)(msgNumber + 1))
			break;

		if (message->msgLength <= sectorSize && message->msgCrc != computeCrc(message))
			break;

		msgNumber = message->msgNumber;

		if (message->msgLength == sizeof(SerialHeader))
			break;

		readPosition = message->readPosition;
		msgLengths[writePosition / sectorSize] = message->msgLength;
		prior2 = prior1;
		prior1 = writePosition;
		writePosition += ROUNDUP(message->msgLength, sectorSize);
		}

	if (!validate(prior1))
		{
		if (validate(prior2))
			{
			readPosition = message->readPosition;
			writePosition = prior1;
			}
		else
			throw SQLError(IO_ERROR, "can't sync serial pipe");
		}

	printf("syncFile readPosition %d, writePosition %d\n", readPosition, writePosition);
	fillWindow();
}

void SerialPipe::read(uint32 position, uint32 length, SerialHeader *data)
{
#ifdef _WIN32

	DWORD ret = SetFilePointer(handle, position, NULL, FILE_BEGIN);

	if (ret != position)
		throw SQLError(IO_ERROR, "serial pipe SetFilePointer failed with %d", GetLastError());


	if (!ReadFile(handle, data, length, &ret, NULL))
		throw SQLError(IO_ERROR, "serial pipe ReadFile failed with %d", GetLastError());
#else

	//off_t loc = 
	lseek(fd, position, SEEK_SET);
	uint32 n = ::read(fd, data, length);

	if (n != length)
		throw SQLEXCEPTION (IO_ERROR, "serial read error on \"%s\": %s (%d)", 
							(const char*) fileName, strerror (errno), errno);
#endif
}

int SerialPipe::getMsg(int bufferLength, void *data)
{
	Sync sync(&syncObject, "SerialPipe::getMsg");
	sync.lock(Exclusive);

	// If there's nothing to do, just return

	if (readPosition == writePosition)
		return 0;

	// Round up the record length to nearest sector size

	int length = msgLengths[readPosition / sectorSize];
	length = ROUNDUP(length, sectorSize);

	// If this is a dumy message, go back to beginning; if message doesn't fit in window,
	// refill window

	if (length == sizeof(SerialHeader))
		{
		readPosition = 0;
		if (windowOffset)
			fillWindow();
		}
	else if (readPosition - windowOffset + length > windowLength)
		fillWindow();

	SerialMessage *record = (SerialMessage*) (window + readPosition - windowOffset);
	//read(readPosition, length, message);
	readPosition += length;

	if (record->msgCrc != computeCrc(record))
		throw SQLError(IO_ERROR, "invalid CRC in serial pipe");

	length = record->msgLength - sizeof(SerialHeader);
	memcpy(data, record->data, MIN(length, bufferLength));

	return length;
}

int SerialPipe::getMsgLength()
{
	return msgLengths[readPosition / sectorSize] - sizeof(SerialHeader);
}

bool SerialPipe::readWait()
{
	for (;;)
		{
		int eventCount = readers.getNext();

		if (readPosition != writePosition)
			return true;

		if (!readers.wait(eventCount))
			return false;
		}

}

// Borrowed, lock, stock and barrel, from zlib.

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

uint32 SerialPipe::crc32(uint32 crc, UCHAR *buf, int len)
{
	crc = crc ^ 0xffffffffL;

	while (len >= 8)
		{
		DO8(buf);
		len -= 8;
		}

	if (len) 
		do { DO1(buf); } while (--len);

	return crc ^ 0xffffffffL;
}

uint32 SerialPipe::computeCrc(SerialHeader *message)
{
	uint32 oldCrc = message->msgCrc;
	message->msgCrc = 0;
	uint32 crc = crc32(0, (UCHAR*) message, message->msgLength);
	message->msgCrc = oldCrc;

	return crc;
}

bool SerialPipe::validate(uint32 position)
{
	SerialMessage *message = buffer;
	uint32 length = msgLengths[position / sectorSize];
	length = ROUNDUP(length, sectorSize);
	read(position, length, message);

	return message->msgCrc == computeCrc(message);
}

void SerialPipe::resetPositions()
{
	readPosition = 0;
	writePosition = 0;
	windowOffset = 0;
}

void SerialPipe::fillWindow()
{
	if (readPosition != writePosition)
		{
		windowOffset = readPosition;
		int length = (readPosition < writePosition) ? writePosition - readPosition : fileLength - readPosition;
		read(readPosition, length, (SerialMessage*) window);
		}
}
