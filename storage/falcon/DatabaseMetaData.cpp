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

// DatabaseMetaData.cpp: implementation of the DatabaseMetaData class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "DatabaseMetaData.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SequenceResultSet.h"
#include "Connection.h"
#include "Index.h"
#include "NetfraVersion.h"
#include "Database.h"

//#define FUDGE_CASE(string)		(string)
#define FUDGE_CASE(string)		JString::upcase(string)

static const char versionString[] = VERSION_STRING;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DatabaseMetaData::DatabaseMetaData(Connection *cnct)
{
	connection = cnct;
	useCount = 0;
	handle = connection->getNextHandle();
}

DatabaseMetaData::~DatabaseMetaData()
{

}

ResultSet* DatabaseMetaData::getTables(const char * catalog, const char * schemaPattern, const char * tableNamePattern, const char * * types)
{
	int count = 0;

	if (types)
		for (const char **ptr = types; *ptr; ++ptr)
			++count;

	return getTables (catalog, schemaPattern, tableNamePattern, count, types);
}

ResultSet* DatabaseMetaData::getTables(const char * catalog, const char * schemaPattern, const char * tableNamePattern, int typeCount, const char * * types)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as table_cat,\n"				// 1
				"schema as table_schem,\n"		// 2
				"tableName as table_name,\n"	// 3
				"type,\n"						// 4
				"remarks,\n"					// 5
				"viewDefinition as view_def\n"	// 6 but unofficial
		"from system.tables where "
		"schema like ? and tableName like ? and"
		"(type=? or type=? or type=?)"
		"order by type,schema,tableName");
	int n = 1;
	statement->setString (n++, FUDGE_CASE (schemaPattern));
	statement->setString (n++, FUDGE_CASE (tableNamePattern));

	if (types)
		for (int type = 0; type < typeCount; ++type)
			statement->setString (n++, FUDGE_CASE (types [type]));
	else
		{
		statement->setString (n++, "TABLE");
		statement->setString (n++, "SYSTEM TABLE");
		statement->setString (n++, "VIEW");
		}

	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getIndexInfo(const char * catalog, const char * schema, const char * tableName, bool b1, bool v2)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as table_cat,\n"						// 1
				"\tschema as table_schem,\n"			// 2
				"\ttableName as table_name,"			// 3
				"\tcase indexType when 2 then 1 else 0 end as non_unique,\n"	// 4
				"\t'' as index_qualifier,\n"			// 5
				"\tindexName as index_name,\n"			// 6
				"\tindexType as type,\n"				// 7
				"\tposition as ordinal_position,\n"		// 8
				"\tfield as column_name,\n"				// 9
				"\t'A' as asc_or_desc,\n"				// 10
				"\t0 as cadinality,\n"					// 11
				"\t0 as pages,\n"						// 12
				"\t'' as filter_condition\n"			// 13
		"from system.indexes, system.indexFields\n"
		" and where indexes.schema = ? and indexes.tableName = ?\n"
		" and indexFields.indexName = indexes.indexName\n"
		" and indexFields.tableName = indexes.tableName\n"
		" and indexFields.schema = indexes.schema\n"
		" and (indexType = ? or indexType = ?)\n"
		"order by indexType, indexName, position");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (tableName));
	statement->setInt (3, UniqueIndex);
	statement->setInt (4, SecondaryIndex);
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getPrimaryKeys(const char * catalog, const char * schema, const char * tableName)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as table_cat,\n"
				"\tschema as table_schem,\n"
				"\ttableName as table_name,"
				"\tfield as column_name,\n"
				"\tposition as ordinal_position,\n"
				"\tindexName as index_name\n"
		"from system.indexes, system.indexFields\n"
		" and where indexes.schema = ? and indexes.tableName = ?\n"
		" and indexFields.indexName = indexes.indexName\n"
		" and indexFields.tableName = indexes.tableName\n"
		" and indexFields.schema = indexes.schema\n"
		" and indexType = ?\n"
		"order by position");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (tableName));
	statement->setInt (3, PrimaryKey);
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

void DatabaseMetaData::addRef()
{
	++useCount;
	connection->addRef();
}

void DatabaseMetaData::release()
{
	--useCount;

	if (connection)
		connection->release();
	else if (useCount == 0)
		delete this;
}

ResultSet* DatabaseMetaData::getImportedKeys(const char * catalog, const char * schema, const char * tableName)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select \n"
				"\t'' as pktable_cat,\n"
				"\tp.schema as pktable_schem,\n"
				"\tp.tableName as pktable_name,"
				"\tpf.field as pkcolumn_name,\n"
				"\t'' as fktable_cat,\n"
				"\tf.schema as fktable_schem,\n"
				"\tf.tableName as fktable_name,"
				"\tff.field as fkcolumn_name,\n"
				"\tk.position as key_seq,\n"
				"\tupdaterule as update_rule,\n"
				"\tdeleterule as delete_rule,\n"
				"\t'' as pkname,\n"
				"\t'' as fkname,\n"
				"\tdeferrability\n"
		"from system.tables f,\n"
			 "\tsystem.foreignKeys k,\n"
			 "\tsystem.tables p,\n"
			 "\tsystem.fields ff,\n"
			 "\tsystem.fields pf\n"
		"where f.schema=? and f.tableName=?\n"
		"\tand foreignTableId = f.tableId\n"
		"\tand p.tableId = primaryTableId\n"
		"\tand ff.fieldId = foreignFieldId\n"
		"\tand ff.tableName = f.tableName\n"
		"\tand ff.schema = f.schema\n"
		"\tand pf.fieldId = primaryFieldId\n"
		"\tand pf.tableName = p.tableName\n"
		"\tand pf.schema = p.schema\n"
		"order by p.schema, p.tableName, k.position");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (tableName));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getColumns(const char * catalog, const char * schema, const char * tableName, const char * fieldName)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as table_cat,\n"				// 1
				"\tschema as table_schem,\n"	// 2
				"\ttableName as table_name,"	// 3
				"\tfield as column_name,\n"		// 4
				"\tcase dataType\n"				// 5
				"\t   when  1 then 1\n"			// actually, String, which shouldn't occur
				"\t   when  2 then 1\n"
				"\t   when  3 then 12\n"
				"\t   when  4 then 5\n"
				"\t   when  5 then 4\n"
				"\t   when  6 then -5\n"
				"\t   when  7 then 6\n"
				"\t   when  8 then 8\n"
				"\t   when  9 then 91\n"
				"\t   when 10 then 93\n"
				"\t   when 11 then 92\n"
				"\t   when 12 then 2005\n"
				"\t   when 13 then 2004\n"
				"\t  end as data_type,\n"
				/***
				"\tcase dataType\n"				// 6
				"\t   when  1 then 'CHAR'\n"
				"\t   when  2 then 'CHAR'\n"
				"\t   when  3 then 'VARCHAR'\n"
				"\t   when  4 then 'SMALLINT'\n"
				"\t   when  5 then 'INTEGER'\n"
				"\t   when  6 then 'BIGINT'\n"
				"\t   when  7 then 'FLOAT'\n"
				"\t   when  8 then 'DOUBLE'\n"
				"\t   when  9 then 'DATE'\n"
				"\t   when 10 then 'TIMESTAMP'\n"
				"\t   when 11 then 'TIME'\n"
				"\t   when 12 then 'CLOB'\n"
				"\t   when 13 then 'BLOB'\n"
				"\t  end as type_name,\n"
				***/
				"\tcase\n"						// 6
					"when domainname is not null and domainname <> '' then schema || '.' || domainname\n"
					"\telse case dataType\n"				// 6
					"\t   when  1 then 'CHAR'\n"
					"\t   when  2 then 'CHAR'\n"
					"\t   when  3 then 'VARCHAR'\n"
					"\t   when  4 then 'SMALLINT'\n"
					"\t   when  5 then 'INTEGER'\n"
					"\t   when  6 then 'BIGINT'\n"
					"\t   when  7 then 'FLOAT'\n"
					"\t   when  8 then 'DOUBLE'\n"
					"\t   when  9 then 'DATE'\n"
					"\t   when 10 then 'TIMESTAMP'\n"
					"\t   when 11 then 'TIME'\n"
					"\t   when 12 then 'CLOB'\n"
					"\t   when 13 then 'BLOB'\n"
					"\t	  end\n"
				"\t  end as type_name,\n"
				"\tprecision as column_size,\n" // 7
				/***
				"\tcase dataType\n"				// 7
				"\t   when  1 then length\n"
				"\t   when  2 then length\n"
				"\t   when  3 then length\n"
				"\t   when  4 then 4\n"
				"\t   when  5 then 9\n"
				"\t   when  6 then 19\n"
				"\t   when  7 then 15\n"
				"\t   when  8 then 15\n"
				"\t   when  9 then 10\n"
				"\t   when 10 then 16\n"
				"\t   when 11 then 16\n"
				"\t   when 12 then 100\n"
				"\t   when 13 then 100\n"
				"\t  end as column_size,\n"
				***/
				"\tnull as buffer_length,\n"	// 8
				"\tscale as decimal_digits,\n"	// 9
				"\t10 as num_prec_radix,\n"		// 10
				"\t1 as nullable,\n"			// 11
				"\tremarks,\n"					// 12
				"\tnull as column_def,\n"		// 13
				"\tnull as SQL_DATA_TYPE,\n"	// 14
				"\tnull as SQL_DATETIME_SUB,\n"	// 15
				"\tlength as CHAR_OCTET_LENGTH,\n"// 16
				"\tfieldId as ordinal_position,\n" // 17
				"\t'YES' as IS_NULLABLE,\n"		// 18
				"\tflags,\n"					// 19
				"\tcollationsequence as collation_sequence,\n"			// 20
				"\trepositoryname as repository_name\n"					// 21
			  //"\tdefaultValue as default_value\n"						// ??
		"from system.fields\n"
		" where schema like ?\n"
		" and tableName like ?\n"
		" and field like ?\n"
		"order by fieldId");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (tableName));
	statement->setString (3, FUDGE_CASE (fieldName));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getUsers(const char * catalog, const char *userPattern)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select userName, coterie from system.users "
			"where userName like ? "
			"and userName <> 'SYSTEM' order by userName");
	statement->setString (1, FUDGE_CASE (userPattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getRoles(const char * catalog, const char * schema, const char *rolePattern)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
			"schema as role_schema,"
			"roleName as role_name "
			"from system.roles "
			"where schema like ? "
			"and roleName like ? "
			"order by schema, roleName");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (rolePattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getUserRoles(const char * catalog, const char * schema, const char *rolePattern, const char * user)
{
	JString sql = "select "
			"userName as user_name, "
			"roleSchema as role_schema,"
			"roleName as role_name,"
			"defaultRole as default_role,"
			"options "
			"from system.userroles";
	const char *sep = " where ";

	if (schema)
		{
		sql += sep;
		sql += "roleSchema = ?";
		sep = " and ";
		}

	if (rolePattern)
		{
		sql += sep;
		sql += "roleName like ?";
		sep = " and ";
		}

	if (user)
		{
		sql += sep;
		sql += "userName like ?";
		sep = " and ";
		}

	sql += "order by userName, roleSchema, roleName";

	PreparedStatement *statement = connection->prepareStatement (sql);
	int n = 1;

	if (schema)
		statement->setString (n++, FUDGE_CASE (schema));

	if (rolePattern)
		statement->setString (n++, FUDGE_CASE (rolePattern));

	if (user)
		statement->setString (n++, FUDGE_CASE (user));

	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getObjectPrivileges(const char * catalog, const char * schemaPattern, const char * namePattern, int objectType)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
				"objectType as object_type,\n"
				"objectSchema as object_schema,\n"
				"objectName as object_name,\n"
				"holderType as holder_type,\n"
				"holderSchema as holder_schema,\n"
				"holderName as holder_name,\n"
				"privilegeMask as privilege_mask\n"
			"from system.privileges where "
				"objectType = ? and "
				"objectSchema like ? and "
				"objectName like ? "
			"order by objectType,objectSchema,objectName,holderType,HolderSchema,holderName");
	statement->setInt (1, objectType);
	statement->setString (2, FUDGE_CASE (schemaPattern));
	statement->setString (3, FUDGE_CASE (namePattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getSequences(const char *catalog, const char *schema, const char *sequence)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
			"'' as sequence_cat,"
			"schema as sequence_schema,"
			"sequenceName as sequence_name,"
			"id as sequence_value "
			"from system.sequences "
			"where schema like ? "
			"and sequenceName like ? "
			"order by schema, sequenceName");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (sequence));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return new SequenceResultSet (resultSet);
}

ResultSet* DatabaseMetaData::getTriggers(const char *catalog, const char *schema, const char *table, const char *triggerPattern)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as table_cat,\n"					// 1
				"schema as table_schem,\n"			// 2
				"tableName as table_name,"			// 3
				"triggerName as trigger_name,\n"	// 4
				"className as class_name,\n"		// 5
				"methodName as method_name,\n"		// 6
				"position,\n"						// 7
				"type_mask,\n"						// 8
				"active\n"							// 9
		"from system.triggers\n"
		" where schema = ?\n"
		" and tableName like ?\n"
		" and triggerName like ?\n"
		"order by schema,tableName,triggerName");

	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (table));
	statement->setString (3, FUDGE_CASE (triggerPattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getHolderPrivileges(const char *catalog, const char *schemaPattern, const char *namePattern, int objectType)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
				"objectType as object_type,\n"
				"objectSchema as object_schema,\n"
				"objectName as object_name,\n"
				"holderType as holder_type,\n"
				"holderSchema as holder_schema,\n"
				"holderName as holder_name,\n"
				"privilegeMask as privilege_mask\n"
			"from system.privileges where "
				"holderType = ? and "
				"holderSchema like ? and "
				"holderName like ? "
			"order by objectType,objectSchema,objectName,holderType,HolderSchema,holderName");
	statement->setInt (1, objectType);
	statement->setString (2, FUDGE_CASE (schemaPattern));
	statement->setString (3, FUDGE_CASE (namePattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getExportedKeys(const char *catalog, const char *schema, const char *tableName)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select \n"
				"\t'' as pktable_cat,\n"
				"\tp.schema as pktable_schem,\n"
				"\tp.tableName as pktable_name,"
				"\tpf.field as pkcolumn_name,\n"
				"\t'' as fktable_cat,\n"
				"\tf.schema as fktable_schem,\n"
				"\tf.tableName as fktable_name,"
				"\tff.field as fkcolumn_name,\n"
				"\tk.position as key_seq,\n"
				"\tupdaterule as update_rule,\n"
				"\tdeleterule as delete_rule,\n"
				"\t'' as pkname,\n"
				"\t'' as fkname,\n"
				"\tdeferrability\n"
		"from system.tables f,\n"
			 "\tsystem.foreignKeys k,\n"
			 "\tsystem.tables p,\n"
			 "\tsystem.fields ff,\n"
			 "\tsystem.fields pf\n"
		"where p.schema=? and p.tableName=?\n"
		"\tand foreignTableId = f.tableId\n"
		"\tand p.tableId = primaryTableId\n"
		"\tand ff.fieldId = foreignFieldId\n"
		"\tand ff.tableName = f.tableName\n"
		"\tand ff.schema = f.schema\n"
		"\tand pf.fieldId = primaryFieldId\n"
		"\tand pf.tableName = p.tableName\n"
		"\tand pf.schema = p.schema\n"
		"order by f.schema, f.tableName, k.position");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (tableName));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

const char* DatabaseMetaData::getDatabaseProductName()
{
	return NETFRASERVER;
}

const char* DatabaseMetaData::getDatabaseProductVersion()
{
	if (databaseVersion.IsEmpty())
		databaseVersion.Format ("%s ODS %d", VERSION_STRING, connection->database->odsVersion);

	return databaseVersion;
}

ResultSet* DatabaseMetaData::getDomains(const char *catalog, const char *schema, const char *fieldName)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select '' as domain_cat,\n"			// 1
				"\tschema as domain_schem,\n"	// 2
				"\tfield as domain_name,\n"		// 4
				"\tcase dataType\n"				// 5
				"\t   when  1 then 1\n"			// actually, String, which shouldn't occur
				"\t   when  2 then 1\n"
				"\t   when  3 then 12\n"
				"\t   when  4 then 5\n"
				"\t   when  5 then 4\n"
				"\t   when  6 then -5\n"
				"\t   when  7 then 6\n"
				"\t   when  8 then 8\n"
				"\t   when  9 then 91\n"
				"\t   when 10 then 93\n"
				"\t   when 11 then 92\n"
				"\t   when 12 then 2005\n"
				"\t   when 13 then 2004\n"
				"\t  end as data_type,\n"
				/***
				"\tcase dataType\n"				// 6
				"\t   when  1 then 'CHAR'\n"
				"\t   when  2 then 'CHAR'\n"
				"\t   when  3 then 'VARCHAR'\n"
				"\t   when  4 then 'SMALLINT'\n"
				"\t   when  5 then 'INTEGER'\n"
				"\t   when  6 then 'BIGINT'\n"
				"\t   when  7 then 'FLOAT'\n"
				"\t   when  8 then 'DOUBLE'\n"
				"\t   when  9 then 'DATE'\n"
				"\t   when 10 then 'TIMESTAMP'\n"
				"\t   when 11 then 'TIME'\n"
				"\t   when 12 then 'CLOB'\n"
				"\t   when 13 then 'BLOB'\n"
				"\t  end as type_name,\n"
				***/
				"\tcase dataType\n"				// 6
				"\t   when  1 then 'CHAR'\n"
				"\t   when  2 then 'CHAR'\n"
				"\t   when  3 then 'VARCHAR'\n"
				"\t   when  4 then 'SMALLINT'\n"
				"\t   when  5 then 'INTEGER'\n"
				"\t   when  6 then 'BIGINT'\n"
				"\t   when  7 then 'FLOAT'\n"
				"\t   when  8 then 'DOUBLE'\n"
				"\t   when  9 then 'DATE'\n"
				"\t   when 10 then 'TIMESTAMP'\n"
				"\t   when 11 then 'TIME'\n"
				"\t   when 12 then 'CLOB'\n"
				"\t   when 13 then 'BLOB'\n"
				"\t  end as type_name,\n"
				"\tcase dataType\n"				// 7
				"\t   when  1 then length\n"
				"\t   when  2 then length\n"
				"\t   when  3 then length\n"
				"\t   when  4 then 4\n"
				"\t   when  5 then 9\n"
				"\t   when  6 then 19\n"
				"\t   when  7 then 15\n"
				"\t   when  8 then 15\n"
				"\t   when  9 then 10\n"
				"\t   when 10 then 16\n"
				"\t   when 11 then 16\n"
				"\t   when 12 then 100\n"
				"\t   when 13 then 100\n"
				"\t  end as column_size,\n"
				//"\tlength as column_size,\n"	// 7
				"\tscale as decimal_digits,\n"	// 9
				"\t10 as num_prec_radix,\n"		// 10
				"\tremarks,\n"					// 12
				"\tlength as CHAR_OCTET_LENGTH \n"// 16
		"from system.fields\n"
		" where schema = ?\n"
		" and domainName like ?\n"
		"order by schema,domainName");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (fieldName));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getRepositories(const char *catalog, const char *schema, const char *namePattern)
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
			"'' as repository_cat,"
			"schema as repository_schem,"
			"repositoryName as repository_name,"
			"sequenceName as sequence_name,"
			"filename as file_pattern,"
			"currentVolume as current_volume,"
			"rollovers "
			"from system.repositories "
			"where schema like ? "
			"and repositoryName like ? "
			"order by schema, repositoryName");
	statement->setString (1, FUDGE_CASE (schema));
	statement->setString (2, FUDGE_CASE (namePattern));
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

ResultSet* DatabaseMetaData::getSchemaParameters()
{
	PreparedStatement *statement = connection->prepareStatement (
		"select\n"
			"schema as table_schem,"
			"sequence_interval,"
			"system_id "
			"from system.schemas "
			"order by schema, repositoryName");
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}
