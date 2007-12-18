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

// NField.h: interface for the NField class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NFIELD_H__02AD6A59_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_NFIELD_H__02AD6A59_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"
//#include "Types.h"	// Added by ClassView

class Context;


class NField : public NNode  
{
public:
	virtual void prettyPrint (int level, PrettyPrint *pp);
	virtual Collation* getCollation();
	virtual bool equiv (NNode *node);
	virtual NNode* copy(CompiledStatement * statement, Context * context);
	virtual FieldType getType();
	virtual void gen (Stream *stream);
	virtual const char* getName();
	 NField(CompiledStatement *statement);
	//virtual NNode* clone();
	virtual bool isMember (Table* table);
	Field* getField();
	virtual Value* eval(Statement * statement);
	virtual int matchField (Context *context, Index *index);
	bool computable(CompiledStatement * statement);
	NField(CompiledStatement *statement, Field *fld, Context *context);
	virtual ~NField();

	Field	*field;
	int		contextId;
	int		valueSlot;
};

#endif // !defined(AFX_NFIELD_H__02AD6A59_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
