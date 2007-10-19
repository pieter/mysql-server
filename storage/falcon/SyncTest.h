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


#ifndef _SYNC_TEST_H_
#define _SYNC_TEST_H_

#include "Thread.h"
#include "SyncObject.h"

static const int MAX_THREADS	= 10;

class SyncTest : public Thread
{
public:
	SyncTest(void);
	~SyncTest(void);
	
	SyncTest	*threads;
	static void testThread(void* parameter);
	void		test();
	void		testThread(void);
	
	bool		stop;
	bool		ready;
	int			count;
	int			lockCollisions;
	int			unlockCollisions;
	SyncTest	*parent;
	SyncObject	starter;
	SyncObject	syncObject;
	
};

#endif
