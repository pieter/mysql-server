/* Copyright (C) 2007 MySQL AB

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

// TableSpace.h: interface for the TableSpace class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TABLESPACE_H__FAD68264_27D0_4E8B_B19C_911F9DC25A89__INCLUDED_)
#define AFX_TABLESPACE_H__FAD68264_27D0_4E8B_B19C_911F9DC25A89__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

static const int TABLESPACE_TYPE_TABLESPACE		= 0;
static const int TABLESPACE_TYPE_REPOSITORY		= 1;

class Dbb;
class Database;
class InfoTable;

class TableSpace  
{
public:
	TableSpace(Database *database, const char *spaceName, int spaceId, const char *spaceFilename, uint64 initialAllocation, int tsType);
	virtual ~TableSpace();

	void	create();
	void	open();
	void	close(void);
	void	shutdown(TransId transId);
	void	dropTableSpace(void);
	bool	fileNameEqual(const char* file);
	void	sync(void);
	void	save(void);
	void	getIOInfo(InfoTable* infoTable);

	JString		name;
	JString		filename;
	TableSpace	*nameCollision;
	TableSpace	*idCollision;
	TableSpace	*next;
	Dbb			*dbb;
	Database	*database;
	uint64		initialAllocation;
	int			tableSpaceId;
	int			type;
	bool		active;
	bool		needSave;
};

#endif // !defined(AFX_TABLESPACE_H__FAD68264_27D0_4E8B_B19C_911F9DC25A89__INCLUDED_)
