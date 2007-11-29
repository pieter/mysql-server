/* Copyright (C) 2007 MySQL AB

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

#ifndef _PRIORITY_H_
#define _PRIORITY_H_

static const int PRIORITY_LOW		= 1;
static const int PRIORITY_MEDIUM	= 2;
static const int PRIORITY_HIGH		= 3;
static const int PRIORITY_MAX		= 4;

class PriorityScheduler;

class Priority
{
public:
	Priority(PriorityScheduler *sched);
	~Priority(void);
	
	void	schedule(int priority);
	void	finished(void);
	
	PriorityScheduler	*scheduler;
	int					priority;
};

#endif
