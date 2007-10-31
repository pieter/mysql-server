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

// IO.h: interface for the IO class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IO_H__6A019C19_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_IO_H__6A019C19_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "JString.h"
#include "SyncObject.h"

#ifndef PATH_MAX
#define PATH_MAX		256
#endif

static const int WRITE_TYPE_FORCE		= 0;
static const int WRITE_TYPE_PRECEDENCE	= 1;
static const int WRITE_TYPE_REUSE		= 2;
static const int WRITE_TYPE_SHUTDOWN	= 3;
static const int WRITE_TYPE_PAGE_WRITER	= 4;
static const int WRITE_TYPE_CLONE		= 5;
static const int WRITE_TYPE_FLUSH		= 6;
static const int WRITE_TYPE_MAX			= 7;

class Bdb;
class Hdr;
class Dbb;

class IO  
{
public:
	IO();
	~IO();

	bool	trialRead (Bdb *bdb);
	void	deleteFile();
	void	writeHeader (Hdr *header);
	int		read(int length, UCHAR *buffer);
	void	write(uint32 length, const UCHAR *data);
	bool	doesFileExist(const char *fileName);
	int		fileStat(const char *fileName, struct stat *stats = NULL, int *errnum = NULL);
	void	declareFatalError();
	void	seek (int pageNumber);
	void	closeFile();
	void	readHeader (Hdr *header);
	void	writePage (Bdb *buffer, int type);
	void	writePages(int32 pageNumber, int length, const UCHAR* data, int type);
	void	readPage (Bdb *page);
	bool	createFile (const char *name, uint64 initialAllocation);
	bool	openFile (const char *name, bool readOnly);
	void	longSeek(int64 offset);
	void	read(int64 offset, int length, UCHAR* buffer);
	void	write(int64 offset, int length, const UCHAR* buffer);
	int		pread(int64 offset, int length, UCHAR* buffer);
	int		pwrite(int64 offset, int length, const UCHAR* buffer);
	void	sync(void);
	void	reportWrites(void);

	void			tracePage(Bdb* bdb);
	void			traceOperation(int operation);
	static void		trace(int fd, int pageNumber, int pageType, int pageId);
	static void		traceOpen(void);
	static void		traceClose(void);
	
	static void		createPath (const char *fileName);
	static const char *baseName(const char *path);
	static void		expandFileName(const char *fileName, int length, char *buffer, const char **baseFileName = NULL);
	static void		deleteFile(const char* fileName);
	static int		getWriteMode(void);

	JString		fileName;
	SyncObject	syncObject;
	int			fileId;
	int			pageSize;
	uint		reads;
	uint		writes;
	uint		flushWrites;
	uint		writesSinceSync;
	uint		fetches;
	uint		fakes;
	uint		priorReads;
	uint		priorWrites;
	uint		priorFlushWrites;
	uint		priorFetches;
	uint		priorFakes;
	uint		writeTypes[WRITE_TYPE_MAX];
	bool		fatalError;

//private:
	Dbb			*dbb;						// this is a crock and should be phased out
};

#endif // !defined(AFX_IO_H__6A019C19_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
