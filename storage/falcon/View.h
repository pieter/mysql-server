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

// View.h: interface for the View class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VIEW_H__F5614381_EB03_11D3_98D6_0000C01D2301__INCLUDED_)
#define AFX_VIEW_H__F5614381_EB03_11D3_98D6_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class NNode;
class Table;
class CompiledStatement;
class Syntax;
class Stream;
class Database;

class View  
{
public:
	bool equiv (NNode *node1, NNode *node2);
	bool isEquiv (View *view);
	void drop (Database *database);
	void save (Database *database);
	void createFields (Table *table);
	void gen (Stream *stream);
	void compile (CompiledStatement *statement, Syntax *viewSyntax);
	View(const char *schema, const char *name);
	virtual ~View();

	int			numberTables;
	int			numberColumns;
	const char	*name;
	const char	*schema;
	JString		*columnNames;
	NNode		*columns;
	NNode		*predicate;
	Table		**tables;
	Table		*table;
	NNode		*sort;
	NNode		*nodeList;
	bool		distinct;
};

#endif // !defined(AFX_VIEW_H__F5614381_EB03_11D3_98D6_0000C01D2301__INCLUDED_)
