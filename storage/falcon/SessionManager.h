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

// SessionManager.h: interface for the SessionManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SESSIONMANAGER_H__E4859522_3879_11D3_AB77_0000C01D2301__INCLUDED_)
#define AFX_SESSIONMANAGER_H__E4859522_3879_11D3_AB77_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Synchronize.h"
#include "SyncObject.h"
#include "SessionQueue.h"

#define SESSION_HASH_SIZE	101

class Database;
class User;
class QueryString;
class Session;
class TemplateContext;
class Application;
class Connection;
class Java;
class JavaEnv;
class JavaNative;
class Stream;
class Client;
class LicenseToken;
class LicenseProduct;
class DateTime;

class _jobject;
class _jclass;
struct JNIEnv_;

typedef JNIEnv_ JNIEnv;

struct _jmethodID;


class SessionManager : public Synchronize 
{
public:
	void release (JavaNative *javaNative, _jobject* object);
	void scheduled (Connection *connection, Application *application, const char *eventName);
	void scavenge (DateTime *now);
	_jobject* getSessionObject (JavaNative *javaNative, Session *session, _jobject *contextObject);
	int initiateService (Connection *connection, Session *session, const char *serviceName);
	void insertPending (Session *session);
	LicenseToken* getLicenseToken (Session *target);
	void zapLinkages();
	Session* findSession(Application *application, TemplateContext *templateContext, const char *cookie);
	void execute (Connection *connection, Application *application, TemplateContext *context, Stream *stream);
	void start (Connection *connection);
	Session* createSession (Application *application);
	void close (Session *session);
	SessionManager(Database *db);
	virtual ~SessionManager();

protected:
	Session* findSession (Application * application, const char *sessionId);
	Session		*hashTable [SESSION_HASH_SIZE];
	SyncObject	syncObject;

public:
	Session* createSession (Application *application, JString sessionId);
	Session* getSpecialSession (Application *application, TemplateContext *context);
	LicenseToken* waitForLicense (Session *target);
	void removeSession (Session *session);
	void purged (Session *session);
	Database	*database;
	Java		*java;
	User		*user;
	int			nextSessionId;
	LicenseProduct *licenseProduct;
	SessionQueue pending;
	SessionQueue waiting;
	_jclass		*sessionManagerClass;
	_jclass		*connectionClass;
	_jclass		*templateContextClass;
	_jclass		*queryStringClass;
	//_jclass		*streamClass;
	_jmethodID	*connectionInit;
	_jmethodID	*connectionClose;
	_jmethodID	*createSessionMethod;
	_jmethodID	*closeSessionMethod;
	_jmethodID	*executeSessionMethod;
	_jmethodID	*executeInitiateService;
	_jmethodID	*scheduledMethod;
	_jmethodID	*templateContextInit;
	_jmethodID	*queryStringInit;
	//_jmethodID	*streamInit;
};

#endif // !defined(AFX_SESSIONMANAGER_H__E4859522_3879_11D3_AB77_0000C01D2301__INCLUDED_)
