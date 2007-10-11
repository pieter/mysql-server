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

// SRLSequence.h: interface for the SRLSequence class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLSEQUENCE_H__9B8F854C_A159_4185_B0FC_C51C2C092868__INCLUDED_)
#define AFX_SRLSEQUENCE_H__9B8F854C_A159_4185_B0FC_C51C2C092868__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLSequence : public SerialLogRecord  
{
public:
	SRLSequence();
	virtual ~SRLSequence();
	virtual void read();
	virtual void append(int sequenceId, int64 sequence);
	virtual void redo();

	int			sequenceId;
	int64		sequence;
	virtual void print(void);
};

#endif // !defined(AFX_SRLSEQUENCE_H__9B8F854C_A159_4185_B0FC_C51C2C092868__INCLUDED_)
