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

// QueryString.cpp: implementation of the QueryString class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "QueryString.h"
#include "Parameter.h"
#include "JString.h"
#include "Interlock.h"

#define ISDIGIT(c)			(c >= '0' && c <= '9')

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

QueryString::QueryString()
{
	useCount = 1;
}

QueryString::QueryString(const char *string)
{
	//printf ("%s\n", string);
	useCount = 1;

	if (!string)
		string = "";

	setString (string);
}

QueryString::~QueryString()
{
	Parameter *variable;
	
	for (int n = 0; n < QUERY_HASH_SIZE; ++n)
		while ((variable = variables [n]))
			{
			variables [n] = variable->collision;
			for (Parameter *var; (var = variable);)
				{
				variable = var->next;
				delete var;
				}
			}	
}

int QueryString::getParameter(const char * queryString, const char * name, int bufferLength, char * buffer)
{
	int l = (int) strlen (name);
	char *q = buffer;
	char *end = q + bufferLength;

	for (const char *p = queryString; *p;)
		{
		if (!strncmp (p, name, l) && p [l] == '=')
			{
			p += l + 1;

			for (char c; (c = *p++) && c != '&';)
				{
				if (c == '+')
					c = ' ';
				else if (c == '%')
					{
					char hex = 0;

					for (int n = 0; (c = *p) && n < 2; ++n, ++p)
						if (ISDIGIT (c))
							hex = hex * 16 + c - '0';
						else if (c >= 'a' && c <= 'f')
							hex = hex * 16 + c - 'a' + 10; 
						else if (c >= 'A' && c <= 'F')
							hex = hex * 16 + c - 'A' + 10;
						else
							break;

					c = hex;

					if (c == '\r')
						continue;
					}
				if (q < end)
					*q++ = c;
				else
					++q;
				}

			if (q < end)
				*q = 0;

			return (int) (q - buffer);
			}

		while (*p && *p++ != '&')
			;
		}

	*q = 0;

	return -1;
}


const char* QueryString::getParameter(const char *name, const char *defaultValue)
{
	int slot = JString::hash (name, QUERY_HASH_SIZE);

	for (Parameter *variable = variables [slot]; variable;
		 variable = variable->collision)
		if (!strcmp (variable->name, name))
			return variable->value;

	return defaultValue;
}


const char* QueryString::getParameter(const WCString *name)
{
	Parameter *parameter = findParameter (name);

	if (!parameter)
		return NULL;

	return parameter->value;
}

void QueryString::setString(const char *string)
{
	if (!(queryString = string))
		queryString = "";

	memset (variables, 0, sizeof (variables));
	uint32 length = (uint32) strlen (queryString);
	char buffer [256];
	char *temp = (length + 2 < sizeof (buffer)) ? buffer : new char [length + 2];

	for (const char *p = queryString; *p;)
		{
		char c, *q = temp;

		while (*p && (c = *p++) != '=')
			{
			if (c == '+')
				c = ' ';
			else if (c == '%')
				c = getHex (&p);

			*q++ = c;
			}

		*q++ = 0;
		char *value = q;
		bool quote = false;

		while (*p && (((c = *p++) != '&') || quote))
			{
			if (c == '+')
				c = ' ';
			else if (c == '%')
				{
				c = getHex (&p);
				/***
				if (c == '\r')
					continue;
				***/
				}

			*q++ = c;
			}

		*q = 0;
		int slot = JString::hash (temp, QUERY_HASH_SIZE);
		Parameter **ptr;
		Parameter *parameter;

		for (ptr = variables + slot; (parameter = *ptr); ptr = &parameter->collision)
			{
			if (strcmp (parameter->name, temp) == 0)
				{
				for (ptr = &parameter->next; *ptr; ptr = &(*ptr)->next)
					;

				break;
				}
			}

		//printf ("%*s: %*s\n", strlen (temp), temp, strlen (value), value);
		*ptr = new Parameter (NULL, temp, (int) strlen (temp), value, (int) strlen (value));
		}

	if (temp != buffer)
		delete [] temp;
}


Parameter* QueryString::findParameter(const WCString *name)
{
	int slot = JString::hash (name, QUERY_HASH_SIZE);

	for (Parameter *variable = variables [slot]; variable;
		 variable = variable->collision)
		if (variable->isNamed (name))
			return variable;

	return NULL;
}

void QueryString::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void QueryString::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

char QueryString::getHex(const char **ptr)
{
	char hex = 0;
	char c;
	const char *p = *ptr;

	for (int n = 0; (c = *p) && n < 2; ++n, ++p)
		if (ISDIGIT (c))
			hex = hex * 16 + c - '0';
		else if (c >= 'a' && c <= 'f')
			hex = hex * 16 + c - 'a' + 10; 
		else if (c >= 'A' && c <= 'F')
			hex = hex * 16 + c - 'A' + 10;
		else
			break;

	*ptr = p;

	return hex;
}
