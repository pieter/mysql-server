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

// Parameters.h: interface for the Parameters class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#if !defined(AFX_PARAMETERS_H__13461881_1D25_11D4_98DF_0000C01D2301__INCLUDED_)
#define AFX_PARAMETERS_H__13461881_1D25_11D4_98DF_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

START_NAMESPACE
#include "Properties.h"
END_NAMESPACE

class Parameter;

class Parameters : public Properties
{
public:
	void setValue (const char *name, const char *value);
	virtual int getValueLength (int index);
	void clear();
	void copy (Properties *properties);
	virtual const char* getValue (int index);
	virtual const char* getName (int index);
	virtual int getCount();
	virtual const char* findValue (const char *name, const char *defaultValue);
	virtual void putValue(const char * name, int nameLength, const char * value, int valueLength);
	virtual void putValue(const char * name, const char * value);
	Parameters();
	virtual ~Parameters();

	Parameter	*parameters;
	int			count;
};

#endif // !defined(AFX_PARAMETERS_H__13461881_1D25_11D4_98DF_0000C01D2301__INCLUDED_)
