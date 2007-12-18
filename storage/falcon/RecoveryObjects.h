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

// RecoveryObjects.h: interface for the RecoveryObjects class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECOVERYOBJECTS_H__00C7CE5F_3C33_435C_9521_9C274CAB0581__INCLUDED_)
#define AFX_RECOVERYOBJECTS_H__00C7CE5F_3C33_435C_9521_9C274CAB0581__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

static const int RPG_HASH_SIZE			= 101;

class RecoveryPage;
class SerialLog;

class RecoveryObjects  
{
public:
	RecoveryObjects(SerialLog *serialLog);
	virtual ~RecoveryObjects();

	bool			isObjectActive(int objectNumber, int tableSpaceId);
	void			reset();
	bool			bumpIncarnation(int objectNumber, int tableSpaceId, int state, bool pass1);
	void			clear();
	RecoveryPage*	findRecoveryObject(int objectNumber, int tableSpaceId);
	void			setActive(int objectNumber, int tableSpaceId);
	void			setInactive(int objectNumber, int tableSpaceId);
	RecoveryPage*	getRecoveryObject(int objectNumber, int tableSpaceId);
	void			deleteObject(int objectNumber, int tableSpaceId);
	int				getCurrentState(int objectNumber, int tableSpaceId);

	SerialLog			*serialLog;
	RecoveryPage		*recoveryObjects[RPG_HASH_SIZE];
};

#endif // !defined(AFX_RECOVERYOBJECTS_H__00C7CE5F_3C33_435C_9521_9C274CAB0581__INCLUDED_)
