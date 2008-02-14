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

// SerialLogFile.h: interface for the SerialLogFile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGFILE_H__053376A1_25B5_48EC_B6A2_92D5120258DE__INCLUDED_)
#define AFX_SERIALLOGFILE_H__053376A1_25B5_48EC_B6A2_92D5120258DE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Mutex.h"

class Database;
struct SerialLogBlock;

class SerialLogFile  
{
public:
	void dropDatabase();
	void zap();
	uint32 read(int64 position, uint32 length, UCHAR *data);
	void write(int64 position, uint32 length, const SerialLogBlock *data);
	void close();
	void open (JString filename, bool creat);
	SerialLogFile(Database *db);
	virtual ~SerialLogFile();

	int64		offset;
	int64		highWater;
	int64		writePoint;
	JString		fileName;
	uint32		sectorSize;
	Mutex		syncObject;
	Database	*database;
	bool		forceFsync;

#ifdef _WIN32
	void	*handle;
#else
	int		handle;
#endif
};

#endif // !defined(AFX_SERIALLOGFILE_H__053376A1_25B5_48EC_B6A2_92D5120258DE__INCLUDED_)
