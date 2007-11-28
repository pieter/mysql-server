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

// Collation.h: interface for the Collation class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLLATION_H__1F63A286_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_COLLATION_H__1F63A286_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Value;
class IndexKey;

class Collation
{
public:
	Collation();

	virtual int		compare (Value *value1, Value *value2) = 0;
	virtual int		makeKey (Value *value, IndexKey *key, int partialKey, int maxKeyLength) = 0;
	virtual bool	starting (const char *string1, const char *string2) = 0;
	virtual bool	like (const char *string, const char *pattern) = 0;
	virtual char	getPadChar(void) = 0;
	virtual const char *getName () = 0;
	virtual int		truncate(Value *value, int partialLength) = 0;
	
	void			addRef(void);
	void			release(void);

protected:
	virtual ~Collation();

public:	
	Collation	*collision;
	int			useCount;
};

#endif // !defined(AFX_COLLATION_H__1F63A286_9414_11D5_899A_CC4599000000__INCLUDED_)
