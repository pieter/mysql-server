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

// ThreadQueue.h: interface for the ThreadQueue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREADQUEUE_H__23686698_223F_4B72_AD3E_BF71D8A51573__INCLUDED_)
#define AFX_THREADQUEUE_H__23686698_223F_4B72_AD3E_BF71D8A51573__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Thread;

class ThreadQueue  
{
public:
	bool isMember (Thread *candidate);
	void remove (Thread *thread);
	void insert (Thread *thread);
	ThreadQueue();
	virtual ~ThreadQueue();

	Thread	*first;
	Thread	*last;
};

#endif // !defined(AFX_THREADQUEUE_H__23686698_223F_4B72_AD3E_BF71D8A51573__INCLUDED_)
