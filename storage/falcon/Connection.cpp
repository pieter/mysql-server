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

// Connection.cpp: implementation of the Connection class.
//
//////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "Engine.h"
#include "Connection.h"
#include "Database.h"
#include "Transaction.h"
#include "SQLError.h"
#include "PreparedStatement.h"
#include "Registry.h"
#include "Stream.h"
#include "DatabaseMetaData.h"
#include "SessionManager.h"
#include "Session.h"
#include "Privilege.h"
#include "User.h"
#include "UserRole.h"
#include "RoleModel.h"
#include "Image.h"
#include "Thread.h"
#include "Parameters.h"
#include "AsciiBlob.h"
#include "SequenceManager.h"
#include "Sequence.h"
#include "Sync.h"
#include "Scheduler.h"
#include "FilterSet.h"
#include "Log.h"
#include "ResultSet.h"
#include "Interlock.h"
#include "FilterSetManager.h"
#include "Agent.h"
#include "Configuration.h"
#include "Server.h"
#include "IOx.h"

#ifndef STORAGE_ENGINE
#include "DataResourceLocator.h"
#include "Application.h"
#include "JavaVM.h"
#include "Java.h"
#include "JavaConnection.h"
#include "QueryString.h"
#include "TemplateContext.h"
#include "JavaGenNative.h"
#endif 

#ifdef LICENSE
#include "License.h"
#include "LicenseManager.h"
#include "LicenseProduct.h"
#include "LicenseToken.h"
#endif

#include "StringTransform.h"
#include "EncryptTransform.h"
#include "SHATransform.h"
#include "Base64Transform.h"

//#define FORCE_ALIASES
#define COOKIE_NAME	"NETFRASTRUCTURE"
#define UTF8		false

static Mutex		shutdownMutex;
//static LinkedList	databases;
static Database		*firstDatabase;
static Database		*lastDatabase;
static Server		*server;
static bool			panicShutdown;

static Registry		registry;
static SyncObject	databaseList;

static const char *ddl [] = {
	"grant all on system.sequences to %s",
	"grant select on system.privileges to public",
	"grant select on system.sequences to public",
	"grant all on system.images to %s",
#ifndef STORAGE_ENGINE
	"grant all on system.applications to %s",
	"grant select on system.applications to public",
	"grant all on system.java_classes to %s",
	"grant all on system.java_manifests to %s",
#endif
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Connection::Connection(Configuration *config)
{
	init(config);
}

Connection::Connection(Database * db)
{
	init(db->configuration);
	database = db;
	database->addRef();
	user = database->roleModel->getSystemUser();
}


Connection::Connection(Database *db, User *usr)
{
	init(db->configuration);
	database = db;
	database->addRef();

	if (usr)
		setUser (usr);
}

void Connection::init(Configuration *config)
{
	configuration = config;
	configuration->addRef();
	database = NULL;
	transaction = NULL;
	metaData = NULL;
	useCount = 1;
	user = NULL;
	resultSets = NULL;
	statements = NULL;
	licenseToken = NULL;
	autoCommit = false;
	javaConnections = NULL;
	numberResultSets = 0;
	traceFlags = 0;
	logFile = NULL;
	licenseNotRequired = false;
	configFile = NULL;
	activeDebugger = false;
	maxSort = -1;
	statementMaxSort = -1;
	maxRecords = -1;
	statementMaxRecords = -1;
	sortCount = 0;
	recordCount = 0;
	registeredStatements = NULL;
	nextHandle = 0;
	isolationLevel = TRANSACTION_CONSISTENT_READ;
	mySqlThreadId = 0;
	currentStatement = NULL;
}

Connection::~Connection()
{
	if (logFile)
		fclose ((FILE*) logFile);

	if (database)
		detachDatabase();

	configuration->release();
}


Connection* createLocalConnection()
{
	Configuration *configuration = new Configuration(NULL);
	Connection *connection = new Connection(configuration);
	configuration->release();

	return connection;
}

void addLogListener (int mask, Listener *logListener, void *arg)
{
	Log::addListener (mask, logListener, arg);
}

void deleteLogListener (Listener *logListener, void *arg)
{
	Log::deleteListener (logListener, arg);
}

void generateDigest (const char *string, char *digest)
{
	//strcpy (digest, Digest::computeDigest (string));
	EncryptTransform<StringTransform,SHATransform,Base64Transform>
		encrypt(string, 0);
	encrypt.get(32, (UCHAR*) digest);
}

Statement* Connection::createStatement()
{
	if (!database)
		return NULL;

	requiresLicense();
	Statement *statement = database->createStatement (this);
	addStatement (statement);

	return statement;
}

PreparedStatement* Connection::prepareStatement(const char * sqlString)
{
	if (!database)
		return NULL;

	requiresLicense();
	PreparedStatement *statement = database->prepareStatement (this, sqlString);
	addStatement ((Statement*) statement);

	return statement;
}


PreparedStatement* Connection::prepareStatement(const WCString *sqlString)
{
	if (!database)
		return NULL;

	requiresLicense();
	PreparedStatement *statement = database->prepareStatement (this, sqlString);
	addStatement ((Statement*) statement);

	return statement;
}

Transaction* Connection::getTransaction()
{
	if (!transaction)
		transaction = database->startTransaction(this);

	return transaction;
}

void Connection::commit()
{
	Transaction *trans;

	for (int n = 0; (trans = transaction); ++n)
		{
		if (n > 5)
			throw SQLEXCEPTION (RUNTIME_ERROR, "non-converging transaction");
			
		transaction = NULL;
		ASSERT (trans->database == database);
		trans->commit();
		transactionEnded();
		}

}

void Connection::rollback()
{
	if (transaction)
		{
		transaction->rollback();
		transaction = NULL;
		transactionEnded();
		}

}

void Connection::transactionEnded()
{
	if (resultSets || statements)
		{
		Sync sync (&syncResultSets, "Connection::transactionEnded");
		sync.lock (Shared);

		for (ResultSet *resultSet = resultSets; resultSet; resultSet = resultSet->connectionNext)
			resultSet->transactionEnded();
		
		for (Statement *statement = statements; statement; statement = statement->connectionNext)
			statement->transactionEnded();
		}
}

void Connection::prepare(int xidSize, const UCHAR *xid)
{
	if (transaction)
		transaction->prepare(xidSize, xid);
}

void Connection::close()
{
	release();
}

void Connection::deleteStatement(Statement * statement)
{
	Sync sync (&syncObject, "Connection::deleteStatement");
	sync.lock (Exclusive);

	for (Statement **ptr = &statements; *ptr; ptr = &(*ptr)->connectionNext)
		if (*ptr == statement)
			{
			*ptr = statement->connectionNext;
			break;
			}
}

void Connection::shutdown()
{
#ifndef STORAGE_ENGINE
	if (server)
		{
		try
			{
			server->addRef();
			Thread *thread = new Thread("Connection::shutdown", NULL); //shutdown, this, NULL);
			thread->createThread(shutdown, NULL);
			thread->release();
			}
		catch (...)
			{
			shutdownDatabases();
			}
		}
#endif

	//printf ("Connection::shutdown(1) %x useCount %d\n", thread, thread->useCount);
}

void Connection::shutdown(void *connection)
{
	Thread *thread = Thread::getThread ("Connection::shutdown");
	thread->addRef();
	//printf ("Connection::shutdown(2) %x useCount %d\n", thread, thread->useCount);
	((Connection*) connection)->shutdownDatabases();
	thread->shutdown();
}

void Connection::shutdownDatabases()
{
#ifndef STORAGE_ENGINE
	Server *srv = server;

	if (srv)
		{
		server = NULL;
		srv->shutdown (false);
		srv->release();
		}
#endif

	Sync sync(&shutdownMutex, "Connection::shutdownDatabases");
	sync.lock(Exclusive);

	for (Database *database; (database = firstDatabase);)
		{
		unlink(database);
		database->shutdown();
		database->shutdown();
		database->release();
		}
}

Statement* Connection::findStatement(const char * cursorName)
{
	Sync sync (&syncObject, "Connection::findStatement");
	sync.lock (Shared);

	for (Statement *statement = statements; statement; statement = statement->connectionNext)
		if (statement->cursorName == cursorName)
			return statement;
	
	return NULL;	
}


Clob* Connection::genHTML(TemplateContext *context, int32 genHeaders)
{
	AsciiBlob *headers = new AsciiBlob (1000);

#ifndef STORAGE_ENGINE

	//context->dump();
	Stream stream (1000);
	context->setConnection (this);
	JString setCookie;
	const char *qs = context->getValue ("QUERY_STRING");
	QueryString *queryString = new QueryString (qs);
	SessionManager *sessionManager = database->sessionManager;

	context->setContentType ("text/html; charset=utf-8");
	context->headersStream = headers;
	Session *session = NULL;
	Session *newSession = NULL;
	const char *cookie = NULL;
	const char *applicationName = NULL;
	char temp [128];
	Application *application = NULL;
	Thread *thread = Thread::getThread("Connection::genHTML");
	thread->setTimeZone (NULL);

	//logFile = fopen ("session.log", "w");

	try
		{
		context->queryString = queryString;

		// Find application

		applicationName = queryString->getParameter (PARM_APPLICATION, NULL);
		const char *defaultApplication = context->getValue ("DEFAULT_APPLICATION", "UnknownApplication");

		if (!applicationName)
			applicationName = defaultApplication;

		application = database->getApplication (applicationName);
		if (!application)
			throw SQLEXCEPTION (RUNTIME_ERROR, "application '%s' is unknown",
									(const char*) applicationName);
		application->addRef();

		if (defaultApplication)
			context->defaultApplication = application->name == defaultApplication;

		if (!queryString->queryString [0])
			{
			const char *alias = context->getValue ("alias", NULL);
			if (alias)
				{
				const char *p = application->findQueryString (this, alias);
				if (p)
					queryString->setString (p);
				}
			}

		application->pushNameSpace (this);

#ifdef FORCE_ALIASES
		if (application->name == defaultApplication)
			context->enableAliases();
#endif

		// Find session (which may or may not exist)

		const char *httpCookie = context->getValue ("HTTP_COOKIE");

		/*** debugging code
		Log::log ("Forcing special agent\n");
		session = sessionManager->getSpecialSession (application, context);
		session->addRef();
		context->enableAliases();
		***/

		//if (!httpCookie)
			{
			const char *agentName = context->getValue ("HTTP_USER_AGENT");
			if (agentName)
				{
				Agent *agent = application->getAgent (agentName);
				if (agent && agent->isSpecialAgent())
					{
					Log::log ("Enabling special agent: %s\n", agentName);
					session = sessionManager->getSpecialSession (application, context);
					session->addRef();
					context->enableAliases();
					}
				}
			}
		
		if (!session)		
			if (session = sessionManager->findSession (application, context, httpCookie))
				session->addRef();

		// See if we can find the client from the cookie

		const char *clientId = NULL;

		if (httpCookie)
			{
			if (cookie = getCookie (httpCookie, COOKIE_NAME, temp, sizeof (temp)))
				{
				clientId = cookie;
				context->putValue ("cookie", clientId);
				}
			}

		// If we have both client and session, make sure they match!

		if (session)
			if (session->zapped)
				{
				session->release();
				session = NULL;
				}

		if (!session)
			session = newSession = sessionManager->createSession (application);

		// Try to active session without waiting.  If a license token
		// isn't immediately available, (do something) and wait for one.

		if (!session->activate (false))
			session->activate (true);

#ifdef LICENSE
		if (!licenseToken)
			if (licenseToken = session->licenseToken)
				licenseToken->addRef();
#endif

		context->session = session;
		context->scriptName = context->getValue ("script_name", "");
		context->push ("sessionId", session->sessionId);
		context->putValue ("application", application->name);

		context->application = application;
		context->stream = &stream;
		context->stepNumber = database->stepNumber++;

#ifdef LICENSE
		if (licenseToken)
			sessionManager->execute (this, application, context, &stream);
		else
			database->genHTML (NULL, application->name, "licenseExceeded", context, &stream, NULL);
#else
		sessionManager->execute (this, application, context, &stream);
#endif

		ASSERT (thread->activeLocks == 0);
		}
	catch (SQLException &exception)
		{
		stream.putSegment ("<h3>Unexpected Program Exception:</h3><br><b><pre>");
		const char *text = exception.getText();
		stream.putSegment (text);
		stream.putSegment ("</pre></b>");
		const char *stackTrace = exception.getTrace();
		if (stackTrace && stackTrace [0])
			{
			Log::debug ("Unexpected program exception: %s\n", text);
			Log::debug ("Stack trace from %s\n%s\n", qs, stackTrace);
			}
		rollback();
		}

	// If any images are referenced, make up a dummy header

	if (genHeaders)
		{
		if (!images.isEmpty())
			{
			const char *sep = "images: ";
			FOR_OBJECTS (Image*, image, &images)
				headers->putSegment (sep);
				headers->putSegment (image->alias);
				sep = ",";
				image->release();
			END_FOR;
			headers->putSegment ("\r\n");
			}
		if (!cookie && session)
			{
			// get expiration date; 10 years is good enough
			int32 date = (int32) (DateTime::getNow() + 10 * 365 * 24 * 60 * 60);
			char expiration [100];
			formatDateString ("%a %d-%b-%y %H:%M:%S GMT", sizeof (expiration), date, expiration);
			setCookie.Format ("set-cookie: %s=%s; expires=%s;\r\n", 
							  COOKIE_NAME, 
							  (const char*) session->sessionId,
							  expiration);
			headers->putSegment (setCookie);
			}
		if (newSession)
			{
			// get expiration date; 1 day is good enough
			int32 date = (int32) (DateTime::getNow() + 24 * 60 * 60);
			char expiration [100];
			formatDateString ("%a %d-%b-%y %H:%M:%S GMT", sizeof (expiration), date, expiration);
			const char *scriptName = context->getValue ("SCRIPT_NAME", "/");
			char *p = strrchr (scriptName, '/');
			if (p)
				{
				setCookie.Format ("set-cookie: %s=%s; expires=%s; path=%.*s\r\n", 
								  applicationName, 
								  (const char*) session->sessionId,
								  expiration,
								  p + 1 - scriptName, scriptName);
				headers->putSegment (setCookie);
				}
			}
		headers->putSegment ("Content-type: ");
		headers->putSegment (context->contentType);
		headers->putSegment ("\r\n\r\n");
		}
				
	if (stream.totalLength == 0)
		stream.putSegment ("<html><body>Well, almost nothing got generated.  Sorry.</body></html>");

	headers->transfer(&stream);
	images.clear();

	if (session)
		{
		session->deactivate();
		session->release();
		}

	if (application)
		{
		application->checkout (this);
		application->release();
		}

	queryString->release();
	context->setConnection (NULL);
#endif

	return headers;
}

void Connection::genNativeMethod(const char * className, const char * fileName, GenOption option)
{
	//JavaGenNative::genNativeMethod (database->java, className, fileName, option);
}

void Connection::pushSchemaName(const char * name)
{
	const char *string = database->getSymbol (name);
	nameSpace.push ((void*) string);
}

void Connection::popSchemaName()
{
	if (!nameSpace.isEmpty())
		nameSpace.pop();
}

const char *Connection::currentSchemaName()
{
	return (const char*) nameSpace.peek();
}

DatabaseMetaData* Connection::getMetaData()
{
	if (!metaData)
		metaData = new DatabaseMetaData (this);

	return metaData;
}

void Connection::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Connection::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		closeConnection();
}

void Connection::closeConnection()
{
#ifndef STORAGE_ENGINE
	if (activeDebugger)
		database->detachDebugger();
#endif

	for (RegisteredStatement *reg; (reg = registeredStatements);)
		{
		reg->statement->close();
		registeredStatements = reg->next;
		delete reg;
		}

	clearResultSets();
	delete this;
}

char* Connection::getCookie(const char * cookie, const char * name, char *temp, int length)
{
	char *end = temp + length - 1;

	for (const char *p = cookie; *p;)
		{
		while (*p == ' ' || *p == '\t')
			++p;
		const char *q;
		for (q = name; UPPER (*p) == *q; ++p, ++q)
			;
		while (*p == ' ' || *p == '\t')
			++p;
		if (*q == 0 && *p == '=')
			{
			++p;
			while (*p == ' ' || *p == '\t')
				++p;
			char *t;
			for (t = temp; t < end && *p && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n';)
				*t++ = *p++;
			*t = 0;			
			return temp;
			}
		while (*p && *p++ != ';')
			;
		}

	return NULL;
}

void Connection::validatePrivilege(PrivilegeObject* object, PrivType priv)
{
	int32 mask = PRIV_MASK (priv);

	if (user && (user->getPrivileges (object) & mask))
		return;
	
	FOR_OBJECTS (Role*, role, &activeRoles)	
		if (role->getPrivileges (object) & mask)
			return;
	END_FOR;

	throw SQLEXCEPTION (SECURITY_ERROR, "requested access denied");
}

void Connection::assumeRoles()
{
	previousRoles.push (NULL);

	FOR_OBJECTS (Role*, role, &activeRoles)
		previousRoles.push (role);
	END_FOR;

	activeRoles.clear();
}

void Connection::addRole(Role * role)
{
	if (activeRoles.appendUnique (role))
		role->addRef();
}

void Connection::revert()
{
	releaseActiveRoles();

	while (!previousRoles.isEmpty())
		{
		Role *role = (Role*) previousRoles.pop();
		if (!role)
			break;
		activeRoles.append (role);
		}
}

void Connection::addResultSet(ResultSet * resultSet)
{
	Sync sync (&syncResultSets, "Connection::addResultSet");
	sync.lock (Exclusive);

	resultSet->connectionNext = resultSets;
	resultSets = resultSet;
	++numberResultSets;
	resultSet->handle = getNextHandle();
}

void Connection::deleteResultSet(ResultSet * resultSet)
{
	Sync sync (&syncResultSets, "Connection::deleteResultSet");
	sync.lock (Exclusive);

	for (ResultSet **ptr = &resultSets; *ptr; ptr = &(*ptr)->connectionNext)
		if (*ptr == resultSet)
			{
			*ptr = resultSet->connectionNext;
			--numberResultSets;
			return;
			}

	ASSERT (false);
}

void Connection::addImage(Image * image)
{
	image->addRef();

	if (!images.appendUnique (image))
		image->release();
}

Role* Connection::findRole(const char *schemaName, const char * roleName)
{
	return database->findRole (schemaName, roleName);
}


Role* Connection::findRole(const WCString *schemaName, const WCString *roleName)
{
	return database->findRole (schemaName, roleName);
}

int Connection::hasRole(Role *role)
{
	if (!user || !role)
		return 0;

	int mask = user->hasRole (role);

	if (mask < 0)
		if (user->system)
			return GRANT_OPTION;
		else
			return mask;

	if (activeRoles.isMember (role))
		mask |= ROLE_ACTIVE;

	if (user->system)
		mask |= GRANT_OPTION;

	return mask;
}


int Connection::hasActiveRole(const char *name)
{
	FOR_STACK (const char*, string, &nameSpace)
		return hasActiveRole (string, name);
	END_FOR;

	return 0;
}

int Connection::hasActiveRole(const char *schema, const char *name)
{
	const char *roleName = database->getSymbol (name);
	const char *schemaName = database->getSymbol (schema);

	FOR_OBJECTS (Role*, role, &activeRoles)
		if (role->name == roleName && role->schemaName == schemaName)
			return ROLE_ACTIVE | HAS_ROLE;
	END_FOR;

	return 0;
}


int Connection::hasActiveRole(const WCString *schema, const WCString *name)
{
	const char *roleName = database->getSymbol (name);
	const char *schemaName = database->getSymbol (schema);

	FOR_OBJECTS (Role*, role, &activeRoles)
		if (role->name == roleName && role->schemaName == schemaName)
			return ROLE_ACTIVE | HAS_ROLE;
	END_FOR;

	return 0;
}

void Connection::validate(int optionMask)
{
	/***
	if (optionMask & validateSpecial)
		{
		Test::copyDocuments(this);
		return;
		}
	***/

	if (database)
		database->validate (optionMask);
}

bool Connection::assumeRole(Role * role)
{
	int mask = user->hasRole (role);

	if (!(mask & HAS_ROLE))
		return false;

	if (!(mask & ROLE_ACTIVE))
		addRole (role);

	return true;
}

void Connection::openDatabase(const char * dbName, Parameters * parameters, Threads *parent)
{
	const char *account = parameters->findValue ("user", "");
	const char *password = parameters->findValue ("password", "");
	const char *address = parameters->findValue ("address", NULL);
	openDatabase(dbName, NULL, account, password, address, parent);
}

void Connection::openDatabase(const char* dbName, const char *filename, const char* account, const char* password, const char* address, Threads* parent)
{
	char dbFileName [256];

	if (database)
		throw SQLEXCEPTION (CONNECTION_ERROR, "database is already open");

	// If we have a filename, we're coming from the storage engine, and
	// we should ignore the registry.

	if (filename)
		IO::expandFileName(filename, sizeof(dbFileName), dbFileName);
	else
		{
		if (!registry.findDatabase (dbName, sizeof (dbFileName), dbFileName))
			throw SQLEXCEPTION (CONNECTION_ERROR, "can't find database \"%s\"", dbName);
		}

	database = getDatabase(dbName, dbFileName, parent);
	
	try
		{
		user = database->findUser (account);
		}
	catch (SQLException &exception)
		{
		Log::debug ("Error during find user: %s\n", exception.getText());
		}

	if (!user)
		throw SQLEXCEPTION (SECURITY_ERROR, "\"%s\" is not a known user for database \"%s\"", 
							account, dbName);

	if (user->coterie)
		{
		if (!address || !user->validateAddress (atoi (address)))
			throw SQLEXCEPTION (SECURITY_ERROR, "invalid address for user \"%s\" in database \"%s\"", 
								account, dbName);
		}

	if (!user->validatePassword (password))
		throw SQLEXCEPTION (SECURITY_ERROR, "invalid password for user \"%s\" in database \"%s\"", 
								account, dbName);

	setUser (user);
}

void Connection::createDatabase(const char * dbName, Parameters * parameters, Threads *parent)
{
	if (database)
		throw SQLEXCEPTION (CONNECTION_ERROR, "database is already open");

	checkSitePassword (parameters);
	const char *fileName = parameters->findValue ("fileName", "");
	const char *account = parameters->findValue ("user", "");
	const char *password = parameters->findValue ("password", "");
	char dbFileName [1024];
	const char *dbFile = registry.findDatabase (dbName, sizeof (dbFileName), dbFileName);
	Sync sync (&databaseList, "Connection::createDatabase");
	sync.lock (Exclusive);
	
	if (dbFile)
		{
		for (Database *db = firstDatabase; db; db = db->next)
			if (db->matches (dbFile))
				{
				unlink(db);
				db->release();
				break;
				}
		}

	registry.defineDatabase (dbName, fileName);
	registry.findDatabase (dbName, sizeof (dbFileName), dbFileName);

	database = new Database (dbName, configuration, parent);
	database->createDatabase (dbFileName);
	database->addRef();
	link(database);
	database->next = NULL;
	database->prior = lastDatabase;
	user = database->createUser (account, password, false, NULL);

	for (const char **p = ddl; *p; ++p)
		{
		char sql [256];
		sprintf (sql, *p, account);
		database->execute (sql);
		}

#ifndef STORAGE_ENGINE
	JString path;
	const char *backupFile = parameters->findValue ("backupFileName", NULL);
	
	if (backupFile)
		path = backupFile;
	else
		{
		path = Registry::getInstallPath();
		path += "/base.nbk";
		}

	database->java->initialize (this, path);
#endif

	database->commitSystemTransaction();
	commit();
}


Database* Connection::createDatabase(const char *dbName, const char *fileName, const char *account, const char *password, Threads *threads)
{
	char dbFileName [1024];

	if (database)
		throw SQLEXCEPTION (CONNECTION_ERROR, "database is already open");

	Sync sync (&databaseList, "Connection::createDatabase");
	sync.lock (Exclusive);

	if (!registry.findDatabase (dbName, sizeof (dbFileName), dbFileName))
		for (Database *db = firstDatabase; db; db = db->next)
			if (db->matches (fileName))
				{
				unlink(db);
				db->release();
				break;
				}

	try
		{
		registry.defineDatabase (dbName, fileName);
		
		if (!registry.findDatabase (dbName, sizeof (dbFileName), dbFileName))
			strcpy(dbFileName, fileName);
		}
	catch(...)
		{
#ifdef STORAGE_ENGINE
		strcpy(dbFileName, fileName);
#else
		throw;
#endif
		}

	database = new Database (dbName, configuration, threads);

#ifdef STORAGE_ENGINE
	//strcpy(dbFileName, fileName);
#endif
	
	try
		{
		database->createDatabase (dbFileName);
		}
	catch (...)
		{
		delete database;
		database = NULL;
		throw;
		}
		
	database->addRef();
	link(database);
	user = database->createUser (account, password, false, NULL);

	for (const char **p = ddl; *p; ++p)
		{
		char sql [256];
		sprintf (sql, *p, account);
		database->execute (sql);
		}

	database->commitSystemTransaction();
	commit();
	
	return database;
}

void Connection::requiresLicense()
{
#ifdef LICENSE
	if (licenseToken)
		return;

	if (licenseNotRequired)
		return;

	if (database->systemConnection == this)
		return;

	LicenseProduct *product = database->licenseManager->getProduct (SERVER_PRODUCT);
	licenseToken = product->getToken();

	if (!licenseToken)
		throw SQLEXCEPTION (RUNTIME_ERROR, "No Server Licenses Available");
#endif
}

Sequence* Connection::findSequence(const char *name)
{
	SequenceManager *manager = database->sequenceManager;
	const char *symbol = database->getSymbol (name);

	FOR_STACK (const char*, string, &nameSpace)
		Sequence *sequence = manager->findSequence (string, symbol);
		if (sequence)
			return sequence;
	END_FOR;

	return NULL;
}

int64 Connection::getSequenceValue(const char *sequenceName)
{
	Sequence *sequence = findSequence (sequenceName);

	if (!sequence)
		throw SQLEXCEPTION (RUNTIME_ERROR, "Sequence \"%s\" is not defined", sequenceName);

	return sequence->update (1, getTransaction());
}


int64 Connection::getSequenceValue(const WCString *sequenceName)
{
	return getSequenceValue (database->getSymbol (sequenceName));
}


int64 Connection::getSequenceValue(const char *schemaName, const char *sequenceName)
{
	Sequence *sequence = getSequence (database->getSymbol (schemaName), database->getSymbol (sequenceName));

	return sequence->update (1, getTransaction());
}

int64 Connection::getSequenceValue(const WCString *schemaName, const WCString *sequenceName)
{
	Sequence *sequence = getSequence (database->getSymbol (schemaName), database->getSymbol (sequenceName));

	return sequence->update (1, getTransaction());
}

Table* Connection::findTable(const char *name)
{
	FOR_STACK (const char*, string, &nameSpace)
		Table *table = database->findTable (string, name);
		if (table)
			return table;
	END_FOR;

	return NULL;
}


void Connection::setAttribute(const char *name, const char *value)
{
	attributes.setValue (name, value);
}

const char* Connection::getAttribute(const char *name, const char *defaultValue)
{
	if (!strcasecmp (name, "account"))
		return user->name;

	if (!strcasecmp (name, "namespace"))
		{
		if (nameSpace.isEmpty())
			throw SQLError (RUNTIME_ERROR, "no namespace is currented defined");
		return currentSchemaName();
		}

	return attributes.findValue (name, defaultValue);
}


void Connection::releaseActiveRoles()
{
	FOR_OBJECTS (Role*, role, &activeRoles)
		role->release();
	END_FOR;

	activeRoles.clear();
}

bool Connection::checkAccess(int32 privMask, PrivilegeObject *object)
{
	if (database->systemConnection == this)
		return true;

	if (user->system)
		return true;

	int32 mask = user->getPrivileges (object);

	if (!(privMask & ~mask))
		return true;
	
	mask = database->roleModel->publicUser->getPrivileges (object);

	if (!(privMask & ~mask))
		return true;
	
	FOR_OBJECTS (Role*, role, &activeRoles)
		mask = role->getPrivileges (object);
		if (!(privMask & ~mask))
			return true;
	END_FOR

	return false;
}

bool Connection::dropRole(Role *role)
{
	if (!activeRoles.deleteItem (role))
		return false;

	role->release();

	return true;
}

void Connection::formatDateString(const char *format, int length, int32 date, char *buffer)
{
	struct tm time;
	DateTime::getYMD (date, &time);
	strftime (buffer, length, format, &time);
}

void Connection::setAutoCommit(bool flag)
{
	autoCommit = flag;
}

bool Connection::getAutoCommit()
{
	return autoCommit;
}

void Connection::updateSchedule(const char *application, const char *eventName, const char *schedule)
{
	database->scheduler->updateSchedule (application, eventName, user, schedule);
}

Connection* Connection::clone()
{
	Connection *connection = new Connection (database, user);
	Stack names;

	FOR_STACK (const char*, name, &nameSpace)
		names.push ((void*) name);
	END_FOR;

	while (!names.isEmpty())
		connection->pushSchemaName ((const char*)names.pop());

	return connection;
}

void Connection::setUser(User *usr)
{
	if ( (user = usr) )
		for (UserRole *userRole = user->roleList; userRole; userRole = userRole->next)
			if (userRole->defaultRole)
				addRole (userRole->role);
}

#ifndef STORAGE_ENGINE
JavaConnection* Connection::getJavaConnection()
{
	javaConnections = new JavaConnection (this, javaConnections);

	return javaConnections;
}

void Connection::closeJavaConnection(JavaConnection *connection)
{
	for (JavaConnection **ptr = &javaConnections; *ptr; ptr = &(*ptr)->next)
		if (*ptr == connection)
			{
			*ptr = connection->next;
			break;
			}
}
#endif

Server* startServer(int port, const char *configFile)
{
	server = new Server (port, configFile);
	server->addRef();

	return server;
}

void Connection::serverOperation(int op, Parameters *parameters)
{
	switch (op)
		{
		case opInstallLicense:
			break;

		default:
			checkSitePassword (parameters);
		}

	database->serverOperation (op, parameters);
}

bool Connection::enableFilterSet(FilterSet *filterSet)
{
	if (filterSets.appendUnique (filterSet))
		{
		filterSet->addRef();
		return false;
		}

	return true;
}

void Connection::disableFilterSet(FilterSet *filterSet)
{
	if (filterSets.deleteItem (filterSet))
		filterSet->release();
}

void Connection::serverExit()
{
	//MemMgrFini();
}

void Connection::enableTriggerClass(const char *name)
{
	disabledTriggerClasses.deleteItem ((void*) database->getSymbol (name));
}

void Connection::disableTriggerClass(const char *name)
{
	disabledTriggerClasses.appendUnique ((void*) database->getSymbol (name));
}

JString Connection::analyze(int mask)
{
	return database->analyze (mask);
}

int32 Connection::setTraceFlags(int32 flags)
{
	int32 oldFlags = traceFlags;
	traceFlags = flags;

	return oldFlags;
}


void Connection::checkSitePassword(Parameters *parameters)
{
	const char *sitePassword = parameters->findValue ("sitePassword", NULL);

	if (!sitePassword)
		throw SQLEXCEPTION (CONNECTION_ERROR, "a site password is required for server operator");

	checkSitePassword (sitePassword);
}


void Connection::checkSitePassword(const char *sitePassword)
{
	if (!Registry::checkSitePassword (sitePassword))
		throw SQLEXCEPTION (CONNECTION_ERROR, "invalid site password");
}

void Connection::clearResultSets()
{
	Sync global (&database->syncResultSets, "Connection::clearResultSets");

	while (resultSets)
		{
		global.lock (Exclusive);
		ResultSet *resultSet = resultSets;
		if (resultSet)
			{
			resultSets = resultSet->connectionNext;
			resultSet->connectionClosed();
			}
		global.unlock();
		}
}

void Connection::clearStatements()
{
	Sync global (&database->syncConnectionStatements, "Connection::clearStatements");

	while (statements)
		{
		global.lock (Exclusive);
		Statement *statement = statements;
		
		if (statement)
			{
			statements = statement->connectionNext;
			statement->connectionClosed();
			}
			
		global.unlock();
		}

}

void Connection::addStatement(Statement *statement)
{
	Sync sync (&syncObject, "Connection::addStatement");
	sync.lock (Exclusive);
	statement->connectionNext = statements;
	statements = statement;
}

void Connection::checkStatement(Statement *handle)
{
	Sync sync (&syncObject, "Connection::checkStatement");
	sync.lock (Shared);

	for (Statement *statement = statements; statement; statement = statement->connectionNext)
		if (statement == handle)
			return;

	throw SQLEXCEPTION (CONNECTION_ERROR, "bad Statement handle");
}

void Connection::checkResultSet(ResultSet *handle)
{
	Sync sync (&syncResultSets, "Connection::checkResultSet");
	sync.lock (Shared);

	for (ResultSet *resultSet = resultSets; resultSet; resultSet = resultSet->connectionNext)
		if (resultSet == handle)
			return;

	throw SQLEXCEPTION (CONNECTION_ERROR, "bad ResultSet handle");
}

void Connection::checkPreparedStatement(PreparedStatement *statement)
{
	checkStatement ((Statement*) statement);

	if (!statement->isPreparedResultSet())
		throw SQLEXCEPTION (CONNECTION_ERROR, "bad PreparedStatement handle");
}

void Connection::setLicenseNotRequired(bool flag)
{
	licenseNotRequired = flag;
}

bool Connection::enableFilterSet(const WCString *schemaName, const WCString *filterSetName)
{
	const char *schema = database->getSymbol (schemaName);
	const char *name = database->getSymbol (filterSetName);
	FilterSet *filterSet = database->filterSetManager->findFilterSet (schema, name);

	if (!filterSet)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s not defined", schema, name);

	return enableFilterSet (filterSet);
}

void Connection::disableFilterSet(const WCString *schemaName, const WCString *filterSetName)
{
	const char *schema = database->getSymbol (schemaName);
	const char *name = database->getSymbol (filterSetName);
	FilterSet *filterSet = database->filterSetManager->findFilterSet (schema, name);

	if (!filterSet)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s not defined", schema, name);

	disableFilterSet (filterSet);
}

#ifndef STORAGE_ENGINE
int Connection::attachDebugger(const char *sitePassword)
{
	checkSitePassword (sitePassword);

	int port = database->attachDebugger();
	activeDebugger = true;

	return port;
}

JString Connection::debugRequest(const char *request)
{
	return database->debugRequest (request);
}
#endif

Sequence* Connection::getSequence(const char *schemaName, const char *sequenceName)
{
	SequenceManager *manager = database->sequenceManager;
	Sequence *sequence = manager->findSequence (schemaName, sequenceName);

	if (!sequence)
		throw SQLEXCEPTION (RUNTIME_ERROR, "Sequence \"%s.%s\" is not defined", schemaName, sequenceName);

	return sequence;
}

int Connection::getLimit(int which)
{
	switch (which)
		{
		case connectionMaxSortLimit:
			return maxSort;

		case statementMaxSortLimit:
			return statementMaxSort;

		case connectionMaxRecordsLimit:
			return maxRecords;

		case statementMaxRecordsLimit:
			return statementMaxRecords;
		}

	return -2;
}

int Connection::setLimit(int which, int value)
{
	int oldValue = getLimit (which);

	switch (which)
		{
		case connectionMaxSortLimit:
			maxSort = value;
			break;

		case statementMaxSortLimit:
			statementMaxSort = value;
			largeSort = value / 2;
			break;

		case connectionMaxRecordsLimit:
			maxRecords = value;
			break;

		case statementMaxRecordsLimit:
			statementMaxRecords = value;
			break;
		}

	return oldValue;
}

void Connection::deleteRepositoryBlob(const char *schema, const char *repositoryName, int volume, int64 blobId)
{
	database->deleteRepositoryBlob (schema, repositoryName, volume, blobId, getTransaction());
}

PreparedStatement* Connection::findRegisteredStatement(StatementType type, void *owner)
{
	for (RegisteredStatement *reg = registeredStatements; reg; reg = reg->next)
		if (reg->type == type && reg->owner == owner)
			return reg->statement;

	return NULL;
}

void Connection::registerStatement(StatementType type, void *owner, PreparedStatement *statement)
{
	RegisteredStatement *reg = new RegisteredStatement;
	reg->owner = owner;
	reg->type = type;
	reg->statement = statement;
	reg->next = registeredStatements;
	registeredStatements = reg;
}

Statement* Connection::findStatement(int32 handle)
{
	for (Statement *statement = statements; statement; statement = statement->connectionNext)
		if (statement->handle == handle)
			return statement;

	return NULL;
}

int32 Connection::getNextHandle()
{
	return ++nextHandle;
}

ResultSet* Connection::findResultSet (int32 handle)
{
	for (ResultSet *resultSet = resultSets; resultSet; resultSet = resultSet->connectionNext)
		if (resultSet->handle == handle)
			return resultSet;

	return NULL;
}

ResultList* Connection::findResultList(int32 handle)
{
	for (Statement *statement = statements; statement; statement = statement->next)
		{
		ResultList *resultList = statement->findResultList(handle);

		if (resultList)
			return resultList;
		}

	return NULL;
}

Database* Connection::getDatabase(const char* dbName, const char* dbFileName, Threads* threads)
{
	Sync sync (&databaseList, "Connection::getDatabase");
	sync.lock (Shared);
	Database *db;

	for (db = firstDatabase; db; db = db->next)
		if (db->matches (dbFileName))
			{
			db->addRef();
			return db;
			}

	sync.unlock();
	sync.lock (Exclusive);
	
	for (db = firstDatabase; db; db = db->next)
		if (db->matches (dbFileName))
			{
			db->addRef();
			return db;
			}
	
	db = new Database (dbName, configuration, threads);
	
	try
		{
		db->openDatabase (dbFileName);
		db->addRef();
		link(db);
		}
	catch (...)
		{
		delete db;
		throw;
		}

	return db;
}

Sequence* Connection::findSequence(const char *schema, const char *name)
{
	return database->sequenceManager->findSequence (database->getSymbol(schema), database->getSymbol(name));
}

void Connection::dropDatabase()
{
	if (!database)
		throw SQLEXCEPTION (CONNECTION_ERROR, "database isn't open");
		
	Sync sync (&databaseList, "Connection::dropDatabase");
	sync.lock (Exclusive);
	unlink(database);
	detachDatabase();
	database->dropDatabase();
	database->release();
	database = NULL;
}

void Connection::shutdownNow()
{
	panicShutdown = true;
	Sync sync(&shutdownMutex, "Connection::shutdownNow");
	sync.lock(Exclusive);

	for (Database *database = firstDatabase; database; database = database->next)
		database->shutdownNow();
}

void Connection::detachDatabase()
{
	for (RegisteredStatement *reg; (reg = registeredStatements);)
		{
		registeredStatements = reg->next;
		delete reg;
		}

#ifndef STORAGE_ENGINE
	for (JavaConnection *connection = javaConnections; connection; connection = connection->next)
		connection->clear();
#endif

#ifdef LICENSE		
	if (licenseToken)
		licenseToken->release();
#endif

	releaseActiveRoles();

	while (!previousRoles.isEmpty())
		{
		Role *role = (Role*) previousRoles.pop();
		if (role)
			role->release();
		}

	try
		{
		if (transaction && transaction->state == Active)
			transaction->rollback();
		}
	catch (SQLException &exception)
		{
		Log::debug ("~Connection rollback failed: %s\n", exception.getText());
		}

	if (resultSets)
		clearResultSets();

	if (statements)
		clearStatements();

	if (metaData)
		delete metaData;

	if (database)
		database->release();

	FOR_OBJECTS (FilterSet*, filterSet, &filterSets)
		filterSet->release();
	END_FOR;

}

void Connection::unlink(Database *database)
{
	if (database->next)
		database->next->prior = database->prior;
	else
		lastDatabase = database->prior;

	if (database->prior)
		database->prior->next = database->next;
	else
		firstDatabase = database->next;
}

void Connection::link(Database *database)
{
	database->next = NULL;

	if ( (database->prior = lastDatabase) )
		lastDatabase->next = database;
	else
		firstDatabase = database;


	lastDatabase = database;
}

void Connection::setTransactionIsolation(int level)
{
	isolationLevel = level;
}

int Connection::getTransactionIsolation()
{
	return isolationLevel;
}

void Connection::commitByXid(int xidLength, const UCHAR* xid)
{
	if (database)
		database->commitByXid(xidLength, xid);
}

void Connection::rollbackByXid(int xidLength, const UCHAR* xid)
{
	if (database)
		database->rollbackByXid(xidLength, xid);
}

void Connection::shutdownDatabase(void)
{
	if (database)
		{
		Database *db = database;
		database = NULL;
		unlink(db);
		close();
		db->shutdown();
		delete db;
		}
	else
		close();
}

void Connection::setCurrentStatement(const char* text)
{
	currentStatement = text;
}

char* Connection::getCurrentStatement(char* buffer, uint bufferLength)
{
	char *q = buffer;
	char *end = buffer + bufferLength - 2;
	
	if (currentStatement)
		for (const char *p = currentStatement; *p;)
			{
			char c = *p++;
			
			if (c == '\r')
				continue;
			
			if (c == '\n')
				c = ' ';
			
			if (q < end)
				*q++ = c;
			}
		
	*q = 0;
	
	return buffer;
}

void Connection::setRecordMemoryMax(uint64 value)
{
	if (database)
		database->setRecordMemoryMax(value);
}

void Connection::setRecordScavengeThreshold(int value)
{
	if (database)
		database->setRecordScavengeThreshold(value);
}

void Connection::setRecordScavengeFloor(int value)
{
	if (database)
		database->setRecordScavengeFloor(value);
}
