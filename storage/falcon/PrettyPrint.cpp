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

// PrettyPrint.cpp: implementation of the PrettyPrint class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "Engine.h"
#include "PrettyPrint.h"
#include "Stream.h"
#include "Log.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#define INDENT		2

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PrettyPrint::PrettyPrint(int flgs, Stream *strm)
{
	flags = flgs;
	stream =  strm;
}

PrettyPrint::~PrettyPrint()
{

}

void PrettyPrint::indent(int level)
{
	if (stream)
		{
		int count = level * INDENT;
		for (int n = 0; n < count; ++n)
			stream->putCharacter (' ');
		}
	else
		Log::debug ("%*s ", level * INDENT, "");
}

void PrettyPrint::put(const char *string)
{
	if (stream)
		stream->putSegment (string);
	else
		Log::debug (string);

}

void PrettyPrint::putLine(const char *string)
{
	if (stream)
		{
		stream->putSegment (string);
		stream->putCharacter ('\n');
		}
	else
		Log::debug ("%s\n", string);
}

void PrettyPrint::format(const char *pattern, ...)
{
	va_list		args;
	va_start	(args, pattern);
	char		temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, pattern, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	put (temp);
}
