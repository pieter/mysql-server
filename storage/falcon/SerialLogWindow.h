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

// SerialLogWindow.h: interface for the SerialLogWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGWINDOW_H__F95E772E_C70C_4968_8F84_A82959AC0237__INCLUDED_)
#define AFX_SERIALLOGWINDOW_H__F95E772E_C70C_4968_8F84_A82959AC0237__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

class SerialLogFile;
class SerialLog;
struct SerialLogBlock;

class SerialLogWindow 
{
public:
	SerialLogWindow(SerialLog *serialLog, SerialLogFile *logFile, int64 logOrigin);
	virtual ~SerialLogWindow();

	void			deactivateWindow();
	void			activateWindow (bool read);
	void			setBuffer (UCHAR *newBuffer);
	int64			getNextFileOffset();
	int64			getNextVirtualOffset(void);
	SerialLogBlock* nextAvailableBlock (SerialLogBlock *block);
	SerialLogBlock* nextBlock(SerialLogBlock *block);
	SerialLogBlock* findBlock(uint64 blockNumber);
	SerialLogBlock* findLastBlock(SerialLogBlock *block);
	SerialLogBlock* readFirstBlock();
	void			setPosition(SerialLogFile *logFile, int64 logOrigin);
	void			addRef(void);
	void			release(void);
	void			setLastBlock(SerialLogBlock* block);
	uint64			getVirtualOffset();
	void			write (SerialLogBlock *block);
	void			print(void);
	bool			 validate(SerialLogBlock* block);

	inline SerialLogBlock* firstBlock()
		{ return (SerialLogBlock*) buffer; }

	SerialLogWindow	*next;
	SerialLogWindow	*prior;
	SerialLogFile	*file;
	SerialLogBlock	*lastBlock;
	SerialLog		*log;
	UCHAR			*buffer;
	UCHAR			*bufferEnd;
	UCHAR			*warningTrack;
	int64			origin;
	uint32			currentLength;
	uint32			bufferLength;
	uint32			sectorSize;
	uint64			firstBlockNumber;
	uint64			lastBlockNumber;
	int				lastBlockOffset;
	int				inUse;
	int				useCount;
	uint64			virtualOffset;
	bool validate(const UCHAR* pointer);
};

#endif // !defined(AFX_SERIALLOGWINDOW_H__F95E772E_C70C_4968_8F84_A82959AC0237__INCLUDED_)
