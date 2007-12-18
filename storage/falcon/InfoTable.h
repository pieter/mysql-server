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

#ifndef _INFO_TABLE_H_
#define _INFO_TABLE_H_

#ifdef _WIN32
typedef __int64 INT64;
#else
typedef long long INT64;
#endif

class THD;
struct charset_info_st;
struct TABLE_LIST;
struct st_table;

class InfoTable
{
public:
	virtual         ~InfoTable() {;}
    virtual void    putRecord(void) = 0;
    virtual void    putInt(int column, int value) = 0;
    virtual void    putInt64(int column, INT64 value) = 0;
    virtual void    putDouble(int column, double value) = 0;
    virtual void    putString(int column, const char *string) = 0;
    virtual void    putString(int column, unsigned int stringLength, const char *string) = 0;
};

class InfoTableImpl : public InfoTable
{
public:
    InfoTableImpl(THD *thd, TABLE_LIST *tables, charset_info_st *scs);
    virtual ~InfoTableImpl(void);
    
    virtual void    putRecord(void);
    virtual void    putInt(int column, int value);
    virtual void    putInt64(int column, INT64 value);
    virtual void    putString(int column, const char *string);
    virtual void    putString(int column, unsigned int stringLength, const char *string);
    virtual void    putDouble(int column, double value);
    
    int             error;
    st_table        *table;
    THD             *mySqlThread;
    charset_info_st *charSetInfo;
};

#endif
