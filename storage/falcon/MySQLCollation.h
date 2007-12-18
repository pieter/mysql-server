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


#ifndef _MYSQL_COLLATION_H_
#define _MYSQL_COLLATION_H_

#include "Collation.h"

// Entrypoints in the Falcon storage handler to collation handling

extern int falcon_strnxfrm (void *cs, 
							const char *dst, uint dstlen, int nweights,
							const char *src, uint srclen);

extern char falcon_get_pad_char (void *cs);
extern int falcon_cs_is_binary (void *cs);
extern unsigned int falcon_get_mbmaxlen (void *cs);
extern char falcon_get_min_sort_char (void *cs);
extern uint falcon_strnchrlen(void *cs, const char *s, uint l);
extern uint falcon_strnxfrmlen(void *cs, const char *s, uint srclen,
							   int partialKey, int bufSize);
extern uint falcon_strntrunc(void *cs, int partialKey,
							const char *s, uint l);
extern int falcon_strnncoll(void *cs, 
							const char *s1, uint l1, 
							const char *s2, uint l2, char flag);
extern int falcon_strnncollsp(void *cs, 
							const char *s1, uint l1, 
							const char *s2, uint l2, char flag);


class MySQLCollation : public Collation
{
public:
	MySQLCollation(JString collationName, void *arg);
	~MySQLCollation(void);

	virtual int			compare (Value *value1, Value *value2);
	virtual int			makeKey (Value *value, IndexKey *key, int partialKey, int maxKeyLength);
	virtual const char	*getName ();
	virtual bool		starting (const char *string1, const char *string2);
	virtual bool		like (const char *string, const char *pattern);
	virtual char		getPadChar(void);
	virtual int			truncate(Value *value, int partialLength);

	JString	name;
	void	*charset;
	char	padChar;
	bool	isBinary;
	uint	mbMaxLen;
	char	minSortChar;
	
	static inline uint computeKeyLength (uint length, const char *key,
										char padChar, char minSortChar)
		{
		for (const char *p = key + length; p > key; --p)
			if ((p[-1] != 0) && (p[-1] != padChar) && (p[-1] != minSortChar))
				return (uint) (p - key);

		return 0;
		}
};

#endif
