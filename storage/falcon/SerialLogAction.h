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

// SerialLogAction.h: interface for the SerialLogAction class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGACTION_H__F7DBE5A6_0BA2_4419_BA1A_5E98602575E6__INCLUDED_)
#define AFX_SERIALLOGACTION_H__F7DBE5A6_0BA2_4419_BA1A_5E98602575E6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class SerialLog;

class SerialLogAction  
{
public:
	SerialLogAction(SerialLog *serialLog);
	virtual ~SerialLogAction();

	virtual uint64	getBlockNumber();
	virtual bool	isTransaction();
	virtual void	preRecovery();

	virtual bool	isRipe () = 0;
	virtual void	doAction () = 0;
	virtual bool	completedRecovery () = 0;

	SerialLog		*log;
	SerialLogAction	*next;
	SerialLogAction	*prior;
	bool			flushing;
	uint64			physicalBlockNumber;
	uint64			minBlockNumber;
	uint64			maxBlockNumber;
};

#endif // !defined(AFX_SERIALLOGACTION_H__F7DBE5A6_0BA2_4419_BA1A_5E98602575E6__INCLUDED_)
