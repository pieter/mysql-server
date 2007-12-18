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

// Debug.h: interface for the Debug class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DEBUG_H__75152282_97E6_48BE_A7B9_B63D67D33867__INCLUDED_)
#define AFX_DEBUG_H__75152282_97E6_48BE_A7B9_B63D67D33867__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum DebugCommand {
	Quit,
	Right,
	Left,
	Up,
	Dump,
	Throw,
	Read,
	Break,
	Help
	};

class Page;
class Bdb;
class Dbb;
class IndexPage;

class Debug  
{
public:
	void breakpoint();
	bool execute(const char *buffer);
	void execute(IndexPage *indexPage, DebugCommand command, const char *string);
	void execute(DebugCommand command, const char *string);
	void dump (Page *page);
	const char* match(const char *string, const char *token);
	void fetch (int pageNumber);
	void interpreter();
	Debug(Dbb *database, Page *sourcePage);
	virtual ~Debug();

	Page		*page;
	Dbb			*dbb;
	Bdb			*bdb;
	static int getPageId(Page* page);
};

#endif // !defined(AFX_DEBUG_H__75152282_97E6_48BE_A7B9_B63D67D33867__INCLUDED_)
