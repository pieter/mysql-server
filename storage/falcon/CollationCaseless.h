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

// CollationCaseless.h: interface for the CollationCaseless class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLLATIONCASELESS_H__1F63A288_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_COLLATIONCASELESS_H__1F63A288_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Collation.h"

class CollationCaseless : public Collation  
{
public:
	virtual bool		like(const char * string, const char * pattern);
	virtual bool		starting (const char *string1, const char *string2);
	virtual const char	*getName();
	virtual int			makeKey (Value *value, IndexKey *key, int partialKey, int maxKeyLength);
	virtual int			compare (Value *value1, Value *value2);
	virtual char		getPadChar(void);
	virtual int			truncate(Value *value, int partialLength);

	CollationCaseless();
	//virtual ~CollationCaseless();

	char	caseTable [256];
};

#endif // !defined(AFX_COLLATIONCASELESS_H__1F63A288_9414_11D5_899A_CC4599000000__INCLUDED_)
