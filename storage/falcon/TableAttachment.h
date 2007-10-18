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

// TableAttachment.h: interface for the TableAttachment class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TABLEATTACHMENT_H__919DFC51_F120_11D2_AB6D_0000C01D2301__INCLUDED_)
#define AFX_TABLEATTACHMENT_H__919DFC51_F120_11D2_AB6D_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define PRE_INSERT		1
#define POST_INSERT		2
#define PRE_UPDATE		4
#define POST_UPDATE		8
#define PRE_DELETE		0x10
#define POST_DELETE		0x20
#define POST_COMMIT		0x40

class Record;
class RecordVersion;
class Table;
	
class TableAttachment  
{
public:
	virtual void preDelete (Table * table, RecordVersion * record);
	virtual void tableDeleted (Table *table);
	virtual void preUpdate (Table * table, RecordVersion * record);
	virtual void preInsert (Table *table, RecordVersion *record);
	virtual void deleteCommit (Table *table, Record *record);
	virtual void updateCommit (Table *table, RecordVersion *record);
	virtual void insertCommit (Table *table, RecordVersion *record);
	virtual void postCommit (Table *table, RecordVersion *record);
	TableAttachment(int32 flags);
	virtual ~TableAttachment();

	int32	mask;
};

#endif // !defined(AFX_TABLEATTACHMENT_H__919DFC51_F120_11D2_AB6D_0000C01D2301__INCLUDED_)
