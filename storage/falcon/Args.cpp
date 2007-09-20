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

// Args.cpp: implementation of the Args class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "Args.h"
#include "ArgsException.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Args::Args()
{

}

Args::~Args()
{

}

void Args::parse(const Switches *switches, int argc, char **argv)
{
	for (char **arg = argv, **end = arg + argc; arg < end;)
		{
		char *p = *arg++;
		const Switches *parameter = NULL;
		bool hit = false;
		for (const Switches *sw = switches; sw->string; ++sw)
			{
			if (strcmp (sw->string, p) == 0)
				{
				if (sw->boolean)
					*sw->boolean = true;
				if (sw->argument)
					{
					if (arg >= end)
						throw ArgsException ("an argument is required for \"%s\"", sw->string);
					*sw->argument = *arg++;
					}
				hit = true;
				break;
				}
			if (!sw->string [0])
				parameter = sw;
			}
		if (!hit)
			{
			if (parameter)
				{
				if (parameter->boolean)
					*parameter->boolean = true;
				if (parameter->argument)
					*parameter->argument = p;
				}
			else
				throw ArgsException ("invalid option \"%s\"", p);
			}
		}
}


void Args::init(const Switches *switches)
{
	for (const Switches *sw = switches; sw->string; ++sw)
		{
		if (sw->boolean)
			*sw->boolean = false;
		if (sw->argument)
			*sw->argument = NULL;
		}
}

void Args::printHelp(const char *helpText, const Switches *switches)
{
	int switchLength = 0;
	int argLength = 0;
	const Switches *sw;

	for (sw = switches; sw->string; ++sw)
		if (sw->description)
			{
			int l = strlen (sw->string);
			if (l > switchLength)
				switchLength = l;
			if (sw->argName)
				{
				l = strlen (sw->argName);
				if (l > argLength)
					argLength = l;
				}
			}

	if (helpText)
		printf (helpText);

	printf ("Options are:\n");

	for (sw = switches; sw->string; ++sw)
		if (sw->description)
			{
			const char *arg = (sw->argName) ? sw->argName : "";
			printf ("  %-*s %-*s   %s\n", switchLength, sw->string, argLength, arg, sw->description);
			}
}

bool Args::readPasswords(const char *msg, char *pw1, int length)
{
#ifdef _WIN32
	DWORD orgSettings, newSettings;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	int ret = GetConsoleMode(hStdin, &orgSettings);
	newSettings = orgSettings & ~(ENABLE_ECHO_INPUT); 
	ret = SetConsoleMode(hStdin, newSettings);
#else
	termios orgSettings, newSettings;
	tcgetattr(0, &orgSettings);
	newSettings = orgSettings;
	newSettings.c_lflag &= (~ECHO);
	tcsetattr(0, TCSANOW, &newSettings);
#endif
	char pw2 [100];
	bool hit = false;

	for (;;)
		{
		if (msg)
			printf (msg);
		printf ("New password: ");
		if (!fgets (pw1, length, stdin))
			break;
		char *p = strchr (pw1, '\n');
		if (p)
			*p = 0;
		if (!pw1 [0])
			{
			printf ("\nPassword may not be null.  Please re-enter.\n");
			continue;
			}
		printf ("\nRepeat new password: ");
		if (!fgets (pw2, sizeof (pw2), stdin))
			break;
		if ((p = strchr (pw2, '\n')))
			*p = 0;
		if (strcmp (pw1, pw2) == 0)
			{
			hit = true;
			break;
			}
		printf ("\nPasswords do not match.  Please re-enter.\n");
		}
#ifdef _WIN32
	ret = SetConsoleMode(hStdin, orgSettings);
#else
	tcsetattr(0, TCSANOW, &orgSettings);
#endif
	printf ("\n");

	return hit;
}

bool Args::readPassword(const char *msg, char *pw1, int length)
{
#ifdef _WIN32
	DWORD orgSettings, newSettings;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	int ret = GetConsoleMode(hStdin, &orgSettings);
	newSettings = orgSettings & ~(ENABLE_ECHO_INPUT); 
	ret = SetConsoleMode(hStdin, newSettings);
#else
	termios orgSettings, newSettings;
	tcgetattr(0, &orgSettings);
	newSettings = orgSettings;
	newSettings.c_lflag &= (~ECHO);
	tcsetattr(0, TCSANOW, &newSettings);
#endif
	bool hit = false;

	for (;;)
		{
		if (msg)
			printf (msg);
		if (!fgets (pw1, length, stdin))
			break;
		char *p = strchr (pw1, '\n');
		if (p)
			*p = 0;
		if (pw1 [0])
			{
			hit = true;
			break;
			}
		printf ("\nPassword may not be null.  Please re-enter.\n");
		continue;
		}

#ifdef _WIN32
	ret = SetConsoleMode(hStdin, orgSettings);
#else
	tcsetattr(0, TCSANOW, &orgSettings);
#endif
	printf ("\n");

	return hit;
}
