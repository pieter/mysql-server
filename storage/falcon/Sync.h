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

// Sync.h: interface for the Sync class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SYNC_H__59333A55_BC53_11D2_AB5E_0000C01D2301__INCLUDED_)
#define AFX_SYNC_H__59333A55_BC53_11D2_AB5E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SynchronizationObject.h"

class LinkedList;

class Sync  
{
public:
	Sync(SynchronizationObject *obj, const char *where);
	virtual ~Sync();

	void	print (const char* label);
	void	findLocks (LinkedList &threads, LinkedList& syncObjects);
	//void	lock (LockType type, const char *fromWhere);
	//void	print(int level);
	void	setObject (SynchronizationObject *obj);
	void	unlock();
	void	lock (LockType type);
	void	lock(LockType type, int timeout);

	SynchronizationObject	*syncObject;
	LockType	state;
	LockType	request;
	Sync		*prior;
	const char	*where;
	const char	*marked;
	void mark(const char* text);
};

#endif // !defined(AFX_SYNC_H__59333A55_BC53_11D2_AB5E_0000C01D2301__INCLUDED_)
