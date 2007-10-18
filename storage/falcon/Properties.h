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


#ifndef _PROPERTIES_H_
#define _PROPERTIES_H_

class Properties  
{
public:
	Properties() {};
	virtual const char * findValue (const char *name, const char *defaultValue) = 0;
	virtual void putValue (const char *name, const char *value) = 0;
	virtual void putValue (const char *name, int nameLength, const char *value, int valueLength) = 0;
	virtual int	 getCount () = 0;
	virtual const char *getName (int index) = 0;
	virtual const char *getValue (int index) = 0;
	virtual ~Properties() {};
	virtual int getValueLength (int index) = 0;
};

#endif
