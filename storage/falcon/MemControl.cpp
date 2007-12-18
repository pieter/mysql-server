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

#include "Engine.h"
#include "MemControl.h"
#include "MemMgr.h"

MemControl::MemControl(void)
{
	count = 0;
}

MemControl::~MemControl(void)
{
}

bool MemControl::poolExtensionCheck(uint size)
{
	uint64 inUse = size;
	
	for (int n = 0; n < count; ++n)
		inUse += pools[n]->currentMemory;
	
	return inUse < maxMemory;
}

void MemControl::addPool(MemMgr* pool)
{
	pools[count++] = pool;
	pool->memControl = this;
}

void MemControl::setMaxSize(uint64 size)
{
	maxMemory = size;
}
