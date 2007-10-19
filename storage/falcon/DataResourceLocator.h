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

// DataResourceLocator.h: interface for the DataResourceLocator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATARESOURCELOCATOR_H__31E372B3_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
#define AFX_DATARESOURCELOCATOR_H__31E372B3_B24B_11D2_AB5E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class PreparedStatement;
class ResultSet;
class Stream;
CLASS(Field);
class Table;
class ForeignKey;
class Index;
class TemplateContext;
class Connection;

class DataResourceLocator  
{
public:
	static JString getLocator (ResultSet *resultSet, ForeignKey *key);
	static JString getLocator (ResultSet *resultSet, Index *index);
	static void genDrl (TemplateContext *context, Index *index);
	static void genDrl (TemplateContext *context, ForeignKey *key);
	static void copyValue (char **to, const char *from);
	PreparedStatement* prepareStatement (Connection *connection, const char *drl);
	static void copy (char** to, const char *from);
	bool getToken (char **pChar, char *token);
	DataResourceLocator();
	virtual ~DataResourceLocator();

};

#endif // !defined(AFX_DATARESOURCELOCATOR_H__31E372B3_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
