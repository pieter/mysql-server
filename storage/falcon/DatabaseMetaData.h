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

// DatabaseMetaData.h: interface for the DatabaseMetaData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATABASEMETADATA_H__C3220BA1_0199_11D3_AB6F_0000C01D2301__INCLUDED_)
#define AFX_DATABASEMETADATA_H__C3220BA1_0199_11D3_AB6F_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define NETFRASERVER		"NetfraServer"

class ResultSet;
class Connection;

class DatabaseMetaData  
{
public:
	ResultSet* getSchemaParameters();
	ResultSet* getRepositories (const char *catalog, const char *schema, const char *namePattern);
	ResultSet* getDomains(const char * catalog, const char * schema,  const char * fieldName);
	virtual ResultSet* getExportedKeys(const char * catalog, const char * schema, const char * tableName);
	virtual ResultSet* getObjectPrivileges (const char *catalog, const char *schemaPattern, const char *namePattern, int objectType);
	virtual ResultSet* getUserRoles (const char * catalog, const char * schema, const char *rolePattern, const char *user);
	virtual ResultSet* getRoles (const char * catalog, const char * schema, const char *rolePattern);
	virtual ResultSet* getUsers (const char * catalog, const char *userPattern);
	virtual ResultSet* getImportedKeys (const char * catalog, const char * schema, const char * tableName);
	virtual void release();
	virtual void addRef();
	virtual ResultSet* getTables (const char *catalog, const char *schemaPattern, const char *tableNamePattern, int typeCount, const char **types);
	virtual ResultSet* getTables (const char *catalog, const char *schemaPattern, const char *tableNamePattern, const char **types);
	virtual ~DatabaseMetaData();
	virtual ResultSet* getIndexInfo (const char *catalog, const char *schema, const char *tableName, bool b1, bool v2);
	virtual ResultSet* getPrimaryKeys (const char *catalog, const char *schema, const char *table);
	virtual ResultSet* getColumns (const char *catalog, const char *schema, const char *table, const char *fieldName);
	virtual ResultSet* getTriggers(const char *catalog, const char *schema, const char *table, const char *triggerPattern);
	virtual ResultSet* getHolderPrivileges(const char * catalog, const char * schemaPattern, const char * namePattern, int objectType);
	virtual ResultSet* getSequences (const char * catalog, const char * schema, const char *sequence);
	virtual const char* getDatabaseProductName();
	virtual const char* getDatabaseProductVersion();

	DatabaseMetaData(Connection *connection);

	Connection	*connection;
	JString		databaseVersion;
	int			useCount;
	int32		handle;
};

#endif // !defined(AFX_DATABASEMETADATA_H__C3220BA1_0199_11D3_AB6F_0000C01D2301__INCLUDED_)
