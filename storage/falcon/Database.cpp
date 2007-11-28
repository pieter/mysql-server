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

// Database.cpp: implementation of the Database class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <errno.h>
#include "Engine.h"
#include "Database.h"
#include "Dbb.h"
#include "PStatement.h"
#include "CompiledStatement.h"
#include "Table.h"
#include "UnTable.h"
#include "Index.h"
#include "IndexKey.h"
#include "Connection.h"
#include "Transaction.h"
#include "Stream.h"
#include "SQLError.h"
#include "RSet.h"
#include "Search.h"
#include "ImageManager.h"
#include "SequenceManager.h"
#include "Inversion.h"
#include "Sync.h"
#include "Threads.h"
#include "Thread.h"
#include "Scheduler.h"
#include "Scavenger.h"
#include "RoleModel.h"
#include "User.h"
#include "Registry.h"
#include "SymbolManager.h"
#include "FilterSetManager.h"
#include "Log.h"
#include "DateTime.h"
#include "Interlock.h"
#include "Cache.h"
#include "NetfraVersion.h"
#include "OpSys.h"
#include "SearchWords.h"
#include "Repository.h"
#include "RepositoryManager.h"
#include "Schema.h"
#include "Configuration.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "PageWriter.h"
#include "Trigger.h"
#include "TransactionManager.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "InfoTable.h"
#include "MemoryManager.h"
#include "MemMgr.h"
#include "RecordScavenge.h"
#include "LogStream.h"
#include "SyncTest.h"
#include "PriorityScheduler.h"
#include "Sequence.h"

#ifndef STORAGE_ENGINE
#include "Applications.h"
#include "Application.h"
#include "JavaVM.h"
#include "Java.h"
#include "Template.h"
#include "Templates.h"
#include "TemplateContext.h"
#include "TemplateManager.h"
#include "Session.h"
#include "SessionManager.h"
#include "DataResourceLocator.h"
#else
extern unsigned int falcon_page_size;
#endif

#ifdef LICENSE
#include "LicenseManager.h"
#include "LicenseProduct.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)
#define EXPIRATION_DAYS					180

#define STATEMENT_RETIREMENT_AGE	60
#define RECORD_RETIREMENT_AGE		60

extern uint falcon_debug_trace;

static const char *createTables = 
	"create table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			primary key (tableName, schema));";

static const char *createOds3aTables = 
	"upgrade table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			cardinality bigint,\
			primary key (tableName, schema));";

static const char *createOds3bTables = 
	"upgrade table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			cardinality bigint,\
			tablespace varchar(128),\
			primary key (tableName, schema));";

static const char *createOds2Fields = 
	"create table Fields (\
			field varchar (128) not null,\
			tableName varchar (128) not null,\
			schema varchar (128) not null,"
			//"domainName varchar (128),"
			"collationsequence varchar (128),"
			"fieldId int,\
			dataType int,\
			length int,\
			scale int,\
			flags int,\
			remarks text,\
			primary key (schema, tableName, field));";

static const char *createOds3Fields = 
	"upgrade table Fields ("
		"field varchar (128) not null,"
		"tableName varchar (128) not null,"
		"schema varchar (128) not null,"
		"domainName varchar (128),"
		"repositoryName varchar (128),"
		"collationsequence varchar (128),"
		"fieldId int,"
		"dataType int,"
		"length int,"
		"scale int,"
		"precision int,"
		"flags int,"
		"remarks text,"
		"primary key (schema, tableName, field));";

static const char *createFieldDomainName = 
	"upgrade index FieldDomainName on Fields (domainName);";

static const char *createFieldCollationSequenceName = 
	"create index FieldCollationSequenceName on Fields (collationsequence);";

static const char *createFormats = 
	"create table Formats (\
			tableId int not null,\
			fieldId int not null,\
			version int not null,\
			dataType int,\
			offset int,\
			length int,\
			scale int,\
			maxId int,\
			primary key (tableId, version, fieldId));";

static const char *createIndexes = 
	"create table Indexes (\
			indexName varchar (128) not null,\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			indexType int,\
			fieldCount int,\
			indexId int,\
			primary key (schema, indexName));";

static const char *createOd3IndexIndex =
	"create index index_table on indexes (schema, tableName)";

static const char *createIndexFields = 
	"create table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			primary key (schema, indexName, field));";

static const char *createOd3IndexFieldsIndex =
	"create index indexfields_table on IndexFields (schema, tableName)";

static const char *createOds3IndexFields = 
	"upgrade table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			partial int,\
			primary key (schema, indexName, field));";

static const char *createOds3aIndexFields = 
	"upgrade table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			partial int,\
			records_per_value int,\
			primary key (schema, indexName, field));";

static const char *createForeignKeys = 
	"create table ForeignKeys (\
			primaryTableId int not null,\
			primaryFieldId int not null,\
			foreignTableId int not null,\
			foreignFieldId int not null,\
			numberKeys int,\
			position int,\
			updateRule smallint,\
			deleteRule smallint,\
			deferrability smallint,\
			primary key (foreignTableId, primaryTableId, foreignFieldId));";

static const char *createForeignKeysIndex = 
	"create index ForeignKeysIndex on ForeignKeys\
			(primaryTableId);";

static const char *createDomains = 
	"create table Domains ("
			"domainName varchar (128) not null,"
			"schema varchar (128) not null,"
		    "dataType int,"
		    "length int,"
		    "scale int,"
		    "remarks text,"
			"primary key (domainName, schema));";

//static const char *createOds3Domains = 
//	"create table Domains ("
//			"domainName varchar (128) not null,"
//			"schema varchar (128) not null,"
//		    "dataType int,"
//		    "length int,"
//		    "scale int,"
//		    "remarks text,"
//			"primary key (domainName, schema));";

static const char *createView_tables = 
	"upgrade table view_tables ("
		"viewName varchar (128) not null,"
		"viewSchema varchar (128) not null,"
		"sequence int not null,"
		"tableName varchar (128) not null,"
		"schema varchar (128) not null,"
		"primary key (viewSchema,viewName,sequence))";

static const char *createRepositories = 
	"upgrade table repositories ("
		"repositoryName varchar (128) not null,"
		"schema varchar (128) not null,"
		"sequenceName varchar (128),"
		"filename varchar (128),"
		"rollovers varchar (128),"
		"currentVolume int,"
		"primary key (repositoryName, schema))";

static const char *createSchemas =
	"upgrade table schemas ("
		"schema varchar (128) not null primary key,"
		"sequence_interval int,"
		"system_id int)";

static const char *createTableSpaces =
	"upgrade table tablespaces ("
		"tablespace varchar(128) not null primary key,"
		"tablespace_id int not null,"
		"filename varchar(512) not null,"
		"status int,"
		"max_size int)";

static const char *createTableSpaceSequence = 
	"upgrade sequence tablespace_ids";

static const char *ods2Statements [] =
    {
	createTables,
	createOds2Fields,
	createFieldCollationSequenceName,
	createFormats,
	createIndexes,
	createIndexFields,
	createForeignKeys,
	createForeignKeysIndex,
	createDomains,

	"grant select on tables to public",
	"grant select on fields to public",
	"grant select on formats to public",
	"grant select on indexes to public",
	"grant select on indexFields to public",
	"grant select on foreignKeys to public",
	"grant select on Domains to public",

	NULL
	};
	
static const char *ods2UpgradeStatements [] = {
	createView_tables,
	createRepositories,
	createSchemas,
	"grant select on repositories to public",
	"grant select on schemas to public",

	NULL
	};

static const char *ods3Upgrade  [] = {
	createOds3Fields,
	createFieldDomainName,
	createOds3IndexFields,
	createOd3IndexIndex,
	createOd3IndexFieldsIndex,
	NULL
	};

static const char *ods3aUpgrade  [] = {
	createOds3aTables,
	createOds3aIndexFields,
	NULL
	};

static const char *ods3bUpgrade  [] = {
	createOds3bTables,
	createTableSpaces,
	createTableSpaceSequence,
	"grant select on tablespaces to public",
	NULL
	};

static const char *changedTables [] = {
	"TABLES",
	"FIELDS",
	"INDEXFIELDS",
	//"INDEXES",
	//"FORMATS",
	//"FOREIGNKEYS",
	//"DOMAINS",
	//"REPOSITORIES",
	NULL
	};
	
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Database::Database(const char *dbName, Configuration *config, Threads *parent)
{
	panicShutdown = false;
	name = dbName;
	configuration = config;
	useCount = 1;
	nextTableId = 0;
	compiledStatements = NULL;
	memset(tables, 0, sizeof (tables));
	memset(tablesModId, 0, sizeof (tablesModId));
	memset(unTables, 0, sizeof (unTables));
	memset(schemas, 0, sizeof (schemas));
	currentGeneration = 1;
	tableList = NULL;
	recordMemoryMax = configuration->recordMemoryMax;
	recordScavengeFloor = configuration->recordScavengeFloor;
	recordScavengeThreshold = configuration->recordScavengeThreshold;
	lastRecordMemory = 0;
	utf8 = false;
	stepNumber = 0;
	shuttingDown = false;
	pendingIOErrors = 0;
	noSchedule = 0;
	//noSchedule = 1;

	fieldExtensions = false;
	cache = NULL;
	dbb = new Dbb(this);
	numberQueries = 0;
	numberRecords = 0;
	numberTemplateEvals = 0;
	numberTemplateExpands = 0;
	threads = new Threads (parent);
	startTime = time (NULL);

	symbolManager = NULL;
	templateManager = NULL;
	imageManager = NULL;
	roleModel = NULL;
	licenseManager = NULL;
	transactionManager = NULL;
	applications = NULL;
	systemConnection = NULL;
	searchWords = NULL;
	java = NULL;
	scheduler = NULL;
	internalScheduler = NULL;
	scavenger = NULL;
	garbageCollector = NULL;
	sequenceManager = NULL;
	repositoryManager = NULL;
	sessionManager = NULL;
	filterSetManager = NULL;
	tableSpaceManager = NULL;
	timestamp = time (NULL);
	tickerThread = NULL;
	serialLog = NULL;
	pageWriter = NULL;
	zombieTables = NULL;
	updateCardinality = NULL;
	ioScheduler = new PriorityScheduler;
	lastScavenge = 0;
	scavengeCycle = 0;
	serialLogBlockSize = configuration->serialLogBlockSize;
	longSync = false;
	recordDataPool = MemMgrGetFixedPool(MemMgrPoolRecordData);
	//recordObjectPool = MemMgrGetFixedPool(MemMgrPoolRecordObject);
	syncObject.setName("Database::syncObject");
	syncTables.setName("Database::syncTables");
	syncStatements.setName("Database::syncStatements");
	syncAddStatement.setName("Database::syncAddStatement");
	syncSysConnection.setName("Database::syncSysConnection");
	syncResultSets.setName("Database::syncResultSets");
	syncConnectionStatements.setName("Database::syncConnectionStatements");
	syncScavenge.setName("Database::syncScavenge");
}


void Database::start()
{
	symbolManager = new SymbolManager;

#ifdef LICENSE
	licenseManager = new LicenseManager(this);
#endif

#ifndef STORAGE_ENGINE
	templateManager = new TemplateManager(this);
	java = new Java(this, applications, configuration->classpath);
	sessionManager = new SessionManager(this);
	applications = new Applications(this);
#endif

	imageManager = new ImageManager(this);
	roleModel = new RoleModel(this);
	systemConnection = new Connection(this);
	tableSpaceManager = new TableSpaceManager(this);
	dbb->serialLog = serialLog = new SerialLog(this, configuration->checkpointSchedule, configuration->maxTransactionBacklog);
	pageWriter = new PageWriter(this);
	searchWords = new SearchWords (this);
	systemConnection->setLicenseNotRequired (true);
	systemConnection->pushSchemaName ("SYSTEM");
	addSystemTables();
	scheduler = new Scheduler(this);
	internalScheduler = new Scheduler(this);
	scavenger = new Scavenger(this, scvRecords, configuration->scavengeSchedule);
	garbageCollector = new Scavenger(this, scvJava, configuration->gcSchedule);
	sequenceManager = new SequenceManager(this);
	repositoryManager = new RepositoryManager(this);
	transactionManager = new TransactionManager(this);
	internalScheduler->addEvent(repositoryManager);
	filterSetManager = new FilterSetManager(this);
	timestamp = time(NULL);
	tickerThread = threads->start("Database::Database", &Database::ticker, this);
	internalScheduler->addEvent(scavenger);
	internalScheduler->addEvent(garbageCollector);
	internalScheduler->addEvent(serialLog);
	pageWriter->start();
	cache->setPageWriter(pageWriter);
}

Database::~Database()
{
	for (Table *table; (table = tableList);)
		{
		tableList = table->next;
		delete table;
		}

	int n;

	for (n = 0; n < TABLE_HASH_SIZE; ++n)
		for (UnTable *untable; (untable = unTables [n]);)
			{
			unTables [n] = untable->collision;
			delete untable;
			}
					
	for (n = 0; n < TABLE_HASH_SIZE; ++n)
		for (Schema *schema; (schema = schemas [n]);)
			{
			schemas [n] = schema->collision;
			delete schema;
			}
					
	if (systemConnection)
		{
		systemConnection->rollback();
		systemConnection->close();
		}

	for (CompiledStatement *statement = compiledStatements; statement;)
		{
		CompiledStatement *object = statement;
		statement = statement->next;
		if (object->useCount > 1)
			Log::debug ("~Database: '%s' in use\n", (const char*) object->sqlString);
		object->release();
		}

	delete dbb;

	if (scavenger)
		scavenger->release();

	if (garbageCollector)
		garbageCollector->release();

	if (threads)
		{
		threads->shutdownAll();
		threads->waitForAll();
		threads->release();
		}

#ifndef STORAGE_ENGINE
	delete java;
	delete templateManager;
	delete sessionManager;
	delete applications;
#endif

#ifdef LICENSE
	delete licenseManager;
#endif

	if (scheduler)
		scheduler->release();

	if (internalScheduler)
		internalScheduler->release();


	delete imageManager;
	delete sequenceManager;
	delete searchWords;
	delete serialLog;
	delete pageWriter;
	delete tableSpaceManager;
	delete cache;
	delete roleModel;
	delete symbolManager;
	delete filterSetManager;
	delete repositoryManager;
	delete transactionManager;
	delete ioScheduler;
}

void Database::createDatabase(const char * filename)
{
	// If valid, use the user-defined serial log path, otherwise use the default.
	
	JString logRoot = setLogRoot(filename, true);
	
	//TBD: Return error to server.
	
#ifdef STORAGE_ENGINE
	int page_size = falcon_page_size;
#else
	int page_size = PAGE_SIZE;
#endif

	cache = dbb->create(filename, page_size, configuration->pageCacheSize, HdrDatabaseFile, 0, "", 0);
	
	try
		{
		start();
		serialLog->open(logRoot, true);
		serialLog->start();
		sequence = dbb->sequence;
		odsVersion = dbb->odsVersion;
		dbb->createInversion(0);
		Table *table;
		Transaction *sysTrans = getSystemTransaction();
		
		for (table = tableList; table; table = table->next)
			table->create ("SYSTEM TABLE", sysTrans);

		for (table = tableList; table; table = table->next)
			table->save();

		roleModel->createTables();
		sequenceManager->initialize();
		Trigger::initialize (this);
		systemConnection->commit();

#ifndef STORAGE_ENGINE
		java->initialize();
		checkManifest();
		templateManager->getTemplates ("base");

#ifdef LICENSE
		licenseManager->initialize();
		licenseCheck();
#endif

		startSessionManager();
#endif

		imageManager->getImages ("base", NULL);
		filterSetManager->initialize();
		upgradeSystemTables();
		scheduler->start();
		internalScheduler->start();
		serialLogBlockSize = serialLog->getBlockSize();
		dbb->updateSerialLogBlockSize();
		commitSystemTransaction();
		serialLog->checkpoint(true);
		}
	catch (...)
		{
		dbb->closeFile();
		dbb->deleteFile();
		
		throw;
		}
}

void Database::openDatabase(const char * filename)
{
	cache = dbb->open (filename, configuration->pageCacheSize, 0);
	start();

	if (   dbb->logRoot.IsEmpty()
	    || (!configuration->serialLogDir.IsEmpty() && dbb->logRoot != configuration->serialLogDir))
		{
		// If valid, use the user-defined serial log path, otherwise use the default.
		
		dbb->logRoot = setLogRoot(filename, true);
		}
		
	if (serialLog)
		{
		if (COMBINED_VERSION(dbb->odsVersion, dbb->odsMinorVersion) >= VERSION_SERIAL_LOG)
			{
			if (dbb->logLength)
				serialLog->copyClone(dbb->logRoot, dbb->logOffset, dbb->logLength);

			try
				{
				serialLog->open(dbb->logRoot, false);
				}
			catch (SQLException&)
				{
				const char *p = strrchr(filename, '.');
				JString logRoot = (p) ? JString(filename, (int) (p - filename)) : name;
				bool failed = true;
				
				try
					{
					serialLog->open(logRoot, false);
					failed = false;
					}
				catch (...)
					{
					}
				
				if (failed)
					throw;
				}
			
			if (dbb->tableSpaceSectionId)
				tableSpaceManager->bootstrap(dbb->tableSpaceSectionId);

			serialLog->recover();
			serialLog->start();
			}
		else
			{
			dbb->enableSerialLog();
			serialLog->open(dbb->logRoot, true);
			serialLog->start();
			}
		}

	sequence = dbb->sequence;
	odsVersion = dbb->odsVersion;
	utf8 = dbb->utf8;
	int indexId = 0;
	int sectionId = 0;
	
	for (Table *table = tableList; table; table = table->next)
		{
		table->setDataSection (sectionId++);
		table->setBlobSection (sectionId++);
		
		FOR_INDEXES (index, table)
			index->setIndex (indexId++);
		END_FOR;
		}

	PreparedStatement *statement = prepareStatement ("select tableid from tables");
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		int n = resultSet->getInt (1);
		
		if (n >= nextTableId)
			nextTableId = n + 1;
		}

	resultSet->close();
	statement->close();
	upgradeSystemTables();
	Trigger::initialize (this);
	serialLog->checkpoint(true);
	//validate(0);

#ifndef STORAGE_ENGINE

#ifdef LICENSE
	licenseManager->initialize();
	licenseCheck();
#endif

	java->initialize();
	checkManifest();
	startSessionManager();
#endif

	sequenceManager->initialize();
	filterSetManager->initialize();
	searchWords->initialize();
	roleModel->initialize();

	if (serialLog)
		serialLog->recoverLimboTransactions();
		
#ifndef STORAGE_ENGINE
	getApplication ("base");
#endif

	tableSpaceManager->initialize();
	internalScheduler->start();

	if (configuration->schedulerEnabled)
		scheduler->start();
}

#ifndef STORAGE_ENGINE
void Database::startSessionManager()
{
	try
		{
		java->run (systemConnection);
		sessionManager->start (systemConnection);
		}
	catch (SQLException &exception)
		{
		Log::debug ("Exception during Java initialization: %s\n", (const char*) exception.getText());
		const char *stackTrace = exception.getTrace();
		if (stackTrace && stackTrace [0])
			Log::debug ("Stack trace:\n%s\n", stackTrace);
		}
}

void Database::genHTML(ResultSet * resultSet, const char *series, const char *type, TemplateContext *context, Stream *stream, JavaCallback *callback)
{
	Templates *tmpls = templateManager->getTemplates (series);
	tmpls->genHTML (type, resultSet, context, 0, stream, callback);
}

void Database::genHTML(ResultSet *resultSet, const WCString *series, const WCString *type, TemplateContext *context, Stream *stream, JavaCallback *callback)
{
	Templates *tmpls = templateManager->getTemplates (series);
	tmpls->genHTML (symbolManager->getSymbol(type), resultSet, context, 0, stream, callback);
}


JString Database::expandHTML(ResultSet *resultSet, const WCString *applicationName, const char *source, TemplateContext *context, JavaCallback *callback)
{
	Templates *tmpls = templateManager->getTemplates (applicationName);

	return tmpls->expandHTML (source, resultSet, context, callback);
}


const char* Database::fetchTemplate(JString applicationName, JString templateName, TemplateContext *context)
{
	Templates *templates = templateManager->getTemplates (applicationName);
	Template *pTemplate = templates->findTemplate (context, templateName);

	if (!pTemplate)
		return NULL;

	return pTemplate->body;
}


void Database::zapLinkages()
{
	for (Table *table = tableList; table; table = table->next)
		table->zapLinkages();

	sessionManager->zapLinkages();
}

void Database::checkManifest()
{
	if (!java->findManifest ("netfrastructure/model/Application"))
		throw SQLError (LICENSE_ERROR, "missing manifest for base application");
}

int Database::attachDebugger()
{
	return java->attachDebugger();
}

JString Database::debugRequest(const char *request)
{
	return java->debugRequest(request);
}

void Database::detachDebugger()
{
	java->detachDebugger();
}

Application* Database::getApplication(const char * applicationName)
{
	return applications->getApplication (applicationName);
}

int Connection::initiateService(const char *applicationName, const char *service)
{
	Application *application = database->getApplication (applicationName);

	if (!application)
		throw SQLEXCEPTION (RUNTIME_ERROR, "application '%s' is unknown",
								(const char*) applicationName);

	application->pushNameSpace (this);
	Session *session = NULL;
	int port;
	
	try
		{
		session = database->sessionManager->createSession (application);
		port = database->sessionManager->initiateService (this, session, service);
		}
	catch (...)
		{
		if (session)
			session->release();
		throw;
		}

	session->release();

	return port;
}

PreparedStatement* Connection::prepareDrl(const char * drl)
{
	DataResourceLocator locator;

	return locator.prepareStatement (this, drl);
}

#endif	// STORAGE_ENGINE

void Database::serverOperation(int op, Parameters *parameters)
{
	switch (op)
		{
#ifndef STORAGE_ENGINE
		case opTraceAll:
			java->traceAll();
			break;
#endif

#ifdef LICENSE
		case opInstallLicense:
			{
			const char *license = parameters->findValue ("license", NULL);
			if (!license)
				throw SQLEXCEPTION (RUNTIME_ERROR, "installLicense requires license");
			licenseManager->installLicense (license);
			licenseCheck();
			break;
			}
#endif

		case opShutdown:
			break;

		case opClone:
		case opCreateShadow:
			{
			const char *fileName = parameters->findValue("fileName", NULL);

			if (!fileName)
				throw SQLEXCEPTION (RUNTIME_ERROR, "Filename required to shadow database");

			dbb->cloneFile(this, fileName, op == opCreateShadow);
			}
			break;

		default:
			throw SQLEXCEPTION (RUNTIME_ERROR, "Server operation %d is not currently supported", op);
		}

}

Statement* Database::createStatement()
{
	return systemConnection->createStatement();
}

Table* Database::findTable (const char *schema, const char *name)
{
	if (!schema)
		return NULL;

	schema = symbolManager->getSymbol (schema);
	name = symbolManager->getSymbol (name);
	Sync sync (&syncTables, "Database::findTable");
	sync.lock (Shared);
	int slot = HASH (name, TABLE_HASH_SIZE);
	Table *table;

	for (table = tables [slot]; table; table = table->collision)
		if (table->name == name && table->schemaName == schema)
			return table;

	sync.unlock();

	for (UnTable *untable = unTables [slot]; untable;
		 untable = untable->collision)
		if (untable->name == name && untable->schemaName == schema)
			return NULL;

	Sync syncSystemTransaction(&syncSysConnection, "Database::findTable");
	syncSystemTransaction.lock(Shared);
	
	PStatement statement = prepareStatement (
		(fieldExtensions) ?
			"select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,cardinality,tablespace \
				from Tables where tableName=? and schema=?" :
			"select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,0,'' \
				from Tables where tableName=? and schema=?");
				
	statement->setString (1, name);
	statement->setString (2, schema);
	RSet resultSet = statement->executeQuery();
	table = loadTable (resultSet);
	resultSet.close();
	statement.close();

	if (!table)
		{
		UnTable *untable = new UnTable (schema, name);
		untable->collision = unTables [slot];
		unTables [slot] = untable;
		}

	return table;
}

Table* Database::addTable(User *owner, const char *name, const char *schema, TableSpace *tableSpace)
{
	if (!schema)
		throw SQLEXCEPTION(DDL_ERROR, "no schema defined for table %s\n", name);
		
	if (!formatting && findTable (schema, name))
		throw SQLEXCEPTION (DDL_ERROR, "table %s is already defined", name);

	Table *table = new Table (this, nextTableId++, schema, name, tableSpace);
	addTable (table);

	return table;
}

/***
int32 Database::createSection(Transaction *transaction)
{
	return dbb->createSection(TRANSACTION_ID(transaction));
}
***/

void Database::addSystemTables()
{
	formatting = true;
	Statement *statement = createStatement();

	for (const char **sql = ods2Statements; *sql; ++sql)
		statement->execute (*sql);

	statement->close();
	formatting = false;
}

PreparedStatement* Database::prepareStatement(const char * sqlStr)
{
	return systemConnection->prepareStatement (sqlStr);
}

void Database::addRef()
{
	++useCount;
}

void Database::release()
{
	int n = --useCount;

	if (n == 1 && systemConnection)
		{
		Connection *temp = systemConnection;
		temp->commit();
		systemConnection = NULL;
		temp->close();
		}
	else if (n == 0)
		{
		shutdown();
		delete this;
		}
}

Statement* Database::createStatement(Connection * connection)
{
	return new Statement (connection, this);
}

PreparedStatement* Database::prepareStatement(Connection * connection, const char *sqlStr)
{
	PreparedStatement *statement = new PreparedStatement (connection, this);

	try
		{
		statement->setSqlString (sqlStr);
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	return statement;
}


PreparedStatement* Database::prepareStatement(Connection *connection, const WCString *sqlStr)
{
	PreparedStatement *statement = new PreparedStatement (connection, this);

	try
		{
		statement->setSqlString (sqlStr);
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	return statement;
}

CompiledStatement* Database::getCompiledStatement(Connection *connection, const char * sqlString)
{
	Sync sync (&syncStatements, "Database::getCompiledStatement(1)");
	sync.lock (Shared);
	//printf("%s\n", (const char*) sqlString);

	for (CompiledStatement *statement = compiledStatements; statement; statement = statement->next)
		if (statement->sqlString == sqlString && statement->validate (connection))
			{
			statement->addRef();
			sync.unlock();
			try
				{
				statement->checkAccess (connection);
				}
			catch (...)
				{
				statement->release();
				throw;
				}
			return statement;
			}

	sync.unlock();

	return compileStatement (connection, sqlString);
}


CompiledStatement* Database::getCompiledStatement(Connection *connection, const WCString *sqlString)
{
	Sync sync (&syncStatements, "Database::getCompiledStatement(WC)");
	sync.lock (Shared);
	//JString str(sqlString);
	//printf("%s\n", (const char*) str);
	
	for (CompiledStatement *statement = compiledStatements; statement; statement = statement->next)
		if (statement->sqlString == sqlString && statement->validate (connection))
			{
			statement->addRef();
			sync.unlock();
			try
				{
				statement->checkAccess (connection);
				}
			catch (...)
				{
				statement->release();
				throw;
				}
			return statement;
			}

	sync.unlock();
	JString sql (sqlString);

	return compileStatement (connection, sql);
}

CompiledStatement* Database::compileStatement(Connection *connection, JString sqlString)
{
	CompiledStatement *statement = new CompiledStatement (connection);

	try
		{
		statement->compile (sqlString);
		statement->checkAccess (connection);
		}
	catch (...)
		{
		delete statement;
		throw;
		}

	if (statement->useable && 
		(statement->numberParameters > 0 || !statement->filters.isEmpty()))
		{
		Sync sync (&syncStatements, "Database::compileStatement(1)");
		sync.lock (Shared);
		Sync sync2 (&syncAddStatement, "Database::compileStatement(2)");
		sync2.lock (Exclusive);
		statement->addRef();
		statement->next = compiledStatements;
		compiledStatements = statement;
		}

	return statement;
}

Transaction* Database::startTransaction(Connection *connection)
{
	return transactionManager->startTransaction(connection);
}


bool Database::flush(int64 arg)
{
	if (cache->flushing)
		return false;
			
	serialLog->preFlush();
	cache->flush(arg);
	
	return true;
}

void Database::commitSystemTransaction()
{
	Sync sync (&syncSysConnection, "Database::commitSystemTransaction");
	sync.lock (Exclusive);
	systemConnection->commit();
}

void Database::rollbackSystemTransaction(void)
{
	Sync sync (&syncSysConnection, "Database::commitSystemTransaction");
	sync.lock (Exclusive);
	systemConnection->rollback();
}

void Database::setDebug()
{
	dbb->setDebug();
}

void Database::clearDebug()
{
	dbb->setDebug();
}

int32 Database::addInversion(InversionFilter * filter, Transaction *transaction)
{
	return dbb->inversion->addInversion (filter, TRANSACTION_ID(transaction), true);
}

void Database::removeFromInversion(InversionFilter *filter, Transaction *transaction)
{
	dbb->inversion->addInversion (filter, TRANSACTION_ID(transaction), false);
}

void Database::search(ResultList *resultList, const char * string)
{
	Search search (string, searchWords);
	search.search (resultList);
}

void Database::reindex(Transaction *transaction)
{
	//rebuildIndexes();

	dbb->inversion->deleteInversion (TRANSACTION_ID(transaction));
	dbb->inversion->createInversion (TRANSACTION_ID(transaction));

	for (Table *table = tableList; table; table = table->next)
		table->reIndexInversion(transaction);
}

Table* Database::getTable(int tableId)
{
	Table *table;

	for (table = tablesModId [tableId % TABLE_HASH_SIZE]; table; 
		 table = table->idCollision)
		if (table->tableId == tableId)
			return table;

	PStatement statement = prepareStatement (
		"select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,cardinality,tablespace \
		 from system.Tables where tableid=?");
	statement->setInt (1, tableId);
	RSet resultSet = statement->executeQuery();
	table = loadTable (resultSet);

	return table;
}

Table* Database::loadTable(ResultSet * resultSet)
{
	Sync sync (&syncTables, "Database::loadTable");

	if (!resultSet->next())
		return NULL;

	const char *name = getString (resultSet->getString(1));
	int version = resultSet->getInt (5);
	const char *schemaName = getString (resultSet->getString(6));
	const char *tableSpaceName = getString (resultSet->getString(9));
	TableSpace *tableSpace = NULL;

	if (tableSpaceName[0])
		tableSpace = tableSpaceManager->findTableSpace(tableSpaceName);

	Table *table = new Table(this, schemaName, name, resultSet->getInt(2), version, resultSet->getLong(8), tableSpace);

	int dataSection = resultSet->getInt (3);

	if (dataSection)
		{
		table->setDataSection (dataSection);
		table->setBlobSection (resultSet->getInt (4));
		}
	else
		{
		const char *viewDef = resultSet->getString (7);
		if (viewDef [0])
			{
			CompiledStatement statement (systemConnection);
			JString string;
			
			// Do a little backward compatibility
			
			if (strncmp (viewDef, "create view ", strlen("create view ")) == 0)
				string = viewDef;
			else
				string.Format ("create view %s.%s %s", schemaName, name, viewDef);
				
			table->setView (statement.getView (string));
			}
		}

	table->loadStuff();
	sync.lock (Exclusive);
	addTable (table);

	return table;
}


bool Database::matches(const char * fileName)
{
	return strcasecmp (dbb->fileName, fileName) == 0;
}

void Database::flushInversion(Transaction *transaction)
{
	dbb->inversion->flush (TRANSACTION_ID(transaction));
}

void Database::dropTable(Table * table, Transaction *transaction)
{
	table->checkDrop();

	// Check for records in active transactions.  If so, barf

	if (hasUncommittedRecords(table, transaction))
		throw SQLError(UNCOMMITTED_UPDATES, "table %s.%s has uncommitted updates and can't be dropped", 
						table->schemaName, table->name);
			
	// OK, now make sure any records are purged out of committed transactions as well
	
	transactionManager->dropTable(table, transaction);
	Sync sync (&syncTables, "Database::dropTable");
	sync.lock (Exclusive);

	// Remove table from linear table list

	Table **ptr;

	for (ptr = &tableList; *ptr; ptr = &((*ptr)->next))
		if (*ptr == table)
			{
			*ptr = table->next;
			break;
			}

	// Remove table from name hash table

	for (ptr = tables + HASH (table->name, TABLE_HASH_SIZE); *ptr;
		 ptr = &((*ptr)->collision))
		if (*ptr == table)
			{
			*ptr = table->collision;
			break;
			}

	// Remove table from id hash table

	for (ptr = tablesModId + table->tableId % TABLE_HASH_SIZE; *ptr;
		 ptr = &((*ptr)->idCollision))
		if (*ptr == table)
			{
			*ptr = table->idCollision;
			break;
			}

	invalidateCompiledStatements(table);
	sync.unlock();
	table->drop(transaction);
	table->expunge(getSystemTransaction());
	delete table;
}

void Database::truncateTable(Table *table, Transaction *transaction)
{
	table->checkDrop();
	
	// Check for records in active transactions

	if (hasUncommittedRecords(table, transaction))
		throw SQLError(UNCOMMITTED_UPDATES, "table %s.%s has uncommitted updates and cannot be truncated",
						table->schemaName, table->name);
						   
	Sync sync(&syncTables, "Database::truncateTable");
	sync.lock(Exclusive);
	
	// No access until truncate complete
	
	Sync syncObj(&table->syncObject, "Database::truncateTable");
	syncObj.lock(Exclusive);
	
	// Keep scavenger out of the way
	
	Sync scavenge(&table->syncScavenge, "Database::truncateTable");
	scavenge.lock(Exclusive);
	
	// Purge records out of committed transactions
	
	transactionManager->truncateTable(table, transaction);
	
	// Recreate data/blob sections and indexes
	
	Sync syncConnection(&syncSysConnection, "Table::truncate");
	syncConnection.lock(Exclusive);
	
	table->truncate(transaction);
	
	commitSystemTransaction();
}

void Database::addTable(Table * table)
{
	Sync sync (&syncTables, "Database::addTable");
	sync.lock (Exclusive);

	if (formatting)
		{
		Table **ptr;
		
		for (ptr = &tableList; *ptr; ptr = &((*ptr)->next))
			;
			
		table->next = *ptr;
		*ptr = table;
		}
	else
		{
		table->next = tableList;
		tableList = table;
		}

	int slot = HASH (table->name, TABLE_HASH_SIZE);
	table->collision = tables [slot];
	tables [slot] = table;

	slot = table->tableId % TABLE_HASH_SIZE;
	table->idCollision = tablesModId [slot];
	tablesModId [slot] = table;

	// Notify folks who track stuff that there's a new table

#ifndef STORAGE_ENGINE
	templateManager->tableAdded (table);
	applications->tableAdded (table);
#endif

#ifdef LICENSE
	licenseManager->tableAdded (table);
#endif

	imageManager->tableAdded (table);
	searchWords->tableAdded (table);
}

void Database::execute(const char * sql)
{
	Statement *statement = createStatement();
	statement->execute (sql);
	statement->close();
}

void Database::shutdown()
{
	Log::log("%d: Falcon shutdown\n", deltaTime);

	if (shuttingDown)
		return;
	
	if (updateCardinality)
		{
		updateCardinality->close();
		updateCardinality = NULL;
		}
		
	shuttingDown = true;
	
	if (systemConnection && 
		systemConnection->transaction && 
		systemConnection->transaction->state == Active)
		systemConnection->commit();
		
	if (repositoryManager)
		repositoryManager->close();

	//flush(0);

	if (scheduler)
		scheduler->shutdown(false);

	if (internalScheduler)
		internalScheduler->shutdown(false);
	
	if (pageWriter)
		pageWriter->shutdown(false);

#ifndef STORAGE_ENGINE
	if (java)
		java->shutdown (false);
#endif

	serialLog->shutdown();
	cache->shutdown();

	if (threads)
		{
		threads->shutdownAll();
		threads->waitForAll();
		}

	tableSpaceManager->shutdown(0);
	dbb->shutdown(0);

	if (serialLog)
		serialLog->close();
}

/***
void Database::deleteSection(int32 sectionId, Transaction *transaction)
{
	dbb->deleteSection (sectionId, TRANSACTION_ID(transaction));
	
	if (transaction)
		transaction->hasUpdates = true;

}
***/

void Database::invalidateCompiledStatements(Table * table)
{
	Sync sync (&syncStatements, "Database::invalidateCompiledStatements");
	sync.lock (Exclusive);

	for (CompiledStatement *statement, **ptr = &compiledStatements; (statement = *ptr);)
		if (statement->references (table))
			{
			statement->invalidate();
			*ptr = statement->next;
			statement->release();
			}
		else
			ptr = &(*ptr)->next;
}


User* Database::createUser(const char * account, const char * password, bool encrypted, Coterie *coterie)
{
	return roleModel->createUser (getSymbol (account), password, encrypted, coterie);
}

User* Database::findUser(const char * account)
{
	return roleModel->findUser (symbolManager->getSymbol (account));
}

Role* Database::findRole(const char * schemaName, const char * roleName)
{
	const char *schema = symbolManager->getSymbol (schemaName);
	const char *role = symbolManager->getSymbol (roleName);
	return roleModel->findRole (schema, role);
}


Role* Database::findRole(const WCString *schemaName, const WCString *roleName)
{
	return roleModel->findRole (symbolManager->getSymbol (schemaName), 
								symbolManager->getSymbol (roleName));
}

void Database::validate(int optionMask)
{
	Sync sync (&syncObject, "Database::validate");
	sync.lock (Exclusive);
	Log::debug ("Validation:\n");
	dbb->validate (optionMask);
	tableSpaceManager->validate(optionMask);
	
	if (optionMask & validateBlobs)
		{
		Sync sync2 (&syncSysConnection, "Database::validate");
		sync2.lock (Shared);
		PreparedStatement *statement = prepareStatement (
			"select tableId,viewDefinition from system.tables");
		ResultSet *resultSet = statement->executeQuery();

		while (resultSet->next())
			if (!resultSet->getString (2)[0])
				{
				Table *table = getTable (resultSet->getInt (1));
				table->validateBlobs (optionMask);
				}

		resultSet->close();
		statement->close();
		}

	Log::debug ("Database::validate: validation complete\n");
}

void Database::scavenge()
{
	updateCardinalities();

	// Start by scavenging compiled statements.  If they're moldy and not in use,
	// off with their heads!

	Sync sync (&syncStatements, "Database::scavenge");
	sync.lock (Exclusive);
	time_t threshold = timestamp - STATEMENT_RETIREMENT_AGE;
	lastScavenge = timestamp;

	for (CompiledStatement *statement, **ptr = &compiledStatements; (statement = *ptr);)
		if (statement->useCount > 1 || statement->lastUse > threshold)
			ptr = &statement->next;
		else
			{
			*ptr = statement->next;
			statement->release();
			}
	
	sync.unlock();

	// Next, scavenge tables

	retireRecords(false);				// age group based scavenger

	// Scavenge expired licenses
	
	DateTime now;
	now.setNow();

#ifdef LICENSE
	licenseManager->scavenge (&now);
#endif
	
#ifndef STORAGE_ENGINE
	sessionManager->scavenge (&now);
#endif
	
	transactionManager->reportStatistics();

	if (serialLog)
		serialLog->reportStatistics();
		
	dbb->reportStatistics();
	tableSpaceManager->reportStatistics();
	repositoryManager->reportStatistics();
}


void Database::retireRecords(bool forced)
{
	int cycle = scavengeCycle;
	Sync lock(&syncScavenge, "Database::retireRecords");
	lock.lock(Exclusive);
	
	if (forced && scavengeCycle > cycle)
		return;
	
	++scavengeCycle;
	
	if (forced)
		Log::log("Forced record scavenge cycle\n");
	
	if (!forced && systemConnection->transaction)
		commitSystemTransaction();
		
	transactionManager->purgeTransactions();
	TransId oldestActiveTransaction = transactionManager->findOldestActive();
	int threshold = 0;
	uint64 total = recordDataPool->activeMemory;
	RecordScavenge recordScavenge(this, oldestActiveTransaction);
	
	// If we passed the upper limit, scavenge.  If we didn't pick up
	// a significant amount of memory since the last cycle, don't bother
	// bumping the age group.

	if (forced || total >  recordScavengeThreshold)
		{
		//LogStream stream;
		//recordDataPool->analyze(0, &stream, NULL, NULL);
		Sync syncTbl (&syncTables, "Database::retireRecords");
		syncTbl.lock (Shared);
		Table *table;
		time_t scavengeStart = deltaTime;
		
		for (table = tableList; table; table = table->next)
			table->inventoryRecords(&recordScavenge);
		
		threshold = recordScavenge.computeThreshold(recordScavengeFloor);
		recordScavenge.printRecordMemory();	
		int count = 0;
		int skipped = 0;
		
		for (table = tableList; table; table = table->next)
			{
			try
				{
				int n = table->retireRecords(&recordScavenge);
				
				if (n >= 0)
					count += n;
				else
					++skipped;
				}
			catch (SQLException &exception)
				{
				//syncTbl.unlock();
				Log::debug ("Exception during scavenger of table %s.%s: %s\n",
						table->schemaName, table->name, exception.getText());
				}
			}

		syncTbl.unlock();
		Log::log(LogScavenge, "%d: Scavenged %d records, " I64FORMAT " bytes in %d seconds\n", 
					deltaTime, recordScavenge.recordsReclaimed, recordScavenge.spaceReclaimed, deltaTime - scavengeStart);
			
		total = recordScavenge.spaceRemaining;
		}
	else if ((total - lastRecordMemory) < recordScavengeThreshold / AGE_GROUPS)
		{
		recordScavenge.scavengeGeneration = -1;
		cleanupRecords (&recordScavenge);
		
		return;
		}
	else
		{
		recordScavenge.scavengeGeneration = -1;
		cleanupRecords (&recordScavenge);
		}

	lastRecordMemory = recordDataPool->activeMemory;
	INTERLOCKED_INCREMENT (currentGeneration);
}

void Database::ticker(void * database)
{
	((Database*) database)->ticker();
}

void Database::ticker()
{
	Thread *thread = Thread::getThread("Database::ticker");

	while (!thread->shutdownInProgress)
		{
		timestamp = time(NULL);
		deltaTime = timestamp - startTime;
		thread->sleep(1000);

#ifdef STORAGE_ENGINE
		if (falcon_debug_trace)
			debugTrace();
#endif
		}
}

int Database::createSequence(int64 initialValue)
{
	Transaction *transaction = systemConnection->getTransaction();

	return dbb->createSequence (initialValue, TRANSACTION_ID(transaction));
}

int64 Database::updateSequence(int sequenceId, int64 delta, Transaction *transaction)
{
	return dbb->updateSequence (sequenceId, delta, TRANSACTION_ID(transaction));
}


Transaction* Database::getSystemTransaction()
{
	return systemConnection->getTransaction();
}


void Database::rebuildIndexes()
{
	Transaction *transaction = getSystemTransaction();

	for (Table *table = tableList; table; table = table->next)
		table->reIndex(transaction);

	commitSystemTransaction();
}

const char* Database::getSymbol(const char *string)
{
	return symbolManager->getSymbol (string);
}

bool Database::isSymbol(const char *string)
{
	return symbolManager->isSymbol (string);
}

const char* Database::getSymbol(const WCString *string)
{
	return symbolManager->getSymbol (string);
}

const char* Database::getString(const char *string)
{
	return symbolManager->getString (string);
}

void Database::upgradeSystemTables()
{
	for (const char **tableName = changedTables; *tableName; ++tableName)
		{
		Table *table = findTable("SYSTEM", *tableName);
		table->refreshFields();
		}

	Table *table = findTable ("SYSTEM", "SCHEMAS");

	if (!table)
		{
		Statement *statement = createStatement();
		
		for (const char **sql = ods2UpgradeStatements; *sql; ++sql)
			statement->execute (*sql);
			
		statement->close();
		}

	table = findTable ("SYSTEM", "FIELDS");
	table->loadIndexes();
	
	if (!table->findField ("PRECISION"))
		{
		Statement *statement = createStatement();
		
		for (const char **sql = ods3Upgrade; *sql; ++sql)
			statement->execute (*sql);
			
		statement->close();
		}

	Table *tables = findTable("SYSTEM", "TABLES");
	
	if (!tables->findField ("CARDINALITY"))
		{
		Statement *statement = createStatement();
		
		for (const char **sql = ods3aUpgrade; *sql; ++sql)
			statement->execute (*sql);
			
		statement->close();
		}

	if (!tables->findField ("TABLESPACE"))
		{
		Statement *statement = createStatement();
		
		for (const char **sql = ods3bUpgrade; *sql; ++sql)
			statement->execute (*sql);
			
		statement->close();
		tables->refreshFields();
		}

	table = findTable("SYSTEM", "TABLESPACES");

	if (table && table->dataSectionId != dbb->tableSpaceSectionId)
		dbb->updateTableSpaceSection(table->dataSectionId);
	
	fieldExtensions = true;
}

JString Database::analyze(int mask)
{
	Stream stream;
	stream.setMalloc (true);
	Sync syncSystemTransaction(&syncSysConnection, "Database::analyze");

	if (mask & analyzeMemory)
		MemMgrAnalyze (mask, &stream);

#ifndef STORAGE_ENGINE
	if (mask & analyzeMemory)
		java->analyzeMemory (mask, &stream);

	if (mask & analyzeClasses)
		{
		stream.putSegment ("\nClasses\n");
		java->analyze (mask, &stream);
		stream.putCharacter ('\n');
		}

	if (mask & analyzeObjects)
		{
		stream.putSegment ("\nObject allocations\n");
		java->analyzeObjects (mask, &stream);
		stream.putCharacter ('\n');
		}
#endif

	if (mask & analyzeRecords)
		{
		stream.putSegment ("\nRecords\n");
		
		for (Table *table = tableList; table; table = table->next)
			{
			int count = table->countActiveRecords();
			
			if (count)
				stream.format ("%s.%s\t%d\n", table->schemaName, table->name, count);
			}
			
		stream.putCharacter ('\n');
		}

	if (mask & analyzeStatements)
		{
		stream.putSegment ("\nStatements\n");
		Sync sync (&syncStatements, "Database::analyze");
		sync.lock (Shared);

		for (CompiledStatement *statement = compiledStatements; statement;
			 statement = statement->next)
			{
			stream.putSegment (statement->sqlString);
			stream.format ("\t(%d)\n", statement->countInstances());
			}
		stream.putCharacter ('\n');
		sync.unlock();
		}

	if (mask & analyzeTables)
		{
		syncSystemTransaction.lock(Shared);
		PreparedStatement *statement = prepareStatement (
			"select schema,tableName,dataSection,blobSection,tablespace from tables order by schema, tableName");
		PreparedStatement *indexQuery = prepareStatement(
			"select indexName, indexId from system.indexes where schema = ? and tableName = ?");
		ResultSet *resultSet = statement->executeQuery();

		while (resultSet->next())
			{
			int n = 1;
			const char *schema = resultSet->getString (n++);
			const char *tableName = resultSet->getString (n++);
			int dataSection = resultSet->getInt (n++);
			int blobSection = resultSet->getInt (n++);
			const char *tableSpaceName = resultSet->getString (n++);
			TableSpace *tableSpace = NULL;

			if (tableSpaceName[0])
				tableSpace = tableSpaceManager->findTableSpace(tableSpaceName);
			
			Dbb *tableDbb = (tableSpace) ? tableSpace->dbb : dbb;
			stream.format ("Table %s.%s\n", schema, tableName);
			tableDbb->analyzeSection (dataSection, "Data section", 3, &stream);
			tableDbb->analyzeSection (blobSection, "Blob section", 3, &stream);
			
			indexQuery->setString(1, schema);
			indexQuery->setString(2, tableName);
			ResultSet *indexes = indexQuery->executeQuery();
			
			while (indexes->next())
				{
				const char *indexName = indexes->getString(1);
				int combinedId = indexes->getInt(2);
				int indexId = INDEX_ID(combinedId);
				int indexVersion = INDEX_VERSION(combinedId);
				tableDbb->analyseIndex(indexId, indexVersion, indexName, 3, &stream);
				}
			
			indexes->close();
			}

		statement->close();
		indexQuery->close();
		}

	if (mask & analyzeSpace)
		dbb->analyzeSpace(0, &stream);
			
	if (mask & analyzeCache)
		dbb->cache->analyze (&stream);

	if (mask & analyzeSync)
		SyncObject::analyze(&stream);

	return stream.getJString();
}

int Database::getMemorySize(const char *string)
{
	int n = 0;

	for (const char *p = string; *p;)
		{
		char c = *p++;
		if (c >= '0' && c <= '9')
			n = n * 10 + c - '0';
		else if (c == 'm' || c == 'M')
			n *= 1000000;
		else if (c == 'k' || c == 'K')
			n *= 1000;
		}

	return n;
}


void Database::cleanupRecords(RecordScavenge *recordScavenge)
{
	Sync sync (&syncTables, "Database::cleanupRecords");
	sync.lock (Shared);

	for (Table *table = tableList; table; table = table->next)
		{
		try
			{
			table->cleanupRecords(recordScavenge);
			}
		catch (SQLException &exception)
			{
			Log::debug ("Exception during cleanupRecords of table %s.%s: %s\n",
					table->schemaName, table->name, exception.getText());
			}
		}
}

void Database::licenseCheck()
{
#ifdef LICENSE
	licensed = false;
	LicenseProduct *product = licenseManager->getProduct (SERVER_PRODUCT);

	if (!(licensed = product->isLicensed()))
		{
		DateTime expiration = DateTime::convert (BUILD_DATE);
		expiration.add (EXPIRATION_DAYS * 24 * 60 * 60);
		DateTime now;
		now.setNow();
		if (now.after (expiration))
			throw SQLError (LICENSE_ERROR, "Unlicensed server usage period has expired");
		}	
#endif
}

Repository* Database::findRepository(const char *schema, const char *name)
{
	return repositoryManager->findRepository (schema, name);
}

Repository* Database::getRepository(const char *schema, const char *name)
{
	return repositoryManager->getRepository (schema, name);
}

Repository* Database::createRepository(const char *name, const char *schema, Sequence *sequence, const char *fileName, int volume, const char *rolloverString)
{
	return repositoryManager->createRepository (name, schema, sequence, fileName, volume, rolloverString);
}

Schema* Database::getSchema(const char *name)
{
	int slot = HASH (name, TABLE_HASH_SIZE);
	Schema *schema;

	for (schema = schemas [slot]; schema; schema = schema->collision)
		if (schema->name == name)
			return schema;

	schema = new Schema (this, name);
	schema->collision = schemas [slot];
	schemas [slot] = schema;

	return schema;
}

void Database::deleteRepository(Repository *repository)
{
	repositoryManager->deleteRepository (repository);
}

void Database::deleteRepositoryBlob(const char *schema, const char *repositoryName, int volume, int64 blobId, Transaction *transaction)
{
	Repository *repository = getRepository (getSymbol (schema), getSymbol (repositoryName));
	repository->deleteBlob (volume, blobId, transaction);	
}

void Database::commit(Transaction *transaction)
{
	dbb->commit(transaction);
}

void Database::rollback(Transaction *transaction)
{
	dbb->rollback(transaction->transactionId, transaction->hasUpdates);
}

void Database::renameTable(Table* table, const char* newSchema, const char* newName)
{
	newSchema = getSymbol(newSchema);
	newName = getSymbol(newName);
	Sync sync (&syncTables, "Database::renameTable");
	sync.lock (Exclusive);
	roleModel->renameTable(table, newSchema, newName);

	// Remove table from name hash table

	for (Table **ptr = tables + HASH (table->name, TABLE_HASH_SIZE); *ptr;
		 ptr = &((*ptr)->collision))
		if (*ptr == table)
			{
			*ptr = table->collision;
			break;
			}

	// Add table back to name hash table

	table->name = newName;
	table->schemaName = newSchema;
	int slot = HASH (table->name, TABLE_HASH_SIZE);
	table->collision = tables[slot];
	tables[slot] = table;

	invalidateCompiledStatements(table);
}

void Database::dropDatabase()
{
	shutdown();
	
	if (serialLog)
		serialLog->dropDatabase();

	tableSpaceManager->dropDatabase();
	dbb->dropDatabase();
}

void Database::shutdownNow()
{
	panicShutdown = true;

	if (cache)
		cache->shutdownNow();

	if (serialLog)
		serialLog->shutdownNow();
}

void Database::validateCache(void)
{
	dbb->validateCache();
}

bool Database::hasUncommittedRecords(Table* table, Transaction *transaction)
{
	return transactionManager->hasUncommittedRecords(table, transaction);
}

void Database::commitByXid(int xidLength, const UCHAR* xid)
{
	serialLog->commitByXid(xidLength, xid);
	transactionManager->commitByXid(xidLength, xid);
}

void Database::rollbackByXid(int xidLength, const UCHAR* xid)
{
	serialLog->rollbackByXid(xidLength, xid);
	transactionManager->rollbackByXid(xidLength, xid);
}

int Database::getMaxKeyLength(void)
{
	switch(dbb->pageSize)
		{
		case  1024:  return MAX_INDEX_KEY_LENGTH_1K;
		case  2048:  return MAX_INDEX_KEY_LENGTH_2K;
		case  4096:  return MAX_INDEX_KEY_LENGTH_4K;
		case  8192:  return MAX_INDEX_KEY_LENGTH_8K;
		case 16384:  return MAX_INDEX_KEY_LENGTH_16K;
		case 32768:  return MAX_INDEX_KEY_LENGTH_32K;
	}

	// Any other page size is programatically unlikely (it would be a bug).

	return MAX_INDEX_KEY_LENGTH_4K;  // Default value.
}

void Database::getIOInfo(InfoTable* infoTable)
{
	int n = 0;
	infoTable->putString(n++, name);
	infoTable->putInt(n++, dbb->pageSize);
	infoTable->putInt(n++, dbb->cache->numberBuffers);
	infoTable->putInt(n++, dbb->reads);
	infoTable->putInt(n++, dbb->writes);
	infoTable->putInt(n++, dbb->fetches);
	infoTable->putInt(n++, dbb->fakes);
	infoTable->putRecord();
}

void Database::getTransactionInfo(InfoTable* infoTable)
{
	transactionManager->getTransactionInfo(infoTable);
}

void Database::getSerialLogInfo(InfoTable* infoTable)
{
	serialLog->getSerialLogInfo(infoTable);
}

void Database::getTransactionSummaryInfo(InfoTable* infoTable)
{
	transactionManager->getSummaryInfo(infoTable);
}

void Database::updateCardinalities(void)
{
	Sync syncSystemTransaction(&syncSysConnection, "Database::updateCardinalities");
	syncSystemTransaction.lock(Shared);
	Sync sync (&syncTables, "Database::updateCardinalities");
	sync.lock (Shared);
	bool hit = false;
	
	try
		{
		for (Table *table = tableList; table; table = table->next)
			{
			uint64 cardinality = table->cardinality;
			
			if (cardinality != table->priorCardinality)
				{
				if (!hit)
					{
					if (!updateCardinality)
						updateCardinality = prepareStatement(
							"update system.tables set cardinality=? where schema=? and tablename=?");
							
					hit = true;
					}
				
				updateCardinality->setLong(1, cardinality);
				updateCardinality->setString(2, table->schemaName);
				updateCardinality->setString(3, table->name);
				updateCardinality->executeUpdate();
				table->priorCardinality = cardinality;
				}
			}
		}
	catch (...)
		{
		}
	
	sync.unlock();
	syncSystemTransaction.unlock();
	commitSystemTransaction();
}

void Database::sync()
{
	cache->syncFile(dbb, "sync");
	tableSpaceManager->sync();
}

void Database::setIOError(SQLException* exception)
{
	Sync sync(&syncObject, "Database::setIOError");
	sync.lock(Exclusive);
	++pendingIOErrors;
	ioError = exception->getText();
	pendingIOErrorCode = exception->getSqlcode();
}

void Database::clearIOError(void)
{
	Sync sync(&syncObject, "Database::clearIOError");
	sync.lock(Exclusive);
	--pendingIOErrors;
}

void Database::preUpdate()
{
	if (pendingIOErrors)
		throw SQLError(pendingIOErrorCode, "Pending I/O error: %s", (const char*) ioError);
		
	serialLog->preUpdate();
}

void Database::setRecordMemoryMax(uint64 value)
{
	if (configuration)
		{
		configuration->setRecordMemoryMax(value);
		recordMemoryMax	= configuration->recordMemoryMax;
		recordScavengeThreshold = configuration->recordScavengeThreshold;
		recordScavengeFloor	= configuration->recordScavengeFloor;
		}
}

void Database::setRecordScavengeThreshold(int value)
{
	if (configuration)
		{
		configuration->setRecordScavengeThreshold(value);
		recordScavengeThreshold = configuration->recordScavengeThreshold;
		recordScavengeFloor = configuration->recordScavengeFloor;
		}
}

void Database::setRecordScavengeFloor(int value)
{
	if (configuration)
		{
		configuration->setRecordScavengeFloor(value);
		recordScavengeFloor = configuration->recordScavengeFloor;
		}
}

void Database::forceRecordScavenge(void)
{
	retireRecords(true);
}

void Database::debugTrace(void)
{
#ifdef STORAGE_ENGINE
	if (falcon_debug_trace & FALC0N_TRACE_TRANSACTIONS)
		transactionManager->printBlockage();

	if (falcon_debug_trace & FALC0N_SYNC_TEST)
		{
		SyncTest syncTest;
		syncTest.test();
		}
	
	if (falcon_debug_trace & FALC0N_SYNC_OBJECTS)
		SyncObject::dump();
	
	if (falcon_debug_trace & FALC0N_REPORT_WRITES)
		tableSpaceManager->reportWrites();
	
	if (falcon_debug_trace & FALC0N_FREEZE)
		Synchronize::freezeSystem();
	
	falcon_debug_trace = 0;
#endif
}

void Database::pageCacheFlushed(int64 flushArg)
{
	serialLog->pageCacheFlushed(flushArg);
}

JString Database::setLogRoot(const char *defaultPath, bool create)
{
	bool error = false;
	char fullDefaultPath[PATH_MAX];
	const char *baseName;
	JString userRoot;

	// Construct a fully qualified path for the default serial log location.
	
	const char *p = strrchr(defaultPath, '.');
	JString  defaultRoot = (p) ? JString(defaultPath, (int)(p - defaultPath)) : name;

	dbb->expandFileName((const char*)defaultRoot, sizeof(fullDefaultPath), fullDefaultPath, &baseName); 
	
	// If defined, serialLogDir is a valid, fully qualified path. Verify that
	// it is also a valid location for the serial log. Otherwise, use the default.
	
	if (!configuration->serialLogDir.IsEmpty())
		{
		int errnum;
		
		userRoot = configuration->serialLogDir + baseName;

		if (dbb->fileStat(JString(userRoot+".fl1"), NULL, &errnum) != 0)
			{		
			switch (errnum)
				{
				case 0:		 // file exists, don't care if create == true
					break;
				
				case ENOENT: // no file, but !create means file expected
					error = !create;
					break;
					
				default:	 // invalid path or other error
					error = true;
					break;
				}
			}
		}
		
	if (!userRoot.IsEmpty() && !error)
		return userRoot.getString();
	else
		return defaultRoot.getString();
}
