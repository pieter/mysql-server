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

// Session.cpp: implementation of the Session class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Session.h"
#include "Database.h"
#include "SessionManager.h"
#include "Interlock.h"
#include "LicenseToken.h"
#include "SQLError.h"

//#define SESSION_DURATION	120
#define SESSION_DURATION	10

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Session::Session(SessionManager *manager, JString id, Application *app)
{
	sessionManager = manager;
	sessionId = id;
	application = app;
	sessionObject = NULL;
	zapped = false;
	active = false;
	licenseToken = NULL;
	expiration = 0;
	sessionQueue = NULL;
	useCount = 1;
}

Session::~Session()
{
}

void Session::close()
{
	if (sessionManager)
		{
		sessionManager->close (this);
		sessionManager = NULL;
		}

	delete this;
}

void Session::zap()
{
	zapped = true;
	sessionObject = NULL;
}

bool Session::activate(bool wait)
{
	if (!sessionManager)
		throw SQLEXCEPTION (RUNTIME_ERROR, "session has been closed");

	if (!licenseToken &&
		!(licenseToken = sessionManager->getLicenseToken ((wait) ? this : NULL)))
		return false;

	active = true;
	expiration = sessionManager->database->timestamp + SESSION_DURATION;
	sessionManager->insertPending (this);

	return true;
}

void Session::deactivate()
{
	active = false;
}

LicenseToken* Session::surrenderLicenseToken(bool voluntary)
{
	if (voluntary)
		{
		if (active)
			return NULL;
		if (expiration > sessionManager->database->timestamp)
			return NULL;
		}

	LicenseToken *token = licenseToken;
	licenseToken = NULL;

	return token;
}


void Session::closed()
{
	active = false;
	releaseLicense();
}

void Session::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Session::release()
{
	ASSERT (useCount > 0);

	if (INTERLOCKED_DECREMENT (useCount) == 0)
		close();
}

void Session::purged()
{
	deactivate();
	releaseLicense();

	if (sessionManager)
		sessionManager->purged (this);
}

void Session::releaseLicense()
{
	if (licenseToken)
		{
		licenseToken->release();
		licenseToken = NULL;
		}
}
