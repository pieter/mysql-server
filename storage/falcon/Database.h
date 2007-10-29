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

// Database.h: interface for the Database class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATABASE_H__5EC961D1_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_DATABASE_H__5EC961D1_A406_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SyncObject.h"

#define ODS_VERSION1		1
#define ODS_VERSION2		2		// Fixed numeric index sign problem, 1/22/2002
#define ODS_VERSION			ODS_VERSION2

#define ODS_MINOR_VERSION0	0
#define ODS_MINOR_VERSION1	1		// Has serial log
#define ODS_MINOR_VERSION2	2		// Has SequencePages external to the section tree
#define ODS_MINOR_VERSION3	3		// Switch to variable length record numbers in index
#define ODS_MINOR_VERSION	ODS_MINOR_VERSION3
//#define AGE_GROUPS			32

#define COMBINED_VERSION(major,minor)	(major * 100 + minor)
#define VERSION_CURRENT					COMBINED_VERSION(ODS_VERSION, ODS_MINOR_VERSION)					
#define VERSION_SERIAL_LOG				COMBINED_VERSION(ODS_VERSION2, ODS_MINOR_VERSION1)

static const int FALC0N_TRACE_TRANSACTIONS	= 1;
static const int FALC0N_SYNC_TEST			= 2;
static const int FALC0N_SYNC_OBJECTS		= 4;
static const int FALC0N_FREEZE				= 8;
static const int FALC0N_REPORT_WRITES		= 16;

#define TABLE_HASH_SIZE		101

class Table;
class UnTable;
class Dbb;
CLASS(Statement);
class PreparedStatement;
class CompiledStatement;
class Table;
class Connection;
class Transaction;
class TransactionManager;
class Stream;
class InversionFilter;
class Bitmap;
class ResultList;
class ResultSet;
class TemplateManager;
class TemplateContext;
class Inversion;
class Java;
class Application;
class Applications;
class Threads;
class Scheduler;
class Scavenger;
class Session;
class SessionManager;
class RoleModel;
class ImageManager;
class LicenseManager;
class Sequence;
class SequenceManager;
class Repository;
class RepositoryManager;
class User;
class Role;
class SymbolManager;
class FilterSetManager;
class TableSpaceManager;
class Coterie;
class Parameters;
class SearchWords;
class Cache; 
class Schema;
class Configuration;
class SerialLog;
class PageWriter;
class IndexKey;
class InfoTable;
class TableSpace;
class MemMgr;
class RecordScavenge;

struct JavaCallback;

class Database
{
public:
	Database(const char *dbName, Configuration *config, Threads *parent);
	virtual ~Database();

	void			shutdownNow();
	void			dropDatabase();
	void			rollback (Transaction *transaction);
	void			commit (Transaction *transaction);
	void			start();
	void			deleteRepositoryBlob(const char *schema, const char *repositoryName, int volume, int64 blobId, Transaction *transaction);
	void			deleteRepository (Repository *repository);
	Schema*			getSchema (const char *schemaName);
	Repository*		createRepository(const char *name, const char *schema, Sequence *sequence, const char *fileName, int volume, const char *rolloverString);
	Repository*		getRepository(const char *schema, const char *name);
	Repository*		findRepository(const char *schema, const char *name);
	const char*		fetchTemplate (JString applicationName, JString templateName, TemplateContext *context);
	void			licenseCheck();
	void			cleanupRecords (RecordScavenge *recordScavenge);
	void			serverOperation (int op, Parameters *parameters);
	void			retireRecords(bool forced);
	int				getMemorySize (const char *string);
	JString			analyze(int mask);
	void			upgradeSystemTables();
	const char*		getString (const char *string);
	const char*		getSymbol (const WCString *string);
	bool			isSymbol (const char *string);
	const char*		getSymbol (const char *string);
	Role*			findRole (const WCString *schema, const WCString *roleName);
	PreparedStatement* prepareStatement (Connection *connection, const WCString *sqlStr);
	CompiledStatement* compileStatement (Connection *connection, JString sqlString);
	CompiledStatement* getCompiledStatement (Connection *connection, const WCString *sqlString);
	void			rebuildIndexes();
	void			removeFromInversion (InversionFilter *filter, Transaction *transaction);
	Transaction*	getSystemTransaction();
	int64			updateSequence (int sequenceId, int64 delta, Transaction *transaction);
	int				createSequence(int64 initialValue);
	void			ticker();
	static void		ticker (void *database);
	void			scavenge();
	void			validate (int optionMask);
	Role*			findRole(const char *schemaName, const char * roleName);
	User*			findUser (const char *account);
	User*			createUser (const char *account, const char *password, bool encrypted, Coterie *coterie);
	int				getMaxKeyLength(void);

#ifndef STORAGE_ENGINE
	void		startSessionManager();
	void		genHTML (ResultSet *resultSet, const char *series, const char *type, 
						 TemplateContext *context, Stream *stream, JavaCallback *callback);
	void		genHTML(ResultSet * resultSet, const WCString *series, const WCString *type, TemplateContext *context, Stream *stream, JavaCallback *callback);
	JString		expandHTML(ResultSet *resultSet, const WCString *applicationName, const char *source, TemplateContext *context, JavaCallback *callback);
	void		zapLinkages();
	void		checkManifest();
	void		detachDebugger();
	JString		debugRequest(const char *request);
	int			attachDebugger();
	Application* getApplication (const char *applicationName);
#endif

	void			invalidateCompiledStatements (Table *table);
	void			shutdown();
	void			execute (const char *sql);
	void			addTable (Table *table);
	void			dropTable (Table *table, Transaction *transaction);
	void			flushInversion(Transaction *transaction);
	bool			matches (const char *fileName);
	Table*			loadTable (ResultSet *resultSet);
	Table*			getTable (int tableId);
	void			reindex(Transaction *transaction);
	void			search (ResultList *resultList, const char *string);
	int32			addInversion (InversionFilter *filter, Transaction *transaction);
	void			clearDebug();
	void			setDebug();
	void			commitSystemTransaction();
	void			rollbackSystemTransaction(void);
	bool			flush(int64 arg);
	
	Transaction*	startTransaction(Connection *connection);
	CompiledStatement* getCompiledStatement (Connection *connection, const char *sqlString);
	void			openDatabase (const char *filename);
	PreparedStatement* prepareStatement (Connection* connection, const char *sqlStr);
	Statement*		createStatement (Connection *connection);
	void			release();
	void			addRef();
	PreparedStatement* prepareStatement (const char *sqlStr);
	void			addSystemTables();
	Table*			addTable (User *owner, const char *name, const char *schema, TableSpace *tableSpace);
	Table*			findTable (const char *schema, const char *name);
	Statement*		createStatement();
	virtual void	createDatabase (const char *filename);
	void			renameTable(Table* table, const char* newSchema, const char* newName);
	bool			hasUncommittedRecords(Table* table, Transaction *transaction);
	void			validateCache(void);
	void			commitByXid(int xidLength, const UCHAR* xid);
	void			rollbackByXid(int xidLength, const UCHAR* xid);
	void			getTransactionSummaryInfo(InfoTable* infoTable);
	void			updateCardinalities(void);
	void			getIOInfo(InfoTable* infoTable);
	void			getTransactionInfo(InfoTable* infoTable);
	void			getSerialLogInfo(InfoTable* infoTable);
	void 			setSyncDisable(int value);
	void			sync(uint threshold);
	void			preUpdate();
	void			setRecordMemoryMax(uint64 value);
	void			setRecordScavengeThreshold(int value);
	void			setRecordScavengeFloor(int value);
	void			forceRecordScavenge(void);
	void			debugTrace(void);
	void			pageCacheFlushed(int64 flushArg);
	JString			setLogRoot(const char *defaultPath, bool create);

	Dbb				*dbb;
	Cache			*cache;
	JString			name;
	Database		*next;					// used by Connection
	Database		*prior;					// used by Connection
	Schema			*schemas [TABLE_HASH_SIZE];
	Table			*tables [TABLE_HASH_SIZE];
	Table			*tablesModId [TABLE_HASH_SIZE];
	Table			*tableList;
	Table			*zombieTables;
	UnTable			*unTables [TABLE_HASH_SIZE];
	CompiledStatement	*compiledStatements;
	Configuration	*configuration;
	SerialLog		*serialLog;
	Connection		*systemConnection;
	int				nextTableId;
	bool			formatting;
	bool			licensed;
	bool			fieldExtensions;
	bool			utf8;
	bool			panicShutdown;
	bool			shuttingDown;
	bool			longSync;
	int				useCount;
	int				sequence;
	int				stepNumber;
	int				scavengeCycle;
	Java			*java;
	Applications	*applications;
	SyncObject		syncObject;
	SyncObject		syncTables;
	SyncObject		syncStatements;
	SyncObject		syncAddStatement;
	SyncObject		syncSysConnection;
	SyncObject		syncResultSets;
	SyncObject		syncConnectionStatements;
	SyncObject		syncScavenge;
	SyncObject		syncSerialLogIO;
	Threads			*threads;
	Scheduler		*scheduler;
	Scheduler		*internalScheduler;
	Scavenger		*scavenger;
	Scavenger		*garbageCollector;
	TemplateManager		*templateManager;
	ImageManager		*imageManager;
	SessionManager		*sessionManager;
	RoleModel			*roleModel;
	LicenseManager		*licenseManager;
	SequenceManager		*sequenceManager;
	SymbolManager		*symbolManager;
	RepositoryManager	*repositoryManager;
	TransactionManager	*transactionManager;
	FilterSetManager	*filterSetManager;
	TableSpaceManager	*tableSpaceManager;
	SearchWords			*searchWords;
	Thread				*tickerThread;
	PageWriter			*pageWriter;
	PreparedStatement	*updateCardinality;
	MemMgr				*recordDataPool;
	time_t				startTime;
	
	volatile time_t		deltaTime;
	volatile time_t		timestamp;
	volatile int		numberQueries;
	volatile int		numberRecords;
	volatile int		numberTemplateEvals;
	volatile int		numberTemplateExpands;
	int					odsVersion;
	int					noSchedule;

	volatile INTERLOCK_TYPE	currentGeneration;
	//volatile long	overflowSize;
	//volatile long 	ageGroupSizes [AGE_GROUPS];
	uint64				recordMemoryMax;
	uint64				recordScavengeThreshold;
	uint64				recordScavengeFloor;
	int64				lastRecordMemory;
	time_t				creationTime;
	volatile time_t		lastScavenge;
};

#endif // !defined(AFX_DATABASE_H__5EC961D1_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
