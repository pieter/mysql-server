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


#ifndef _SYNCHRONIZATIONOBJECT_H_
#define _SYNCHRONIZATIONOBJECT_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum LockType {
	None,
	Exclusive,
	Shared,
	Invalid
	};

class LinkedList;
class Sync;

class SynchronizationObject
{
public:
	SynchronizationObject(void) {};
	virtual ~SynchronizationObject(void) {}
	
	virtual void unlock (Sync *sync, LockType type) = 0;
	virtual void lock (Sync *sync, LockType type, int timeout) = 0;
	virtual void findLocks (LinkedList &threads, LinkedList& syncObjects) = 0;
};

#endif
