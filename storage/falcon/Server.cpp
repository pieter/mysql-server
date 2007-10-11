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

// Server.cpp: implementation of the Server class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "ScanDir.h"
#include "Socket.h"
#include "Server.h"
#include "Database.h"
#include "Protocol.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "SQLError.h"
#include "Threads.h"
#include "ResultSet.h"
#include "ResultList.h"
#include "ResultSetMetaData.h"
#include "DatabaseMetaData.h"
#include "Value.h"
#include "Parameters.h"
#include "Thread.h"
#include "Log.h"
#include "Registry.h"
#include "Sync.h"
#include "StreamSegment.h"
#include "Configuration.h"

#ifndef STORAGE_ENGINE
#include "TemplateContext.h"
#endif

#ifdef LICENSE
#include "License.h"
#endif

#define	LOG_TIMEOUT			10000

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Server::Server(Server *server, Protocol *proto)
{
	init();
	active = true;
	useCount = 1;
	parent = server;
	configuration = parent->configuration;
	configuration->addRef();
	connection = new Connection (configuration);

	protocol = proto;
	threads = NULL;
}

Server::Server(int requestedPort, const char *configFileName)
{
	init();
	//configFile = configFileName;
	configuration = new Configuration(configFileName);
	useCount = 2;							// one for caller, one for thread
	threads = new Threads (NULL, configuration->maxThreads);
	Thread::getThread ("Server::Server");	// Just to label thread
	port = (requestedPort) ? requestedPort : PORT;
	serverSocket = new Socket;
	//int32 ipAddress = getLicenseIP();
	serverSocket->bind (0, port);
	serverSocket->listen();
	thread = threads->start ("Server::Server", &Server::getConnections, this);
}


void Server::init()
{
	parent = NULL;
	connection = NULL;
	protocol = NULL;
	active = false;
	serverShutdown = false;
	forceShutdown = false;
	panicMode = false;
	thread = NULL;
	threads = NULL;
	serverSocket = NULL;
	string1 = NULL;
	string2 = NULL;
	string3 = NULL;
	string4 = NULL;
	string5 = NULL;
	string6 = NULL;
	activeServers = NULL;
	first = true;
	logSocket = NULL;
	acceptLogSocket = NULL;
	waitThread = NULL;
}

Server::~Server()
{
	cleanup();
	Log::deleteListener (logListener, this);	// just in case

	if (protocol)
		{
		delete protocol;
		protocol = NULL;
		}

	if (logSocket)
		{
		logSocket->close();
		delete logSocket;
		}

	if (acceptLogSocket)
		acceptLogSocket->close();

	if (connection)
		connection->close();

	if (serverSocket)
		{
		serverSocket->close();
		delete serverSocket;
		}

	if (threads)
		{
		threads->shutdownAll();
		threads->release();
		}

	if (configuration)
		configuration->release();
}

void Server::getConnections()
{
	active = true;

	try
		{
		for (;;)
			{
			Protocol *protocol = serverSocket->acceptProtocol();
			if (!protocol)
				break;
			Server *server = new Server (this, protocol);
			Sync sync (&syncObject, "Server::getConnections");
			sync.lock (Exclusive);
			server->nextServer = activeServers;
			sync.unlock();
			activeServers = server;
			server->startAgent();
			}
		}
	catch (...)
		{
		active = false;
		release();
		throw;
		}

	active = false;
	release();
}

void Server::getConnections(void * lpParameter)
{
	((Server*) lpParameter)->getConnections();
}

void Server::startAgent()
{
	thread = parent->threads->startWhenever ("Server::startAgent", agentLoop, this);
}

void Server::agentLoop(void * lpParameter)
{
	((Server*) lpParameter)->agentLoop();
}

void Server::agentLoop()
{
	thread = Thread::getThread("Server::agentLoop");
	MsgType msgType;

	try
		{
		while (connection && protocol)
			{
			msgType = protocol->getMsg();
			
			if (first)
				{
				if ((int) msgType & 0xffff0000)
					{
					protocol->setSwapBytes (true);
					protocol->swapBytes (sizeof (msgType), &msgType);
					}
					
				first = false;
				}
				
			try
				{
				dispatch (msgType);
				Thread::validateLocks();
				}
			catch (SQLException &exception)
				{
				protocol->sendFailure (&exception);
				}
				
			cleanup();
			
			if (protocol)
				protocol->flush();
			}
		}
	catch (...)
		{
		ASSERT (thread->javaThread == NULL);
		
		try
			{
			if (connection)
				connection->rollback();
			}
		catch (...)
			{
			}
			
		if (protocol)
			protocol->shutdown();
		}

	if (connection)
		{
		connection->close();
		connection = NULL;
		}

	if (parent)
		parent->agentShutdown (this);

	release();
}

void Server::prepareStatement()
{
	string1 = protocol->getString();
	PreparedStatement *statement = connection->prepareStatement (string1);
	connection->checkStatement (statement);
	protocol->sendSuccess();
	protocol->putHandle (statement->handle);
	protocol->putLong (statement->parameters.count);
}


void Server::executePreparedQuery()
{
	PreparedStatement *statement = getPreparedStatement();
	int32 numberParameters = protocol->getLong();
	connection->checkPreparedStatement (statement);

	for (int n = 0; n < numberParameters; ++n)
		{
		int parameter = protocol->getLong();
		protocol->getValue (statement->parameters.values + parameter);
		}

	ResultSet *resultSet = statement->executeQuery();
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::sendResultSet(ResultSet * resultSet)
{
	protocol->putHandle (resultSet->handle);
	int columns = resultSet->numberColumns;
	protocol->putLong (columns);

	for (int n = 1; n <= columns; ++n)
		protocol->putString (resultSet->getColumnName (n));
}

void Server::getMetaData()
{
	ResultSet *resultSet = findResultSet();
	connection->checkResultSet (resultSet);
	ResultSetMetaData *metaData = resultSet->getMetaData();
	protocol->sendSuccess();
	int numberColumns = metaData->getColumnCount();
	protocol->putLong (numberColumns);
	protocol->putString (metaData->getQuery());

	for (int n = 1; n <= numberColumns; ++n)
		{
		protocol->putString (metaData->getColumnName (n));
		protocol->putString (metaData->getTableName (n));
		protocol->putString (metaData->getSchemaName (n));
		protocol->putLong (metaData->getColumnType (n));
		protocol->putLong (metaData->getColumnDisplaySize (n));
		protocol->putBoolean (metaData->isNullable (n));
		if (protocol->protocolVersion >= PROTOCOL_VERSION_3)
			protocol->putShort (metaData->getScale (n));
		}
}

void Server::next()
{
	ResultSet *resultSet = findResultSet();
	connection->checkResultSet (resultSet);
	Value *vector [20];
	Value **values = vector;
	bool b = resultSet->next();
	int count = resultSet->numberColumns;

	if (b)
		try
			{
			if (count > 20)
				values = new Value* [count];
			for (int n = 1; n <= count; ++n)
				{
				Value *value = values [n - 1] = resultSet->getValue (n);
				value->getStringLength();
				}
			}
		catch (...)
			{
			if (values != vector)
				delete [] values;
			throw;
			}

	protocol->sendSuccess();
	protocol->putLong (b);

	if (!b)
		return;

	protocol->putLong (count);
	Database *database = connection->database;

	for (int n = 0; n < count; ++n)
		protocol->putValue (database, values [n]);

	if (values != vector)
		delete [] values;
}

void Server::commitTransaction()
{
	connection->commit();
	protocol->sendSuccess();
}

void Server::prepareTransaction()
{
	connection->prepare(0, NULL);
	protocol->sendSuccess();
}

void Server::rollbackTransaction()
{
	connection->rollback();
	protocol->sendSuccess();
}

void Server::closeConnection()
{
	connection->close();
	connection = NULL;
	protocol->sendSuccess();
	protocol->shutdown();
	delete protocol;
	protocol = NULL;
}

void Server::createStatement()
{
	Statement *statement = connection->createStatement ();
	protocol->sendSuccess();
	protocol->putHandle (statement->handle);
}

void Server::execute()
{
	Statement *statement = getStatement();
	string1 = protocol->getString();
	connection->checkStatement (statement);
	bool result = statement->execute (string1);
	protocol->sendSuccess();
	protocol->putBoolean (result);
}

void Server::executeUpdate()
{
	Statement *statement = getStatement();
	string1 = protocol->getString();
	connection->checkStatement (statement);
	int result = statement->executeUpdate (string1);
	protocol->sendSuccess();
	protocol->putLong (result);
}

void Server::search()
{
	Statement *statement = getStatement();
	string1 = protocol->getString();
	connection->checkStatement (statement);
	ResultList *resultList = statement->search (string1);
	resultList->sort();
	protocol->sendSuccess();
	protocol->putHandle (resultList->handle);
	protocol->putLong (resultList->getCount());
}

void Server::getResultSet()
{
	Statement *statement = getStatement(); 
	connection->checkStatement (statement);
	ResultSet *resultSet = statement->getResultSet();
	protocol->sendSuccess();
	
	if (resultSet)
		{
		protocol->putLong (1);
		sendResultSet (resultSet);
		}
	else
		protocol->putLong (0);
}

void Server::dispatch(MsgType type)
{
	switch (type)
		{
		case OpenDatabase2:
			openDatabase2();
			break;

		case CreateDatabase2:
			createDatabase2();
			break;

		case GetDatabaseMetaData:
			getDatabaseMetaData();
			break;

		case Shutdown:
			protocol->close();
			delete protocol;
			protocol = NULL;
			return;

		case PrepareStatement:
			prepareStatement();
			break;

		case CreateStatement:
			createStatement();
			break;

		case GenConnectionHtml:
			genConnectionHtml();
			break;

		case SetCursorName:
			setCursorName();
			break;

		case Execute:
			execute();
			break;

		case ExecuteQuery:
			executeQuery();
			break;

		case Search:
			search();
			break;

		case ExecuteUpdate:
			executeUpdate();
			break;

		case GetResultSet:
			getResultSet();
			break;

		case GetMoreResults:
			getMoreResults();
			break;

		case GetUpdateCount:
			getUpdateCount();
			break;

		case PrepareDrl:
			prepareDrl();
			break;

		case ExecutePreparedQuery:
			executePreparedQuery();
			break;

		case ExecutePreparedStatement:
			executePreparedStatement();
			break;

		case ExecutePreparedUpdate:
			executePreparedUpdate();
			break;

		case GetMetaData:
			getMetaData();
			break;

		case Next:
			next();
			break;

		case NextHit:
			nextHit();
			break;

		case FetchRecord:
			fetchRecord();
			break;

		case CommitTransaction:
			commitTransaction();
			break;

		case PrepareTransaction:
			prepareTransaction();
			break;

		case RollbackTransaction:
			rollbackTransaction();
			break;

		case CloseConnection:
			closeConnection();
			break;

		case GetTables:
			getTables();
			break;

		case GetColumns:
			getColumns();
			break;

		case GetPrimaryKeys:
			getPrimaryKeys();
			break;

		case GetTriggers:
			getTriggers();
			break;

		case GetImportedKeys:
			getImportedKeys();
			break;

		case GetIndexInfo:
			getIndexInfo();
			break;

		case Ping:
			ping();
			break;

		case HasRole:
			hasRole();
			break;

		case GetUsers:
			getUsers();
			break;

		case GetRoles:
			getRoles();
			break;

		case GetUserRoles:
			getUserRoles();
			break;

		case GetObjectPrivileges:
			getObjectPrivileges();
			break;

		case InitiateService:
			initiateService();
			break;

		case Validate:
			validate();
			break;

		case GetSequenceValue:
			getSequenceValue();
			break;

		case GetSequenceValue2:
			getSequenceValue2();
			break;

		case CloseStatement:
			closeStatement();
			break;

		case CloseResultSet:
			closeResultSet();
			break;

		case AddLogListener:
			addListener();
			break;

		case GetDatabaseProductName:
			getDatabaseProductName();
			break;

		case GetDatabaseProductVersion:
			getDatabaseProductVersion();
			break;

		case Analyze:
			analyze();
			break;

		case StatementAnalyze:
			statementAnalyze();
			break;

		case SetTraceFlags:
			setTraceFlags();
			break;

		case SetAttribute:
			setAttribute();
			break;

		case ServerOperation:
			serverOperation();
			break;

		case GetHolderPrivileges:
			getHolderPrivileges();
			break;

		case GetSequences:
			getSequences();
			break;

		case AttachDebugger:
			attachDebugger();
			break;

		case DebugRequest:
			debugRequest();
			break;

		case SetAutoCommit:
			setAutoCommit();
			break;

		case GetAutoCommit:
			getAutoCommit();
			break;

		case GetConnectionLimit:
			getConnectionLimit();
			break;

		case SetConnectionLimit:
			setConnectionLimit();
			break;

		case DeleteBlobData:
			deleteBlobData();
			break;

		default:
			{
			SQLError exception (NETWORK_ERROR, "unsupported verb code %d", type);
			protocol->sendFailure (&exception);
			protocol->close();
			delete protocol;
			protocol = NULL;
			return;
			}
		}
}

void Server::executePreparedStatement()
{
	PreparedStatement *statement = getPreparedStatement();
	connection->checkPreparedStatement (statement);
	int32 numberParameters = protocol->getLong();

	for (int n = 0; n < numberParameters; ++n)
		protocol->getValue (statement->parameters.values + n);

	bool result = statement->execute();
	protocol->sendSuccess();
	protocol->putBoolean (result);
}

void Server::nextHit()
{
	ResultList *resultList = getResultList();
	bool hit = resultList->next();
	protocol->sendSuccess();
	protocol->putBoolean (hit);

	if (hit)
		{
		protocol->putDouble (resultList->getScore());
		protocol->putString (resultList->getTableName());
		}
}

void Server::fetchRecord()
{
	ResultList *resultList = getResultList();
	ResultSet *resultSet = resultList->fetchRecord();
	protocol->sendSuccess();
	sendResultSet (resultSet);
}


void Server::executeQuery()
{
	Statement *statement = getStatement();
	string1 = protocol->getString();
	connection->checkStatement (statement);
	ResultSet *resultSet = statement->executeQuery (string1);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}


void Server::prepareDrl()
{
#ifdef STORAGE_ENGINE
	notInStorageEngine();
#else
	string1 = protocol->getString();
	PreparedStatement *statement = connection->prepareDrl (string1);
	protocol->sendSuccess();
	protocol->putHandle (statement->handle);
	protocol->putLong (statement->parameters.count);
#endif
}

void Server::setCursorName()
{
	Statement *statement = getStatement();
	string1 = protocol->getString();	// name
	connection->checkStatement (statement);
	statement->setCursorName(string1);
	protocol->sendSuccess();
}

void Server::executePreparedUpdate()
{
	PreparedStatement *statement = getPreparedStatement();
	int32 numberParameters = protocol->getLong();
	connection->checkStatement (statement);

	for (int n = 0; n < numberParameters; ++n)
		{
		int parameter = protocol->getLong();
		protocol->getValue (statement->parameters.values + parameter);
		}

	int rowsUpdated = statement->executeUpdate();
	protocol->sendSuccess();
	protocol->putLong (rowsUpdated);
}

void Server::genConnectionHtml()
{
#ifdef STORAGE_ENGINE
	notInStorageEngine();
#else
	TemplateContext *context = new TemplateContext (connection);

	try
		{
		int32 items = protocol->getLong();

		for (int n = 0; n < items; ++n)
			{
			char *variable = protocol->getString();
			int valueLength = protocol->getLong();
			char *value = new char [valueLength];
			protocol->getBytes (valueLength, value);
			context->putValue (variable, value, valueLength);
			//Log::debug ("%s : %.*s\n", variable, valueLength, value);
			delete [] variable;
			delete [] value;
			}

		int32 genHeaders = protocol->getLong();
		Clob *clob = connection->genHTML (context, genHeaders);
		protocol->sendSuccess();
		protocol->putLong (clob->length());

		for (StreamSegment ss(clob->getStream()); ss.remaining; ss.advance())
			protocol->putBytes(ss.available, ss.data);


		/***
		for (int offset = 0, length; length = clob->getSegmentLength (offset); offset += length)
			protocol->putBytes (length, clob->getSegment (offset));
		***/

		clob->release();
		context->release();
		}
	catch (...)
		{
		context->release();
		throw;
		}
#endif
}

void Server::shutdown(bool panic)
{
	if (active)
		{
		shutdownServer (panic);
		//threads->waitForAll();
		}
}

void Server::shutdownServer(bool panic)
{
	forceShutdown = true;
	panicMode = panic;

	if (waitThread)
		{
		waitThread->wake();
		return;
		}

	Thread *shutdownThread = Thread::getThread ("Server::shutdownServer");
	shutdownThread->addRef();
	addRef();
	thread->shutdown();
	stopServerThreads(panic);

	if (waitThread)
		waitThread->wake();

	shutdownThread->release();
	release();
}

void Server::agentShutdown(Server * server)
{
	Sync sync (&syncObject, "Server::agentShutdown");
	sync.lock (Exclusive);

	for (Server **ptr = &activeServers; *ptr; ptr = &(*ptr)->nextServer)
		if (*ptr == server)
			{
			*ptr = server->nextServer;
			break;
			}
}

void Server::shutdownAgent(bool panic)
{
	/*** this is awfully draconian.
	if (connection)
		{
		connection->close();
		connection = NULL;
		}
	***/

	if (protocol)
		protocol->shutdown();

	thread->shutdown();
	parent = NULL;
}

void Server::getDatabaseMetaData()
{
	DatabaseMetaData *metaData = connection->getMetaData ();
	protocol->sendSuccess();
	protocol->putHandle (metaData->handle);
}

void Server::getTables()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	//schemaPattern
	string3 = protocol->getString();	// tableNamePattern
	int typeCount = protocol->getLong();
	char **types = NULL;

	if (typeCount)
		{
		types = new char* [typeCount];
		for (int n = 0; n < typeCount; ++n)
			types [n] = protocol->getString();
		}

	ResultSet *resultSet = metaData->getTables (string1, string2, string3, typeCount, (const char**) types);
	protocol->sendSuccess();
	sendResultSet (resultSet);

	if (typeCount)
		{
		for (int n = 0; n < typeCount; ++n)
			delete [] types [n];
		delete [] types;
		}
}

void Server::getColumns()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();		// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// tableNamePattern
	string4 = protocol->getString();	// fieldNamePattern

	ResultSet *resultSet = metaData->getColumns (string1, string2, string3, string4);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getPrimaryKeys()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	//scheamPattern
	string3 = protocol->getString();	// tableNamePattern

	ResultSet *resultSet = metaData->getPrimaryKeys (string1, string2, string3);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getImportedKeys()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// tableNamePattern

	ResultSet *resultSet = metaData->getImportedKeys (string1, string2, string3);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getIndexInfo()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// tableNamePattern
	bool unique = protocol->getBoolean();
	bool approximate = protocol->getBoolean();

	ResultSet *resultSet = metaData->getIndexInfo (string1, string2, string3, unique, approximate);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getMoreResults()
{
	Statement *statement = getStatement();
	connection->checkStatement (statement);
	bool ret = statement->getMoreResults();
	protocol->sendSuccess();
	protocol->putBoolean (ret);
}

void Server::getUpdateCount()
{
	Statement *statement = getStatement();
	connection->checkStatement (statement);
	int count = statement->getUpdateCount();
	protocol->sendSuccess();
	protocol->putLong (count);
}

void Server::cleanup()
{
	if (string1)
		{
		delete [] string1;
		string1 = NULL;
		}

	if (string2)
		{
		delete [] string2;
		string2 = NULL;
		}

	if (string3)
		{
		delete [] string3;
		string3 = NULL;
		}

	if (string4)
		{
		delete [] string4;
		string4 = NULL;
		}

	if (string5)
		{
		delete [] string5;
		string5 = NULL;
		}

	if (string6)
		{
		delete [] string6;
		string6 = NULL;
		}
}

void Server::ping()
{
	protocol->sendSuccess();
}

void Server::hasRole()
{
	string1 = protocol->getString();
	string2 = protocol->getString();
	int mask = -1;
	Role *role = connection->findRole (string1, string2);

	if (role)
		mask = connection->hasRole (role);

	protocol->sendSuccess();
	protocol->putLong (mask);
}

void Server::getObjectPrivileges()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// namePattern
	int32 objectType = protocol->getLong();
	ResultSet *resultSet = metaData->getObjectPrivileges (string1, string2, string3, objectType);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getUserRoles()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schema
	string3 = protocol->getString();	// role
	string4 = protocol->getString();	// user

	ResultSet *resultSet = metaData->getUserRoles (string1, string2, string3, string4);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getRoles()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// rolePattern

	ResultSet *resultSet = metaData->getRoles (string1, string2, string3);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getUsers()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// userPattern

	ResultSet *resultSet = metaData->getUsers (string1, string2);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getParameters(Parameters * parameters)
{
	int32 items = protocol->getLong();

	for (int n = 0; n < items; ++n)
		{
		char *variable = protocol->getString();
		char *value = protocol->getString();
		parameters->putValue (variable, value);
		delete [] variable;
		delete [] value;
		}
}

void Server::openDatabase2()
{
	Parameters parameters;

	int protoVersion = protocol->getLong();
	protocol->protocolVersion = MIN (protoVersion, PROTOCOL_VERSION);
	string1 = protocol->getString();		// database
	getParameters (&parameters);

	int32 address = protocol->getPartnerAddress();
	char temp [32];
	sprintf (temp, "%d", address);
	parameters.putValue ("address", temp);
	connection->openDatabase (string1, &parameters, parent->threads);
	protocol->sendSuccess();
	protocol->putLong (protocol->protocolVersion);
}

void Server::createDatabase2()
{
	Parameters parameters;

	int protoVersion = protocol->getLong();
	protocol->protocolVersion = MIN (protoVersion, PROTOCOL_VERSION);
	string1 = protocol->getString();	// name
	getParameters (&parameters);
	connection->createDatabase (string1, &parameters, parent->threads);
	protocol->sendSuccess();
	protocol->putLong (protocol->protocolVersion);
}

void Server::initiateService()
{
#ifdef STORAGE_ENGINE
	notInStorageEngine();
#else
	string1 = protocol->getString();	// application
	string2 = protocol->getString();	// service
	int port = connection->initiateService (string1, string2);
	protocol->sendSuccess();
	protocol->putLong (port);
#endif
}


void Server::getTriggers()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	//scheamPattern
	string3 = protocol->getString();	// tableNamePattern
	string4 = protocol->getString();	// 	triggerNamePatterm

	ResultSet *resultSet = metaData->getTriggers (string1, string2, string3, string4);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::validate()
{
	int flags = protocol->getLong();
	connection->validate (flags);
	protocol->sendSuccess();
}

void Server::getSequenceValue()
{
	string1 = protocol->getString();
	int64 result = connection->getSequenceValue (string1);
	protocol->sendSuccess();
	protocol->putQuad (result);
}

void Server::closeStatement()
{
	Statement *statement = getStatement();
	connection->checkStatement (statement);
	statement->close();
	protocol->sendSuccess();
}

void Server::closeResultSet()
{
	ResultSet *resultSet = findResultSet();
	connection->checkResultSet (resultSet);
	resultSet->close();
	protocol->sendSuccess();
}

void Server::addListener()
{
	int mask = protocol->getLong();
    int port = 0;

	if (!logSocket)
		{
		acceptLogSocket = new Protocol;
		acceptLogSocket->bind (0, 0);
		acceptLogSocket->listen(1);
		parent->threads->start ("Server::addListener", &Server::acceptLogConnection, this);
		port = acceptLogSocket->getLocalPort();
		}

	protocol->sendSuccess();
	protocol->putShort (port);

	Log::deleteListener (logListener, this);
	Log::addListener (mask, logListener, this);
}

void Server::logListener(int mask, const char *text, void *arg)
{
	((Server*) arg)->logListener (mask, text);
}

void Server::logListener(int mask, const char *text)
{
	if (logSocket)
		try
			{
			logSocket->putLong (mask);
			logSocket->putString (text);
			logSocket->flush();
			}
		catch (...)
			{
			/***
			Log::deleteListener (logListener, this);
			Log::log ("Server::logListener: connection lost to %s: %s\n", 
					  (const char*) logSocket->getLocalName(),
					  (const char*) exception.getText());
			***/
			logSocket->close();
			delete logSocket;
			logSocket = NULL;
			throw;
			}
}

void Server::acceptLogConnection(void *arg)
{
	((Server*) arg)->acceptLogConnection();
}

void Server::acceptLogConnection()
{
	try
		{
		Protocol *protocol = acceptLogSocket->acceptProtocol();
		if (!protocol)
			return;
		logSocket = protocol;
		logSocket->setSwapBytes (protocol->swap);
		acceptLogSocket->close();
		delete acceptLogSocket;
		acceptLogSocket = NULL;
		logSocket->setWriteTimeout (LOG_TIMEOUT);
		}
	catch (...)
		{
		}
}

void Server::getDatabaseProductName()
{
	DatabaseMetaData *metaData = getDbMetaData();
	const char *string = metaData->getDatabaseProductName();
	protocol->sendSuccess();
	protocol->putString (string);
}

void Server::getDatabaseProductVersion()
{
	DatabaseMetaData *metaData = getDbMetaData();
	const char *string = metaData->getDatabaseProductVersion();
	protocol->sendSuccess();
	protocol->putString (string);
}

void Server::analyze()
{
	int mask = protocol->getLong();
	JString string = connection->analyze (mask);
	protocol->sendSuccess();
	protocol->putString (string);
}

void Server::statementAnalyze()
{
	Statement *statement = getStatement();
	int mask = protocol->getLong();
	connection->checkStatement (statement);
	JString string = statement->analyze (mask);
	protocol->sendSuccess();
	protocol->putString (string);
}

void Server::setTraceFlags()
{
	int32 mask = protocol->getLong();
	mask = connection->setTraceFlags (mask);
	protocol->sendSuccess();
	protocol->putLong (mask);
}

void Server::serverOperation()
{
	int op = protocol->getLong();
	Parameters parameters;
	getParameters (&parameters);
	connection->serverOperation (op, &parameters);
	protocol->sendSuccess();

	switch (op)
		{
		case opShutdown:
			parent->shutdownServer (false);
			break;
		}
}

void Server::setAttribute()
{
	string1 = protocol->getString();	// name
	string2 = protocol->getString();	// value
	connection->setAttribute (string1, string2);
	protocol->sendSuccess();
}

void Server::addRef()
{
	++useCount;
}

void Server::release()
{
	if (--useCount == 0)
		delete this;
}

int Server::waitForExit()
{
	//addRef();
	waitThread = Thread::getThread("Server::waitForExit()");

	while (!serverShutdown)
		if (forceShutdown)
			stopServerThreads(panicMode);
		else
			waitThread->sleep();

	threads->waitForAll();
	Thread::deleteThreadObject();
	release();

	return 0;
}

void Server::getSequences()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// namePattern

	ResultSet *resultSet = metaData->getSequences (string1, string2, string3);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}

void Server::getHolderPrivileges()
{
	DatabaseMetaData *metaData = getDbMetaData();
	string1 = protocol->getString();	// catalog
	string2 = protocol->getString();	// schemaPattern
	string3 = protocol->getString();	// namePattern
	int objectType = protocol->getLong();

	ResultSet *resultSet = metaData->getHolderPrivileges (string1, string2, string3, objectType);
	protocol->sendSuccess();
	sendResultSet (resultSet);
}


void Server::attachDebugger()
{
#ifdef STORAGE_ENGINE
	notInStorageEngine();
#else
	string1 = protocol->getString();
	int port = connection->attachDebugger (string1);
	protocol->sendSuccess();
	protocol->putLong (port);
#endif
}

void Server::debugRequest()
{
#ifdef STORAGE_ENGINE
	notInStorageEngine();
#else
	string1 = protocol->getString();
	JString response = connection->debugRequest (string1);
	protocol->sendSuccess();
	protocol->putString (response);
#endif
}

void Server::getSequenceValue2()
{
	string1 = protocol->getString();
	string2 = protocol->getString();
	int64 result = connection->getSequenceValue (string1, string2);
	protocol->sendSuccess();
	protocol->putQuad (result);
}

void Server::setAutoCommit()
{
	connection->setAutoCommit (protocol->getBoolean());
	protocol->sendSuccess();
}

void Server::getAutoCommit()
{
	bool value = connection->getAutoCommit();
	protocol->sendSuccess();
	protocol->putBoolean (value);

}

void Server::setConnectionLimit()
{
	int which = protocol->getLong();
	int value = protocol->getLong();
	int oldValue = connection->setLimit (which, value);
	protocol->sendSuccess();
	protocol->putLong (oldValue);
}

void Server::getConnectionLimit()
{
	int which = protocol->getLong();
	int oldValue = connection->getLimit (which);
	protocol->sendSuccess();
	protocol->putLong (oldValue);
}

int32 Server::getLicenseIP()
{
	int ipAddress = 0;

#ifdef LICENSE
	for (ScanDir dir (Registry::getInstallPath() + "/" + "licenses", "*.lic"); dir.next();)
		{
		const char *fileName = dir.getFilePath();
		JString licenseText = License::loadFile (fileName);

		if (!licenseText.IsEmpty())
			{
			License license (licenseText);

			if (license.valid && license.product.equalsNoCase (SERVER_PRODUCT))
				{
				int32 ip = license.getIpAddress (0);

				if (ip)
					ipAddress = ip;
				}
			}
		}
#endif

	return ipAddress;
}

void Server::deleteBlobData()
{
	string1 = protocol->getString();
	uint length = protocol->getLong();
	UCHAR repositoryInfo [256];

	if (length > sizeof (repositoryInfo))
		{
		for (uint n = 0; n < length; ++n)
			protocol->getByte();
		throw SQLError (NETWORK_ERROR, "invalidation repository data");
		}

	protocol->getBytes (length, repositoryInfo);
	BlobReference ref;
	ref.setReference (length, repositoryInfo);
	connection->deleteRepositoryBlob (string1, ref.repositoryName, ref.repositoryVolume, ref.blobId);
	protocol->sendSuccess();
}

PreparedStatement* Server::getPreparedStatement()
{
	Statement *statement = connection->findStatement(protocol->getHandle());

	return (PreparedStatement*) statement;
}

Statement* Server::getStatement()
{
	Statement *statement = connection->findStatement(protocol->getHandle());

	return statement;
}

DatabaseMetaData* Server::getDbMetaData()
{
	//int32 handle = 
	protocol->getHandle();

	return connection->getMetaData();
}

ResultSet* Server::findResultSet()
{
	return connection->findResultSet(protocol->getHandle());
}

ResultList* Server::getResultList()
{
	return connection->findResultList(protocol->getHandle());
}

bool Server::isActive()
{
	return active;
}

void Server::stopServerThreads(bool panic)
{
	serverSocket->shutdown();
	Sync sync (&syncObject, "Server::stopServerThreads");
	sync.lock (Shared);

	for (Server *server = activeServers; server; server = server->nextServer)
		server->shutdownAgent(panic);

	sync.unlock();
	threads->shutdownAll();
	threads->waitForAll();
	Connection::shutdownDatabases();
	serverShutdown = true;

}

void Server::notInStorageEngine()
{
	throw SQLError(FEATURE_NOT_YET_IMPLEMENTED, "feature not implemented in storage engine");
}
