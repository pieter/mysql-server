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


#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <string.h>
#include "mysql_priv.h"
#include "InfoTable.h"

InfoTableImpl::InfoTableImpl(THD *thd, TABLE_LIST *tables, charset_info_st *scs)
{
	table = tables->table;
	mySqlThread = thd;
	charSetInfo = scs;
	error = 0;
}

InfoTableImpl::~InfoTableImpl(void)
{
}

void InfoTableImpl::putRecord(void)
{
	error = schema_table_store_record(mySqlThread, table);
}

void InfoTableImpl::putInt(int column, int value)
{
	table->field[column]->store(value, false);
}

void InfoTableImpl::putString(int column, const char *string)
{
	table->field[column]->store(string, strlen(string), charSetInfo);
}

void InfoTableImpl::putInt64(int column, INT64 value)
{
	table->field[column]->store(value, false);
}

void InfoTableImpl::putDouble(int column, double value)
{
	table->field[column]->store(value);
}

void InfoTableImpl::putString(int column, unsigned int stringLength, const char *string)
{
	table->field[column]->store(string, stringLength, charSetInfo);
}
