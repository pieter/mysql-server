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

// Parameter.cpp: implementation of the Parameter class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "Parameter.h"
#include "JString.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Parameter::Parameter(Parameter *nxt, const char *nam, int namLen, const char *val, int valLen)
{
	next = nxt;
	nameLength = namLen;
	valueLength = valLen;
	name = new char [nameLength + valueLength + 2];
	memcpy (name, nam, nameLength);
	name [nameLength] = 0;
	value = name + nameLength + 1;
	memcpy (value, val, valueLength);
	value [valueLength] = 0;
	collision = NULL;
}

Parameter::~Parameter()
{
	delete [] name;
}

void Parameter::setValue(const char *newValue)
{
	if (!strcmp (value, newValue))
		return;

	char *oldName = name;
	valueLength = (int) strlen (newValue);
	name = new char [nameLength + valueLength + 2];
	memcpy (name, oldName, nameLength);
	name [nameLength] = 0;
	value = name + nameLength + 1;
	memcpy (value, newValue, valueLength);
	value [valueLength] = 0;
	delete [] oldName;
}

bool Parameter::isNamed(const WCString *parameter)
{
	if (nameLength != parameter->count)
		return false;

	for (int n = 0; n < nameLength; ++n)
		if (name [n] != parameter->string [n])
			return false;

	return true;
}
