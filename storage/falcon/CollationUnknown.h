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

#ifndef _COLLATION_UNKNOWN_H_
#define _COLLATION_UNKNOWN_H_

#include "Collation.h"
#include "JString.h"

class CollationManager;

class CollationUnknown : public Collation
{
public:
	CollationUnknown(CollationManager *collationManager, const char *collationName);
	virtual ~CollationUnknown(void);
	
	virtual int			compare (Value *value1, Value *value2);
	virtual int			makeKey (Value *value, IndexKey *key, int partialKey, int maxKeyLength);
	virtual const char	*getName ();
	virtual bool		starting (const char *string1, const char *string2);
	virtual bool		like (const char *string, const char *pattern);
	virtual char		getPadChar(void);
	virtual int			truncate(Value *value, int partialLength);

	CollationManager*	manager;
	JString				name;
	Collation*			collation;

	void				getCollation(void);
};

#endif
