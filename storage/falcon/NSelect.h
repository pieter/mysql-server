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

// NSelect.h: interface for the NSelect class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSELECT_H__02AD6A57_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_NSELECT_H__02AD6A57_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"
#include "Row.h"

struct NSelectEnv {
    LinkedList	*sourceContexts;
	LinkedList	*tables;
	LinkedList	*conjuncts;
	};

class Context;
class Fsb;
class ResultSet;
class Value;
class Table;

class NSelect : public NNode, public Row
{
public:
	NSelect(CompiledStatement *statement, Syntax *syntax, NNode *inExpr);
	virtual ~NSelect();

	void			pushContexts();
	NNode*			getValue (const char *name);
	int				compileJoin (Syntax *node, bool innerJoin, NSelectEnv *env);
	int				getIndex (Field *field);
	int				getColumnIndex (const char *columnName);
	const char*		getColumnName (int index);
	bool			next (Statement *statement, ResultSet *resultSet);
	void			evalStatement (Statement *statement);

	virtual Field*	getField (int index);
	virtual bool	computable(CompiledStatement * statement);
	virtual int		getNumberValues();
	virtual Value*	getValue (Statement *statement, int index);
	virtual Field*	getField (const char* fieldName);
	virtual bool	references (Table *table);
	virtual bool	isMember (Table *table);
	virtual void	prettyPrint (int level, PrettyPrint *pp);

	int			numberContexts;
	int			numberColumns;
	int			countSlot;
	const char	**columnNames;
	Context		**contexts;
	int			contextMap;
	NNode		*values;
	NNode		*orgBoolean;
	NNode		*groups;
	NSelect		*select;
	Fsb			*stream;
	NSelect		**unionBranches;
	int			*groupSlots;
	bool		statistical;
	CompiledStatement *compiledStatement;
};

#endif // !defined(AFX_NSELECT_H__02AD6A57_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
