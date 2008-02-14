/* Copyright (C) 2008 MySQL AB

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

#ifndef _SRL_SAVEPOINT_ROLLBACK_H_
#define _SRL_SAVEPOINT_ROLLBACK_H_

#include "SerialLogRecord.h"

class SRLSavepointRollback : public SerialLogRecord
{
public:
	SRLSavepointRollback(void);
	~SRLSavepointRollback(void);
	void append(TransId transactionId, int savepointId);
	virtual void read(void);
	virtual void pass1(void);
	
	TransId	transactionId;
	int		savepointId;
};

#endif
