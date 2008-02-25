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

// RecordVersion.h: interface for the RecordVersion class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECORDVERSION_H__84FD1965_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_RECORDVERSION_H__84FD1965_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Record.h"

class Transaction;

class RecordVersion : public Record  
{
public:
	RecordVersion(Table *tbl, Format *fmt, Transaction *tran, Record *oldVersion);

	virtual bool		isSuperceded();
	virtual Transaction* getTransaction();
	virtual TransId		getTransactionId();
	virtual int			getSavePointId();
	virtual void		setSuperceded (bool flag);
	virtual Record*		getPriorVersion();
	virtual Record*		getGCPriorVersion(void);
	virtual bool		scavenge(RecordScavenge *recordScavenge);
	virtual void		scavenge(TransId targetTransactionId, int oldestActiveSavePoint);
	virtual bool		isVersion();
	virtual Record*		rollback();
	virtual Record*		fetchVersion (Transaction *transaction);
	virtual Record*		releaseNonRecursive();
	virtual void		setPriorVersion (Record *oldVersion);
	virtual void		setVirtualOffset(uint64 offset);
	virtual uint64		getVirtualOffset();
	virtual int			thaw();
	virtual void		print(void);

	void				commit();

protected:
	virtual ~RecordVersion();

public:
	uint64			virtualOffset;		// byte offset into serial log window
	Transaction		*transaction;
	Record			*priorVersion;
	RecordVersion	*nextInTrans;
	RecordVersion	*prevInTrans;
	TransId			transactionId;
	int				savePointId;
	bool			superceded;
};

#endif // !defined(AFX_RECORDVERSION_H__84FD1965_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
