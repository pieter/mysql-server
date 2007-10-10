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

// ArgsException.cpp: implementation of the ArgsException class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "ArgsException.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ArgsException::ArgsException(const char * txt, ...)
{
	va_list		args;
	va_start	(args, txt);
	char		temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, txt, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	text = temp;
}

ArgsException::~ArgsException()
{

}

const char* ArgsException::getText()
{
	return text;
}
