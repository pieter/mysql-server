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

// Fsb.cpp: implementation of the Fsb class.
//
//////////////////////////////////////////////////////////////////////


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#include <stdio.h>
#include "Engine.h"
#include "Fsb.h"
#include "Log.h"
#include "Stream.h"
#include "NNode.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Fsb::Fsb()
{

}

Fsb::~Fsb()
{

}

void Fsb::open(Statement * statement)
{

}

Row* Fsb::fetch(Statement * statement)
{
	return NULL;
}

void Fsb::close(Statement * statement)
{

}

void Fsb::getStreams(int **ptr)
{

}



void Fsb::prettyPrint(int level, PrettyPrint *pp)
{

}

int Fsb::getStreamIndex(Statement *statement)
{
	return 0;
}
