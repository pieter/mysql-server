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

// Schema.h: interface for the Schema class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCHEMA_H__3F08D185_DFEF_4312_BBD0_0E37A0156EE0__INCLUDED_)
#define AFX_SCHEMA_H__3F08D185_DFEF_4312_BBD0_0E37A0156EE0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Database;

class Schema  
{
public:
	void refresh();
	void setSystemId (int newId);
	void setInterval (int newInterval);
	void update();
	Schema(Database *db, const char *schemaName);
	virtual ~Schema();

	const char	*name;
	Schema		*collision;
	Database	*database;
	int			systemId;
	int			sequenceInterval;
};

#endif // !defined(AFX_SCHEMA_H__3F08D185_DFEF_4312_BBD0_0E37A0156EE0__INCLUDED_)
