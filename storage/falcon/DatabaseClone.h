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

#ifndef _DATABASE_CLONE_H_
#define _DATABASE_CLONE_H_

#include "DatabaseCopy.h"

class IO;
class Hdr;
class Bdb;

class DatabaseClone : public DatabaseCopy
{
public:
	DatabaseClone(Dbb *dbb);
	virtual ~DatabaseClone(void);
	
	virtual void		close(void);
	virtual const char* getFileName(void);

	void				createFile(const char* fileName);
	void				readHeader(Hdr* header);
	void				writeHeader(Hdr* header);
	void				writePage(Bdb* bdb);

	IO			*shadow;
	void clone(void);
};

#endif

