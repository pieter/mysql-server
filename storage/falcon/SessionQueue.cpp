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

// SessionQueue.cpp: implementation of the SessionQueue class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SessionQueue.h"
#include "Session.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SessionQueue::SessionQueue()
{
	first = NULL;
	last = NULL;
}

SessionQueue::~SessionQueue()
{
}

void SessionQueue::insert(Session *session)
{
	if (session->sessionQueue)
		session->sessionQueue->remove (session);

	session->sessionQueue = this;
	session->prior = last;
	session->next = NULL;

	if (last)
		last->next = session;
	else
		first = session;

	last = session;
}

void SessionQueue::remove(Session *session)
{
	ASSERT (session->sessionQueue == this);

	if (session->prior)
		session->prior->next = session->next;
	else
		first = session->next;

	if (session->next)
		session->next->prior = session->prior;
	else
		last = session->prior;

	session->sessionQueue = NULL;
}

void SessionQueue::moveToEnd(Session *session)
{
	ASSERT (session->sessionQueue == this);

	if (last == session)
		return;

	// First, remove it

	if (session->prior)
		session->prior->next = session->next;
	else
		first = session->next;

	if (session->next)
		session->next->prior = session->prior;
	else
		last = session->prior;

	// Then re-insert it

	session->prior = last;
	session->next = NULL;

	if (last)
		last->next = session;
	else
		first = last = session;

}
