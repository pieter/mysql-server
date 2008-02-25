/* Copyright (C) 2008 MySQL AB

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

#ifndef _SERIALIZE_H_
#define _SERIALIZE_H_

#include "Stream.h"

class Serialize : public Stream
{
public:
	Serialize(void);
	virtual ~Serialize(void);
	
	void	putInt(int value);
	void	putInt64(int64 value);
	void	putData(uint length, const UCHAR* data);
	int		getInt(void);
	int64	getInt64(void);
	int		getDataLength(void);
	void	getData(uint length, UCHAR* buffer);
	UCHAR*	reserve(uint length);
	UCHAR	getByte(void);

	void	release(uint actualLength)
		{
		totalLength += actualLength;
		current->length += actualLength;
		}
		
	Segment	*segment;
	UCHAR	*data;
	UCHAR	*end;
};

#endif
