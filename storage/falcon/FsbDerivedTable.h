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

#ifndef _FSB_DERIVED_TABLE_H_
#define _FSB_DERIVED_TABLE_H_

#include "Fsb.h"

class NSelect;

class FsbDerivedTable : public Fsb  
{
public:
	FsbDerivedTable(NSelect *node);
	~FsbDerivedTable(void);
	
	virtual void open(Statement* statement);
	virtual Row* fetch(Statement* statement);
	virtual void close(Statement* statement);
	
	NSelect		*select;
	virtual const char* getType(void);
	virtual void prettyPrint(int level, PrettyPrint* pp);
};

#endif
