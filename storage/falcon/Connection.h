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

// Connection.h: interface for the Connection class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONNECTION_H__02AD6A46_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_CONNECTION_H__02AD6A46_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#include "Engine.h"	// Added by ClassView
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "LinkedList.h"
#include "Stack.h"
#include "Privilege.h"
#include "GenOption.h"
#include "PrivType.h"
#include "Parameters.h"
#include "SyncObject.h"
#include "Log.h"

#define SERVER_PRODUCT			"NetfraServer"
#define SERVER_PRODUCT_UNITS	2

static const int TRANSACTION_NONE			  = 0;	// Transactions are not supported. 
static const int TRANSACTION_READ_UNCOMMITTED = 1;	// Dirty reads, non-repeatable reads and phantom reads can occur.
static const int TRANSACTION_READ_COMMITTED   = 2;	// Dirty reads are prevented; non-repeatable reads and phantom reads can occur.
static const int TRANSACTION_WRITE_COMMITTED  = 4;	// Dirty reads are prevented; non-repeatable reads happen after writes; phantom reads can occur.
static const int TRANSACTION_CONSISTENT_READ  = 8;	// Dirty reads and non-repeatable reads are prevented; phantom reads can occur.   
static const int TRANSACTION_SERIALIZABLE     = 16;	// Dirty reads, non-repeatable reads and phantom reads are prevented.

#define IS_READ_UNCOMMITTED (_level) (_level == TRANSACTION_READ_UNCOMMITTED)
#define IS_READ_COMMITTED(_level) (_level == TRANSACTION_READ_COMMITTED)
#define IS_WRITE_COMMITTED(_level) (_level == TRANSACTION_WRITE_COMMITTED)
#define IS_CONSISTENT_READ(_level) (_level == TRANSACTION_CONSISTENT_READ)
#define IS_REPEATABLE_READ(_level) (_level == TRANSACTION_WRITE_COMMITTED || _level == TRANSACTION_CONSISTENT_READ)
#define IS_SERIALIZABLE(_level) (_level == TRANSACTION_SERIALIZABLE)

// TRANSACTION_WRITE_COMMITTED is a Falcon name for InnoDB's version of
//   'repeatable read' which is not a consistent read.  Changes are allowed
//   any time within a transaction to newly committed records.  And these 
//   transactions will wait upon pending transactions if those pending changes 
//   may be needed.  But reads alone are repeatable.  A 'write committed'
//   transaction does not read newer records unless those newer records were 
//   changed by itself.
// TRANSACTION_CONSISTENT_READ is truly a consistent read.  Newer changes are
//   never read or written to.  They only affect a transaction by preventing
//   duplicates.

const int analyzeMemory		= 1;
const int analyzeClasses	= 2;
const int analyzeRecords	= 4;
const int analyzeStatements = 8;
const int analyzeTables		= 16;
const int analyzeCache		= 32;
const int analyzeObjects	= 64;
const int analyzeSpace		= 128;
const int analyzeSync		= 256;
const int analyzeIO			= 512;

const int traceBooleans		= 1;
const int traceTriggers		= 2;

const int opTraceAll		= 1;
const int opShutdown		= 2;
const int opShutdownImmed	= 4;
const int opCancelRequest	= 8;
const int opClone			= 16;
const int opCreateShadow	= 32;
const int opInstallLicense	= 64;

const int validatePages		= 1;
const int validateRepair	= 2;
const int validateBlobs		= 4;
const int validateSpecial	= 8;
const int validateMinutia	= 16;

const int statementMaxSortLimit		= 1;
const int connectionMaxSortLimit	= 2;
const int statementMaxRecordsLimit	= 3;
const int connectionMaxRecordsLimit	= 4;

class Database;
CLASS(Statement);
class PreparedStatement;
class Transaction;
class TemplateContext;
class DatabaseMetaData;
class User;
class Role;
class PrivilegeObject;
class ResultSet;
class Image;
class Parameters;
class Clob;
class LicenseToken;
class Sequence;
class JavaConnection;
class FilterSet;
class Table;
class Threads;
class Configuration;
class ResultList;

enum StatementType {
	statementDeleteCascade,
	};

struct RegisteredStatement {
	StatementType		type;
	void				*owner;
	PreparedStatement	*statement;
	RegisteredStatement	*next;
	};
	
class Connection
{
protected:
	virtual ~Connection();
	void closeConnection();

public:
	Connection (Database *db);
	Connection(Configuration *config);
	Connection (Database *db, User *usr);

	virtual void		serverOperation (int op, Parameters * parameters);
	virtual void		updateSchedule (const char *application, const char *eventName, const char *schedule);
	virtual void		shutdown();
	virtual PreparedStatement* prepareStatement (const char *sqlString);
	virtual Statement*	createStatement();
	virtual void		openDatabase (const char *dbName, Parameters *parameters, Threads *parent);
	virtual void		createDatabase (const char *dbName, Parameters *parameters, Threads *parent);
	virtual void		close();
	virtual void		prepare(int xidSize, const UCHAR *xid);
	virtual void		rollback();
	virtual void		commit();
	virtual Clob*		genHTML (TemplateContext *context, int32 genHeaders);
	virtual void		genNativeMethod (const char *className, const char *fileName, GenOption option);
	virtual void		popSchemaName();
	virtual void		pushSchemaName (const char *name);
	virtual DatabaseMetaData* getMetaData();
	virtual void		release();
	virtual void		addRef();
	virtual void		validate (int optionMask);
	virtual bool		assumeRole (Role *role);
	virtual const char* getAttribute (const char *name, const char *defaultValue);
	virtual void		setAttribute (const char *name, const char *value);
	virtual bool		dropRole (Role *role);
	virtual Connection* clone();
	virtual Sequence*	findSequence(const char *schema, const char *name);
	virtual void		dropDatabase();
	virtual int			getTransactionIsolation();
	virtual void		setTransactionIsolation  (int level);
	virtual void		setSyncDisable(int value);
	virtual void		setRecordMemoryMax(uint64 maxMemory);
	virtual void		setRecordScavengeThreshold(int value);
	virtual void		setRecordScavengeFloor(int value);

	int32			getNextHandle();
	void			deleteRepositoryBlob (const char *schema, const char *repositoryName, int volume, int64 blobId);
	int				setLimit (int which, int value);
	int				getLimit (int which);
	void			checkSitePassword (const char *sitePassword);
	void			setLicenseNotRequired (bool flag);
	void			checkPreparedStatement (PreparedStatement *statement);
	void			transactionEnded();
	void			checkSitePassword (Parameters *parameters);
	int32			setTraceFlags (int32 flags);
	JString			analyze (int mask);
	void			disableTriggerClass (const char *name);
	char*			getCookie (const char *cookie, const char *name, char *temp, int length);
	const char*		currentSchemaName();
	Transaction*	getTransaction();
	void			validatePrivilege (PrivilegeObject *object, PrivType priv);
	void			setUser (User *usr);
	bool			getAutoCommit();
	void			setAutoCommit (bool flag);
	void			formatDateString(const char * format, int length, int32 date, char * buffer);
	bool			checkAccess (int32 privMask, PrivilegeObject * object);
	Table*			findTable (const char *name);
	void			requiresLicense();
	void			addImage (Image *image);
	void			init(Configuration *config);


	Statement*		findStatement (const char *cursorName);
	Statement*		findStatement (int32 handle);
	void			deleteStatement (Statement *statement);
	void			addStatement (Statement *statement);
	void			clearStatements();
	void			checkStatement (Statement *handle);
	void			registerStatement(StatementType typ, void *own, PreparedStatement *statement);
	PreparedStatement* prepareStatement (const WCString *sqlString);
	PreparedStatement* findRegisteredStatement (StatementType typ, void *own);

	Sequence*		getSequence (const char *schemaName, const char *sequenceName);
	Sequence*		findSequence (const char *name);
	int64			getSequenceValue(const WCString *schemaName, const WCString *sequenceName);
	int64			getSequenceValue (const char *sequenceName);
	int64			getSequenceValue (const WCString *sequenceName);
	int64			getSequenceValue (const char *schemaName, const char *sequenceName);

	Database*		createDatabase (const char *dbName, const char *dbFileName, const char *account, const char *password, Threads *threads);
	Database*		getDatabase(const char* dbName, const char* filename, Threads* threads);
	void			openDatabase(const char* dbName, const char *filename, const char* account, const char* password, const char* address, Threads* parent);
	void			detachDatabase();

	void			addRole (Role *role);
	Role*			findRole (const WCString *schemaName, const WCString *roleName);
	int				hasActiveRole (const char *schema, const char *name);
	int				hasActiveRole (const WCString *schema, const WCString *name);
	int				hasActiveRole (const char *name);
	int				hasRole (Role *role);
	Role*			findRole (const char *schemaName, const char *roleName);
	void			releaseActiveRoles();
	void			revert();
	void			assumeRoles ();

	void			deleteResultSet (ResultSet *resultSet);
	void			addResultSet (ResultSet *resultSet);
	ResultList*		findResultList (int32 handle);
	ResultSet*		findResultSet (int32 handle);
	void			checkResultSet (ResultSet *handle);
	void			clearResultSets();

	void			enableTriggerClass (const char *name);
	bool			enableFilterSet (FilterSet *filterSet);
	bool			enableFilterSet (const WCString *schemaName, const WCString *filterSetName);
	void			disableFilterSet(FilterSet *filterSet);
	void			disableFilterSet (const WCString *schemaName, const WCString *filterSetName);

	void			commitByXid(int xidLength, const UCHAR* xid);
	void			rollbackByXid(int xidLength, const UCHAR* xid);
	void			shutdownDatabase(void);
	void			setCurrentStatement(const char* text);
	char*			getCurrentStatement(char* buffer, uint bufferLength);

	static void		shutdown (void *connection);
	static void		shutdownDatabases();
	static void		serverExit();
	static void		link (Database *database);
	static void		unlink (Database *database);
	static void		shutdownNow();

#ifndef STORAGE_ENGINE
	void			closeJavaConnection (JavaConnection *connection);
	JavaConnection* getJavaConnection();
	JString			debugRequest (const char *request);
	int				attachDebugger (const char *sitePassword);
	int				initiateService (const char *application, const char *service);
	virtual PreparedStatement* prepareDrl (const char *drl);
#endif

	Database		*database;
	Transaction		*transaction;
	Configuration	*configuration;
	DatabaseMetaData *metaData;
	JavaConnection	*javaConnections;
	volatile INTERLOCK_TYPE	useCount;
	User			*user;
	ResultSet		*resultSets;
	LicenseToken	*licenseToken;
	Stack			nameSpace;
	LinkedList		filterSets;
	LinkedList		disabledTriggerClasses;
	Parameters		attributes;
	bool			autoCommit;
	int				numberResultSets;
	int32			traceFlags;
	void			*logFile;
	const char		*configFile;
	const char		*currentStatement;
	int				statementMaxSort;
	int				maxSort;
	int				largeSort;
	int				maxRecords;
	int				statementMaxRecords;
	int				sortCount;
	int				recordCount;
	int				nextHandle;
	int				isolationLevel;
	int				mySqlThreadId;

protected:
	Statement		*statements;
	RegisteredStatement	*registeredStatements;
	LinkedList		activeRoles;
	LinkedList		images;
	Stack			previousRoles;
	SyncObject		syncObject;
	SyncObject		syncResultSets;
	bool			licenseNotRequired;
	bool			activeDebugger;
};

extern Connection*	createLocalConnection();
extern void			generateDigest (const char *string, char *digest);
extern void			addLogListener (int mask, Listener *logListener, void *arg);
extern void			deleteLogListener (Listener *logListener, void *arg);

#endif // !defined(AFX_CONNECTION_H__02AD6A46_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
