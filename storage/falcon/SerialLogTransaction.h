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

// SerialLogTransaction.h: interface for the SerialLogTransaction class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGTRANSACTION_H__33E33BBC_8622_49DB_BD48_C6D51B5A1002__INCLUDED_)
#define AFX_SERIALLOGTRANSACTION_H__33E33BBC_8622_49DB_BD48_C6D51B5A1002__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#include "SerialLogAction.h"

enum sltState {
	sltUnknown,
	sltPrepared,
	sltCommitted,
	sltRolledBack
	};

class SerialLog;
class SerialLogWindow;
class Transaction;
struct SerialLogBlock;
	
class SerialLogTransaction //: public SerialLogAction
{
public:
	virtual uint64	getBlockNumber();
	//virtual bool	isTransaction();
	virtual bool	completedRecovery();
	virtual void	preRecovery();
	virtual void	doAction();
	virtual bool	isRipe();
	virtual bool	isXidEqual(int testLength, const UCHAR* test);

	void			setFinished();
	void			setState(sltState newState);
	void			setStart(const UCHAR *record, SerialLogBlock *blk, SerialLogWindow *win);
	void			rollback();
	void			commit();
	void			setPhysicalBlock();
	void			setXID(int xidLength, const UCHAR* xidPtr);

	SerialLogTransaction(SerialLog *serialLog, TransId transId);
	virtual ~SerialLogTransaction();

	SerialLogTransaction	*collision;
	TransId					transactionId;
	volatile sltState		state;
	SerialLogWindow			*window;
	Transaction				*transaction;
	uint64					blockNumber;
	int						blockOffset;
	int						recordOffset;
	int						xidLength;
	UCHAR					*xid;
	bool					finished;

	SerialLog				*log;
	SerialLogTransaction	*next;
	SerialLogTransaction	*prior;
	SerialLogTransaction	*earlier;
	SerialLogTransaction	*later;
	bool					flushing;
	bool					ordered;
	uint64					physicalBlockNumber;
	uint64					minBlockNumber;
	uint64					maxBlockNumber;

	void setTransaction(Transaction* transaction);
};

#endif // !defined(AFX_SERIALLOGTRANSACTION_H__33E33BBC_8622_49DB_BD48_C6D51B5A1002__INCLUDED_)
