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

// Server.h: interface for the Server class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERVER_H__84FD1985_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_SERVER_H__84FD1985_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "MsgType.h"
#include "SyncObject.h"

class Database;
class Socket;
CLASS(Protocol);
class Connection;
class Threads;
class ResultSet;
class Parameters;
class Thread;
class Configuration;
CLASS(Statement);
class PreparedStatement;
class DatabaseMetaData;
class ResultList;

class Server  
{
public:
	void notInStorageEngine();
	void stopServerThreads(bool panic);
	ResultList* getResultList();
	ResultSet* findResultSet();
	DatabaseMetaData* getDbMetaData();
	Statement* getStatement();
	PreparedStatement* getPreparedStatement();
	void deleteBlobData();
	int32 getLicenseIP();
	void getConnectionLimit();
	void setConnectionLimit();
	void getAutoCommit();
	void setAutoCommit();
	void getSequenceValue2();
	void debugRequest();
	void attachDebugger();
	void getHolderPrivileges();
	void getSequences();
	void addRef();
	void shutdownServer (bool panic);
	void setAttribute();
	void serverOperation();
	void setTraceFlags();
	void statementAnalyze();
	void analyze();
	void getDatabaseProductVersion();
	void getDatabaseProductName();
	void acceptLogConnection();
	static void acceptLogConnection (void *arg);
	void logListener (int mask, const char *text);
	static void logListener (int mask, const char *text, void *arg);
	void addListener();
	void closeResultSet();
	void closeStatement();
	void getSequenceValue();
	void validate();
	void getTriggers();
	void initiateService();
	void createDatabase2();
	void openDatabase2();
	void getParameters (Parameters *parameters);
	void getUsers();
	void getRoles();
	void getUserRoles();
	void getObjectPrivileges();
	void hasRole();
	void ping();
	void init();
	void cleanup();
	void getUpdateCount();
	void getMoreResults();
	void getIndexInfo();
	void getImportedKeys();
	void getPrimaryKeys();
	void getColumns();
	void getTables();
	void getDatabaseMetaData();
	void shutdownAgent(bool panic);
	void agentShutdown (Server *server);
	void genConnectionHtml();
	void executePreparedUpdate();
	void setCursorName();
	void prepareDrl();
	void getDrlResultSet();
	void executeQuery();
	void fetchRecord();
	void nextHit();
	void executePreparedStatement();
	void dispatch (MsgType type);
	void getResultSet();
	void search();
	void executeUpdate();
	void execute();
	void createStatement();
	void closeConnection();
	void rollbackTransaction();
	void prepareTransaction();
	void commitTransaction();
	void next();
	void getMetaData();
	void sendResultSet (ResultSet *resultSet);
	void executePreparedQuery();
	void prepareStatement();
	void agentLoop();
	void startAgent();
	static void agentLoop (void *lpParameter);
	static void getConnections (void *lpParameter);
	void getConnections();

	virtual void shutdown (bool panic);
	virtual int waitForExit();
	virtual void release();
	virtual bool isActive();

	Server(int port, const char *configFile);
	Server(Server *server, Protocol *proto);

protected:
	virtual ~Server();

	SyncObject	syncObject;
	Connection	*connection;
	Protocol	*protocol;
	Thread		*thread;
	Thread		*waitThread;
	Threads		*threads;
	Server		*parent;
	Socket		*serverSocket;
	Protocol	*logSocket;
	Protocol	*acceptLogSocket;
	Server		*activeServers;
	Server		*nextServer;
	Configuration	*configuration;
	char		*string1;
	char		*string2;
	char		*string3;
	char		*string4;
	char		*string5;
	char		*string6;
	int			useCount;
	int			port;
	bool		active;
	bool		first;
	bool		forceShutdown;
	bool		panicMode;
	volatile bool	serverShutdown;
	//int32		ipAddress;
};

#endif // !defined(AFX_SERVER_H__84FD1985_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
