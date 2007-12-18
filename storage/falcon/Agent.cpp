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

// Agent.cpp: implementation of the Agent class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Agent.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Agent::Agent(const char *agentName, const char *agentAction)
{
	name = agentName;
	action = agentAction;
	specialAgent = !action.equalsNoCase ("disable");
}

Agent::~Agent()
{

}

bool Agent::isSpecialAgent()
{
	return specialAgent;	
}
