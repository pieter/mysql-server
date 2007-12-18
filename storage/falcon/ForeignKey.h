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

// ForeignKey.h: interface for the ForeignKey class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FOREIGNKEY_H__31E372B1_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
#define AFX_FOREIGNKEY_H__31E372B1_B24B_11D2_AB5E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

static const int importedKeyCascade	= 0;
static const int importedKeyRestrict = 1;
static const int importedKeySetNull  = 2;
static const int importedKeyNoAction = 3;
static const int importedKeySetDefault  = 4;
static const int importedKeyInitiallyDeferred  = 5;
static const int importedKeyInitiallyImmediate  = 6;
static const int importedKeyNotDeferrable  = 7;

class Table;
CLASS(Field);
class Database;
class ResultSet;
class Transaction;
class Record;

class ForeignKey  
{
public:
	void cascadeDelete (Transaction * transaction, Record * oldRecord);
	void setDeleteRule (int rule);
	void bindTable (Table *table);
	void deleteForeignKey();
	void create();
	void drop();
	bool matches (ForeignKey *key, Database *database);
	bool isMember (Field *field, bool foreign);
	void bind (Database *database);
	static void loadForeignKeys (Database *database, Table *table);
	static void loadPrimaryKeys (Database *database, Table *table);
	void loadRow (Database *database, ResultSet *resultSet);
	void save (Database *database);

	ForeignKey();
	ForeignKey (ForeignKey *key);
	ForeignKey (int cnt, Table *primary, Table *foreign);
	virtual ~ForeignKey();

	int		numberFields;
	Table	*primaryTable;
	Table	*foreignTable;
	Field	**primaryFields;
	Field	**foreignFields;
	int		primaryTableId;
	int		foreignTableId;
	int		*primaryFieldIds;
	int		*foreignFieldIds;
	int		deleteRule;
	JString	deleteStatement;
	ForeignKey	*next;					// next in Table

protected:
};

#endif // !defined(AFX_FOREIGNKEY_H__31E372B1_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
