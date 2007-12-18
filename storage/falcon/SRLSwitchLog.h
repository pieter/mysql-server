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

// SRLSwitchLog.h: interface for the SRLSwitchLog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLSWITCHLOG_H__44F9D267_477F_45F0_B528_D2A9F757FB93__INCLUDED_)
#define AFX_SRLSWITCHLOG_H__44F9D267_477F_45F0_B528_D2A9F757FB93__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLSwitchLog : public SerialLogRecord  
{
public:
	void read();
	SRLSwitchLog();
	virtual ~SRLSwitchLog();

};

#endif // !defined(AFX_SRLSWITCHLOG_H__44F9D267_477F_45F0_B528_D2A9F757FB93__INCLUDED_)
