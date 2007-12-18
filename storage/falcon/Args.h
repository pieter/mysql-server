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

// Args.h: interface for the Args class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ARGS_H__7C584339_5A11_4040_889B_417B607C858B__INCLUDED_)
#define AFX_ARGS_H__7C584339_5A11_4040_889B_417B607C858B__INCLUDED_

//#include "JString.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct Switches 
	{
    const char	*string;
	bool		*boolean;
	char		**argument;
	const char	*argName;
	const char	*description;
	};

#define WORD_ARG(value,name)	"", NULL, &value, name, NULL,
#define ARG_ARG(sw,value,help)	sw, NULL, &value, NULL, help,
#define SW_ARG(sw,value,help)	sw, &value,NULL,  NULL, help,


class Args  
{
public:
	static bool readPassword (const char *msg, char *pw1, int length);
	static bool readPasswords(const char *msg, char *pw1, int length);
	static void printHelp (const char *helpText, const Switches *switches);
	static void init (const Switches *switches);
	static void parse (const Switches *switches, int argc, char **argv);
	Args();
	virtual ~Args();

};

#endif // !defined(AFX_ARGS_H__7C584339_5A11_4040_889B_417B607C858B__INCLUDED_)
