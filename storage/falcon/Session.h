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

// Session.h: interface for the Session class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SESSION_H__E4859521_3879_11D3_AB77_0000C01D2301__INCLUDED_)
#define AFX_SESSION_H__E4859521_3879_11D3_AB77_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


class Database;
class Application;
class SessionManager;
class LicenseToken;
class Thread;
class SessionQueue;

class _jobject;

class Session  
{
public:
	void release();
	void addRef();
	void closed();
	LicenseToken* surrenderLicenseToken(bool voluntary);
	void deactivate();
	bool activate(bool wait);
	void zap();
	void close();
	Session(SessionManager *manager, JString id, Application *app);
	virtual ~Session();

public:
	void releaseLicense();
	void purged();
	JString			sessionId;
	//JString			clientId;
	Session			*collision;
	Session			*next;
	Session			*prior;
	SessionQueue	*sessionQueue;
	Application		*application;
	SessionManager	*sessionManager;
	_jobject		*sessionObject;
	bool			zapped;
	bool			active;
	LicenseToken	*licenseToken;
	int32			expiration;
	Thread			*thread;
	volatile INTERLOCK_TYPE	useCount;
};

#endif // !defined(AFX_SESSION_H__E4859521_3879_11D3_AB77_0000C01D2301__INCLUDED_)
