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

// Queue.h: interface for the Queue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_QUEUE_H__1F89E5DB_3459_4538_95BB_7F9E139F1DF8__INCLUDED_)
#define AFX_QUEUE_H__1F89E5DB_3459_4538_95BB_7F9E139F1DF8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

template <class T>
class Queue
{
public:
	Queue()
		{
		first = NULL;
		last = NULL;
		count = 0;
		};

	~Queue()
		{
		/***
		for (T *object; object = first;)
			{
			first = object->next;
			delete object;
			}
		***/
		};

	void prepend (T *object)
		{
		if ( (object->next = first) )
			first->prior = object;
		else
			last = object;

		object->prior = NULL;
		first = object;
		++count;
		};

	void append (T *object)
		{
		if ( (object->prior = last) )
			last->next = object;
		else
			first = object;

		object->next = NULL;
		last = object;
		++count;
		};

	void appendAfter (T *object, T *item)
		{
		if ( (object->next = item->next) )
			object->next->prior = object;
		else
			last = object;
		
		item->next = object;
		object->prior = item;
		++count;
		}
		
	void remove (T *object)
		{
		if (object->next)
			object->next->prior = object->prior;
		else
			last = object->prior;

		if (object->prior)
			object->prior->next = object->next;
		else
			first = object->next;

		--count;
		};

	T			*first;
	T			*last;
	SyncObject	syncObject;
	int			count;
};

#endif // !defined(AFX_QUEUE_H__1F89E5DB_3459_4538_95BB_7F9E139F1DF8__INCLUDED_)
