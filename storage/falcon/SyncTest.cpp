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

#include <stdio.h>
#include "Engine.h"
#include "SyncTest.h"
#include "Sync.h"
#include "Thread.h"
#include "Threads.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

SyncTest::SyncTest(void) : Thread("SyncTest")
{
	threads = NULL;
}

SyncTest::~SyncTest(void)
{
	useCount = 0;
	delete [] threads;
}

void SyncTest::test()
{
	if (!threads)
		threads = new SyncTest[MAX_THREADS];
	
	Sync sync(&starter, "SyncTest::test");
	Threads *threadBarn = new Threads(NULL, MAX_THREADS);
		
	for (int n = 1; n <= MAX_THREADS; ++n)
		{
		sync.lock(Exclusive);
		stop = false;
		int thd;
		
		for (thd = 0; thd < n; ++thd)
			{
			SyncTest *thread = threads + thd;
			thread->parent = this;
			thread->ready = false;
			threadBarn->start("", testThread, thread);
			}
		
		for(;;)
			{
			bool waiting = false;
			
			for (thd = 0; thd < n; ++thd)
				if (!threads[thd].ready)
					{
					waiting = true;
					break;
					}
			
			if (!waiting)
				break;
				
			Thread::sleep(100);
			}
			
		sync.unlock();
		Thread::sleep(1000);
		stop = true;
		threadBarn->waitForAll();
		int total = 0;
		
		for (thd = 0; thd < n; ++thd)
			total += threads[thd].count;
		
		printf("%d threads, %d cycles:", n, total);

		for (thd = 0; thd < n; ++thd)
			printf(" %d", threads[thd].count);
					
		printf("\n");
		}
	
	threadBarn->shutdownAll();
	threadBarn->waitForAll();
	threadBarn->release();
}

void SyncTest::testThread(void* parameter)
{
	((SyncTest*) parameter)->testThread();
}

void SyncTest::testThread(void)
{
	count = 0;
	Sync syncStart(&starter, "SyncTest::thread");
	ready = true;
	syncStart.lock(Shared);
	Sync sync(&parent->syncObject, "SyncTest::thread");
	
	while (!parent->stop)
		{
		++count;
		sync.lock(Shared);
		sync.unlock();
		}
}
