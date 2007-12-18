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

// SRLWordUpdate.h: interface for the SRLWordUpdate class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLWORDUPDATE_H__0604EAA4_084B_456C_A170_F4BB8B0B0A54__INCLUDED_)
#define AFX_SRLWORDUPDATE_H__0604EAA4_084B_456C_A170_F4BB8B0B0A54__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLWordUpdate : public SerialLogRecord  
{
public:
	void read();
	SRLWordUpdate();
	virtual ~SRLWordUpdate();

};

#endif // !defined(AFX_SRLWORDUPDATE_H__0604EAA4_084B_456C_A170_F4BB8B0B0A54__INCLUDED_)
