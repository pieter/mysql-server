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

// NParameter.h: interface for the NParameter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NPARAMETER_H__02AD6A4A_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_NPARAMETER_H__02AD6A4A_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"

class NParameter : public NNode  
{
public:
	virtual bool equiv (NNode *node);
	virtual NNode* copy(CompiledStatement * statement, Context * context);
	virtual void gen (Stream *stream);
	//virtual NNode* clone();
	Value* eval (Statement *statement);
	NParameter(CompiledStatement *statement, int index);
	virtual ~NParameter();

	int		number;
};

#endif // !defined(AFX_NPARAMETER_H__02AD6A4A_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
