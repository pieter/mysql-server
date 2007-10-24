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

#ifndef _SRL_SECTION_LINE_H_
#define _SRL_SECTION_LINE_H_

#include "SerialLogRecord.h"

class SRLSectionLine : public SerialLogRecord
{
public:
	SRLSectionLine(void);
	virtual ~SRLSectionLine(void);
	
	void append(Dbb *dbb, int32 sectionPageNumber, int32 dataPageNumber);
	virtual void read(void);
	virtual void pass1(void);
	virtual void pass2(void);
	virtual void redo(void);
	virtual void print(void);

	int		tableSpaceId;
	int32	pageNumber;
	int32	dataPageNumber;
};

#endif
