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

// OpSys.cpp: implementation of the OpSys class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "Engine.h"
#include "OpSys.h"
#include "Log.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

OpSys::OpSys()
{

}

OpSys::~OpSys()
{

}

void OpSys::logParameters()
{
#ifdef RUSAGE_SELF
	rusage usage;
	rlimit size;

	if (getrusage (RUSAGE_SELF, &usage) == 0)
		Log::log ("minflt %d, maxflt %d, swp %d\n",
				  usage.ru_minflt,
				  usage.ru_majflt,
				  usage.ru_nswap);

#ifdef RLIMIT_AS

	if (getrlimit (RLIMIT_AS, &size) == 0 && size.rlim_cur != (rlim_t) -1)
		Log::log ("size %d/%d\n",
				  size.rlim_cur,
				  size.rlim_max);

#endif
#endif
}
