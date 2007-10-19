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

#ifndef _MsgType_
#define _MsgType_

enum MsgType {
	Failure,
	Success,
	Shutdown,
	OpenDatabase,
	CreateDatabase,
	CloseConnection,
	PrepareTransaction,
	CommitTransaction,
	RollbackTransaction,
	PrepareStatement,
	PrepareDrl,
	CreateStatement,
	GenConnectionHtml,

	GetResultSet,
	Search,
	CloseStatement,
	ClearParameters,
	GetParameterCount,
	Execute,
	ExecuteQuery,
	ExecuteUpdate,
	SetCursorName,

	ExecutePreparedStatement,
	ExecutePreparedQuery,
	ExecutePreparedUpdate,
	SendParameters,

	GetMetaData,
	Next,
	CloseResultSet,
	GenHtml,

	NextHit,
	FetchRecord,
	CloseResultList,

	GetDatabaseMetaData,	// Connection
	GetCatalogs,			// DatabaseMetaData
	GetSchemas,				// DatabaseMetaData
	GetTables,				// DatabaseMetaData
	getTablePrivileges,		// DatabaseMetaData
	GetColumns,				// DatabaseMetaData
	GetColumnsPrivileges,	// DatabaseMetaData
	GetPrimaryKeys,			// DatabaseMetaData
	GetImportedKeys,		// DatabaseMetaData
	GetExportedKeys,		// DatabaseMetaData
	GetIndexInfo,			// DatabaseMetaData
	GetTableTypes,			// DatabaseMetaData
	GetTypeInfo,			// DatabaseMetaData
	GetMoreResults,			// Statement
	GetUpdateCount,			// Statement

	Ping,					// Connection
	HasRole,				// Connection
	GetObjectPrivileges,	// DatabaseMetaData
	GetUserRoles,			// DatabaseMetaData
	GetRoles,				// DatabaseMetaData
	GetUsers,				// DatabaseMetaData

	OpenDatabase2,			// variation with attribute list
	CreateDatabase2,		// variation with attribute list

	InitiateService,		// Connection
	GetTriggers,			// DatabaseMetaData
	Validate,
	GetAutoCommit,
	SetAutoCommit,
	GetReadOnly,
	IsReadOnly,
	GetTransactionIsolation,
	SetTransactionIsolation,
	GetSequenceValue,
	AddLogListener,				// Connection
	DeleteLogListener,			// Connection
	GetDatabaseProductName,		// DatabaseMetaData
	GetDatabaseProductVersion,	// DatabaseMetaData
	Analyze,					// Connection
	StatementAnalyze,			// Statement
	SetTraceFlags,				// Connection
	ServerOperation,			// Connection
	SetAttribute,				// Connection
	GetSequences,				// DatabaseMetaData
	GetHolderPrivileges,		// DatabaseMetaData
	AttachDebugger,				// Connection
	DebugRequest,				// Non-operational debug request
	GetSequenceValue2,			// Connection
	GetConnectionLimit,			// Connection
	SetConnectionLimit,			// Connection
	DeleteBlobData,				// Connection
	};

#endif

