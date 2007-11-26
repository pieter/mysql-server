/* Copyright (C) 2007 MySQL AB

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

// SRLCreateTableSpace.h: interface for the SRLCreateTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLCREATETABLESPACE_H__1D7C8978_EEA6_49B0_9A65_0BEFDDB9ABA8__INCLUDED_)
#define AFX_SRLCREATETABLESPACE_H__1D7C8978_EEA6_49B0_9A65_0BEFDDB9ABA8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class TableSpace;

class SRLCreateTableSpace  : public SerialLogRecord   
{
public:
	SRLCreateTableSpace();
	virtual ~SRLCreateTableSpace();

	virtual void	redo();
	virtual void	commit();
	virtual void	pass2();
	virtual void	pass1();
	virtual void	read();
	virtual void	print(void);
	void			append (TableSpace *tableSpace);

	const char	*name;
	const char	*filename;
	int			tableSpaceId;
	int			nameLength;
	int			filenameLength;
};

#endif // !defined(AFX_SRLCREATETABLESPACE_H__1D7C8978_EEA6_49B0_9A65_0BEFDDB9ABA8__INCLUDED_)
