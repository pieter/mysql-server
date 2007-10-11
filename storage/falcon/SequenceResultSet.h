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

// SequenceResultSet.h: interface for the SequenceResultSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEQUENCERESULTSET_H__52A2DA18_7937_11D4_98F0_0000C01D2301__INCLUDED_)
#define AFX_SEQUENCERESULTSET_H__52A2DA18_7937_11D4_98F0_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "MetaDataResultSet.h"

class SequenceManager;

class SequenceResultSet : public MetaDataResultSet  
{
public:
	virtual bool next();
	SequenceResultSet(ResultSet *source);
	virtual ~SequenceResultSet();

	SequenceManager	*sequenceManager;
};

#endif // !defined(AFX_SEQUENCERESULTSET_H__52A2DA18_7937_11D4_98F0_0000C01D2301__INCLUDED_)
