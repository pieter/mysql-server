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

// SerialPipe.h: interface for the SerialPipe class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SerialPipe_H__4A1C1F1A_B435_4497_91BF_39F3E04536AE__INCLUDED_)
#define AFX_SerialPipe_H__4A1C1F1A_B435_4497_91BF_39F3E04536AE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#undef _WIN32

#include "SyncObject.h"
#include "Event.h"
#include "JString.h"
#include "Engine.h"	// Added by ClassView

struct SerialHeader
	{
	uint32	msgLength;
	uint32	msgNumber;
	uint32	readPosition;
	uint32	msgCrc;
	};

struct SerialMessage : public SerialHeader
	{
	UCHAR	data[1];
	};

class Thread;

class SerialPipe  
{
public:
	void fillWindow();
	void resetPositions();
	bool validate(uint32 position);
	static uint32 computeCrc (SerialHeader *message);
	static uint32 crc32(uint32 crc, UCHAR *buf, int len);
	bool readWait();
	int getMsgLength();
	int getMsg(int bufferLength, void *buffer);
	void read(uint32 position, uint32 length, SerialHeader *data);
	void syncFile();
	void write(uint32 position, uint32 length, SerialHeader *data);
	void close();
	void open();
	void putMsg(bool waitFlag, int numberBuffers, ...);
	SerialPipe(const char *filename, int fileLength,  int windowSize, int bufferLength);
	virtual ~SerialPipe();

	JString		fileName;
	uint32		fileLength;
	uint32		readPosition;
	uint32		writePosition;
	uint32		msgNumber;
	uint32		bufferLength;
	uint32		sectorSize;
	int			*msgLengths;

	uint32		windowLength;
	uint32		windowOffset;
	UCHAR		*window;

	SerialMessage	*buffer;
	SyncObject		syncObject;
	Event		readers;
	Event		writers;

#ifdef _WIN32
	HANDLE			handle;
#else
	int				fd;
#endif
};

#endif // !defined(AFX_SerialPipe_H__4A1C1F1A_B435_4497_91BF_39F3E04536AE__INCLUDED_)
