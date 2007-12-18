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

// SessionManager.cpp: implementation of the SessionManager class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <memory.h>
#include <time.h>
#include "Engine.h"
#include "SessionManager.h"
#include "Session.h"
#include "QueryString.h"
#include "TemplateContext.h"
#include "JavaVM.h"
#include "Java.h"
#include "JavaNative.h"
#include "JavaEnv.h"
#include "JavaThread.h"
#include "JavaObject.h"
#include "Database.h"
#include "Application.h"
#include "Connection.h"
#include "Sync.h"
#include "Thread.h"
#include "Log.h"
#include "SQLException.h"

#ifdef LICENSE
#include "LicenseManager.h"
#include "LicenseProduct.h"
#endif

#define CONNECTION		"netfrastructure/sql/NfsConnection"
#define APPLICATION		"netfrastructure/model/Application"
#define QUERYSTRING		"netfrastructure/model/QueryString"
#define TEMPLATECONTEXT	"netfrastructure/model/TemplateContext"
#define SESSIONMANAGER	"netfrastructure/model/SessionManager"
#define STRING			"java/lang/String"
#define SIG(sig)		"L" sig ";"
#define SPACE_THREASHOLD	10000
#define SPECIAL_SESSION	"special"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SessionManager::SessionManager(Database *db)
{
	database = db;
	java = database->java;
	nextSessionId = 0;
	memset (hashTable, 0, sizeof (hashTable));
	connectionClass = NULL;
	licenseProduct = NULL;
	executeInitiateService = NULL;
	scheduledMethod = NULL;
	}

SessionManager::~SessionManager()
{
	Session *session;

	for (int n = 0; n < SESSION_HASH_SIZE; ++n)
		while (session = hashTable [n])
			{
			hashTable [n] = session->collision;
			session->close();
			}

}

void SessionManager::close(Session * session)
{
	removeSession (session);
}

Session* SessionManager::createSession(Application *application)
{
	JString sessionId;
	sessionId.Format ("%d:%d", time (NULL), ++nextSessionId);

	return createSession (application, sessionId);
}


Session* SessionManager::createSession(Application *application, JString sessionId)
{
	if (!connectionClass)
		start (database->systemConnection);

	Sync sync (&syncObject, "SessionManager::createSession");
	sync.lock (Exclusive);
	Session *session = new Session (this, sessionId, application);
	int slot = sessionId.hash (SESSION_HASH_SIZE);
	session->collision = hashTable [slot];
	hashTable [slot] = session;

	return session;
}

void SessionManager::start(Connection * connection)
{
#ifdef LICENSE
	licenseProduct = database->licenseManager->getProduct (SERVER_PRODUCT);
#endif

	JavaNative jni ("SessionManager::start", java->javaEnv);
	connectionClass = jni.FindClass (CONNECTION);
	connectionInit = jni.GetMethodID (connectionClass, "<init>", "()V");
	connectionClose = jni.GetMethodID (connectionClass, "close", "()V");
	jobject connectionObject = java->wrapConnection (&jni, connection);

	sessionManagerClass = jni.FindClass (SESSIONMANAGER);
	jmethodID method = jni.GetStaticMethodID (sessionManagerClass, "initialize", "(L" CONNECTION ";)V");
	jni.CallStaticVoidMethod (sessionManagerClass, method, connectionObject);

	if (jni.ExceptionOccurred())
		java->throwCException (&jni);

	executeSessionMethod = jni.GetStaticMethodID (sessionManagerClass, "execute", 
		"(" 
		SIG (CONNECTION) 
		SIG (TEMPLATECONTEXT) 
		SIG (QUERYSTRING) 
		SIG (APPLICATION) 
		")V");

	createSessionMethod = jni.GetStaticMethodID (sessionManagerClass, "createSession", 
		"(" 
		SIG (STRING) 
		SIG (STRING) 
		SIG (TEMPLATECONTEXT) 
		")" SIG (APPLICATION));

	closeSessionMethod = jni.GetStaticMethodID (sessionManagerClass, "closeSession", 
		"(" 
		SIG (STRING) 
		")V");

	templateContextClass = jni.FindClass (TEMPLATECONTEXT);
	if (!templateContextClass) java->throwCException (&jni);
	templateContextInit = jni.GetMethodID (templateContextClass, "<init>", "()V");
	if (!templateContextInit) java->throwCException (&jni);

	queryStringClass = jni.FindClass (QUERYSTRING);
	if (!queryStringClass) java->throwCException (&jni);
	queryStringInit = jni.GetMethodID (queryStringClass, "<init>", "(" SIG (STRING) ")V");
	if (!queryStringInit) java->throwCException (&jni);
}

void SessionManager::release(JavaNative *javaNative, _jobject *object)
{
	javaNative->CallVoidMethod (object, connectionClose);
}

void SessionManager::execute(Connection *connection, Application *application, TemplateContext *templateContext, Stream * stream)
{
	if (java->checkClassReload())
		java->reloadClasses();

	if (!connectionClass)
		start (database->systemConnection);

	JavaNative jni ("SessionManager::execute", java->javaEnv);
	int32 objectsAllocated = jni.thread->objectsAllocated;
	int32 spaceAllocated = jni.thread->spaceAllocated;

	// Encapsulate Query String, TemplateContext

	QueryString *queryString = templateContext->queryString;
	queryString->addRef();
	jobject queryObject = jni.NewObject (queryStringClass, queryStringInit, jni.NewStringUTF (queryString->queryString));
	jobject exception = jni.ExceptionOccurred();

	if (exception)
		java->throwCException (&jni, exception);

	jni.setExternalValue (queryObject, queryString);
	templateContext->addRef();
	jobject contextObject =	java->createExternalObject (&jni, templateContextClass, templateContextInit, templateContext);

	_jobject* javaConnection = java->wrapConnection (&jni, connection);

	jni.CallStaticVoidMethod (sessionManagerClass, executeSessionMethod,
		javaConnection,
		contextObject,
		queryObject,
		getSessionObject (&jni, templateContext->session, contextObject));

	exception = jni.ExceptionOccurred();
	release (&jni, javaConnection);

	if (exception)
		java->throwCException (&jni, exception);

	objectsAllocated = jni.thread->objectsAllocated - objectsAllocated;
	spaceAllocated = jni.thread->spaceAllocated - spaceAllocated;

	if (spaceAllocated > SPACE_THREASHOLD)
		Log::log (LogLog | LogScrub,
				  "step (%d) allocated %d java objects, %d bytes for %s\n", 
				  templateContext->stepNumber, objectsAllocated, spaceAllocated, queryString->queryString);
}

Session* SessionManager::findSession(Application * application, TemplateContext * templateContext, const char *cookie)
{
	QueryString *queryString = templateContext->queryString;
	Sync sync (&syncObject, "SessionManager::findSession");
	sync.lock (Shared);
	
	if (cookie)
		{
		const char *p = cookie;
		const char *appName = application->name;
		while (*p)
			{
			const char *q = appName;
			for (; *q && *p && *q == *p; ++p, ++q)
				;
			if (*q == 0 && *p++ == '=')
				{
				char sessionId [32];
				char *s = sessionId;
				while (*p && *p != ';' && s < sessionId + sizeof (sessionId) - 1)
					*s++ = *p++;
				*s = 0;
				Session *session = findSession (application, sessionId);
				if (session)
					{
					templateContext->noSessionId = true;
					return session;
					}
				}
			while (*p && *p++ != ';')
				;
			while (*p == ' ')
				++p;
			}
		}

	const char *sessionId = queryString->getParameter (PARM_SESSION, NULL);

	if (sessionId)
		return findSession (application, sessionId);

	return NULL;
}

Session* SessionManager::findSession(Application * application, const char *sessionId)
{
	int slot = JString::hash (sessionId, SESSION_HASH_SIZE);

	for (Session *session = hashTable [slot]; session; session = session->collision)
		if (session->application == application && session->sessionId == sessionId)
			return session;

	return NULL;
}

void SessionManager::zapLinkages()
{
	Sync sync (&syncObject, "SessionManager::zapLinkages");
	sync.lock (Exclusive);
	connectionClass = NULL;
	scheduledMethod = NULL;
	createSessionMethod = NULL;
	executeInitiateService = NULL;

	for (int n = 0; n < SESSION_HASH_SIZE; ++n)
		for (Session *session = hashTable [n]; session; session = session->collision)
			session->zap();
}

LicenseToken* SessionManager::getLicenseToken(Session *target)
{
#ifdef LICENSE
	// If we've got one to issue, just do it

	LicenseToken *licenseToken = licenseProduct->getToken();

	if (licenseToken)
		return licenseToken;

#endif

	return NULL;
}

void SessionManager::insertPending(Session *session)
{
	Sync sync (&syncObject, "SessionManager::insertPending");
	sync.lock (Exclusive);
	pending.insert (session);
}

int SessionManager::initiateService(Connection *connection, Session *session, const char *serviceName)
{
	if (java->checkClassReload())
		java->reloadClasses();

	if (!connectionClass)
		start (database->systemConnection);

	JavaNative jni ("SessionManager::initiateService", java->javaEnv);
	int32 objectsAllocated = jni.thread->objectsAllocated;
	int32 spaceAllocated = jni.thread->spaceAllocated;

	if (!executeInitiateService)
		executeInitiateService = jni.GetStaticMethodID (sessionManagerClass, "initiateService", 
			"(" 
			SIG (CONNECTION) 
			SIG (APPLICATION) 
			SIG (STRING) 
			")I");


	_jobject* javaConnection = java->wrapConnection (&jni, connection);

	int port = jni.CallStaticIntMethod (sessionManagerClass, executeInitiateService,
		javaConnection,
		getSessionObject (&jni, session, NULL),
		jni.NewStringUTF (serviceName));

	if (jni.ExceptionOccurred())
		java->throwCException (&jni);

	objectsAllocated = jni.thread->objectsAllocated - objectsAllocated;
	spaceAllocated = jni.thread->spaceAllocated - spaceAllocated;

	if (spaceAllocated > SPACE_THREASHOLD)
		Log::log ("service allocated %d java objects, %d bytes\n", 
				  objectsAllocated, spaceAllocated);

	return port;
}

_jobject* SessionManager::getSessionObject(JavaNative *javaNative, Session *session, _jobject *contextObject)
{
	if (session->sessionObject)
		return session->sessionObject;

	session->sessionObject = 
		javaNative->CallStaticObjectMethod (sessionManagerClass, createSessionMethod, 
							javaNative->NewStringUTF (session->application->name),
							javaNative->NewStringUTF (session->sessionId),
							contextObject);

	jobject exception = javaNative->ExceptionOccurred();
	if (exception) java->throwCException (javaNative, exception);

	javaNative->setExternalValue (session->sessionObject, session);
	session->addRef();

	return session->sessionObject;
}


void SessionManager::scavenge(DateTime *now)
{

}

void SessionManager::scheduled(Connection *connection, Application *application, const char *eventName)
{
	try
		{
		if (java->checkClassReload())
			java->reloadClasses();

		Session *session = createSession (application);
		JavaNative jni ("SessionManager::scheduled", java->javaEnv);
		int32 objectsAllocated = jni.thread->objectsAllocated;
		int32 spaceAllocated = jni.thread->spaceAllocated;

		if (!scheduledMethod)
			{
			scheduledMethod = jni.GetStaticMethodID (sessionManagerClass, "scheduled", 
				"(" 
				SIG (APPLICATION) 
				SIG (CONNECTION) 
				SIG (STRING) 
				SIG (TEMPLATECONTEXT) 
				")V");
			if (!scheduledMethod)
				{
				Log::log ("can't find \"ScheduleManager.scheduled\"");
				close (session);
				return;
				}
			};

		jobject javaConnection = java->wrapConnection (&jni, connection);
		TemplateContext *templateContext = new TemplateContext (connection);
		templateContext->application = application;
		jobject contextObject =	java->createExternalObject (&jni, templateContextClass, templateContextInit, templateContext);
		jni.CallStaticVoidMethod (sessionManagerClass, scheduledMethod,
			getSessionObject (&jni, session, NULL),
			javaConnection,
			jni.NewStringUTF (eventName),
			contextObject);

		jobject exception = jni.ExceptionOccurred();
		release (&jni, javaConnection);
		session->sessionObject = 
			jni.CallStaticObjectMethod (sessionManagerClass, closeSessionMethod, 
										jni.NewStringUTF (session->sessionId));
		session->release();

		if (exception)
			java->throwCException (&jni, exception);

		objectsAllocated = jni.thread->objectsAllocated - objectsAllocated;
		spaceAllocated = jni.thread->spaceAllocated - spaceAllocated;

		if (spaceAllocated > SPACE_THREASHOLD)
			Log::log ("scheduled event %s allocated %d java objects, %d bytes\n", 
					  eventName, objectsAllocated, spaceAllocated);

		}
	catch (SQLException &error)
		{
		Log::log ("Exception during scheduled event: %s\n", (const char*) error.getText());
		const char *stackTrace = error.getTrace();
		if (stackTrace && stackTrace [0])
			Log::log (stackTrace);
		throw;
		}
}


void SessionManager::purged(Session *session)
{
	removeSession (session);
}

void SessionManager::removeSession(Session *session)
{
	Sync sync (&syncObject, "SessionManager::close");
	sync.lock (Exclusive);

	// If it's in a queue, take it out now

	if (session->sessionQueue)
		session->sessionQueue->remove (session);

	// Take session of out sessionId hash table

	bool hit = false;

	for (Session **ptr = hashTable + session->sessionId.hash (SESSION_HASH_SIZE);
		 *ptr; ptr = &(*ptr)->collision)
		if (*ptr == session)
			{
			*ptr = session->collision;
			hit = true;
			break;
			}

	//session->release();
}

LicenseToken* SessionManager::waitForLicense(Session *target)
{
	LicenseToken *licenseToken;

	// If somebody else if waiting, and we're unwilling to
	// wait, just say no.

	Sync sync (&syncObject, "SessionManager::getLicenseToken");
	sync.lock (Exclusive);

	if (waiting.first && !target)
		return NULL;

	// We're serious about this.  Link into queue first

	if (target)
		{
		target->thread = Thread::getThread("SessionManager::getLicenseToken");
		waiting.insert (target);
		}

	for (;;)
		{
		int wait = 10;
		if (target == waiting.first)
			for (Session *session = pending.first; session;  session = session->next)
				{
				if (licenseToken = session->surrenderLicenseToken (true))
					{
					pending.remove (session);
					if (target)
						waiting.remove (target);
					if (waiting.first)
						{
						Thread *thread = waiting.first->thread;
						thread->licenseWakeup = true;
						thread->wake();
						}
					return licenseToken;
					}
				if (!session->active)
					{
					int wt = session->expiration - database->timestamp;
					wt = MAX (wt, 0);
					wait = MIN (wt, wait);
					}
				}
		if (!target)
			{
			if (waiting.first)
				{
				Thread *thread = waiting.first->thread;
				thread->licenseWakeup = true;
				thread->wake();
				}
			return NULL;
			}
		if (target == waiting.first)
			{
			sync.unlock();
			sleep (wait * 1000);
			}
		else
			{
			Thread *thread = target->thread;
			thread->licenseWakeup = false;
			sync.unlock();
			while (!thread->licenseWakeup)
				thread->sleep();
			}
		sync.lock (Exclusive);
		}
}

Session* SessionManager::getSpecialSession(Application *application, TemplateContext *context)
{
	Session *session = findSession (application, SPECIAL_SESSION);

	if (session)
		return session;

	return createSession (application, SPECIAL_SESSION);
}
