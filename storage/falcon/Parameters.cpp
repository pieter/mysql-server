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

// Parameters.cpp: implementation of the Parameters class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#include <string.h>
#include "Engine.h"
#include "Parameters.h"
#include "Parameter.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Parameters::Parameters()
{
	parameters = NULL;
	count = 0;
}

Parameters::~Parameters()
{
	clear();
}

void Parameters::putValue(const char * name, const char * value)
{
	putValue (name, (int) strlen (name), value, (int) strlen (value));
}

void Parameters::putValue(const char * name, int nameLength, const char * value, int valueLength)
{
	++count;
	parameters = new Parameter (parameters, name, nameLength, value, valueLength);
}

const char* Parameters::findValue(const char * name, const char *defaultValue)
{
	for (Parameter *parameter = parameters; parameter; parameter = parameter->next)
		if (!strcasecmp (name, parameter->name))
			return parameter->value;

	return defaultValue;
}

int Parameters::getCount()
{
	return count;
}

const char* Parameters::getName(int index)
{
	Parameter *parameter = parameters;

	for (int n = 0; n < count; ++n, parameter = parameter->next)
		if (n == index)
			return parameter->name;

	return NULL;
}


const char* Parameters::getValue(int index)
{
	Parameter *parameter = parameters;

	for (int n = 0; n < count; ++n, parameter = parameter->next)
		if (n == index)
			return parameter->value;

	return NULL;
}

void Parameters::copy(Properties * properties)
{
	int count = properties->getCount();

	for (int n = 0; n < count; ++n)
		putValue (properties->getName (n), properties->getValue (n));
}

void Parameters::clear()
{
	for (Parameter *parameter; (parameter = parameters);)
		{
		parameters = parameter->next;
		delete parameter;
		}
	
	count = 0;
}

int Parameters::getValueLength(int index)
{
	Parameter *parameter = parameters;

	for (int n = 0; n < count; ++n, parameter = parameter->next)
		if (n == index)
			return parameter->valueLength;

	return 0;
}

void Parameters::setValue(const char *name, const char *value)
{
	for (Parameter *parameter = parameters; parameter; parameter = parameter->next)
		if (!strcasecmp (name, parameter->name))
			{
			parameter->setValue (value);
			return;
			}

	putValue (name, value);
}
