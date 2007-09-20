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

// AsciiBlob.h: interface for the AsciiBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */

#if !defined(AFX_ASCIIBLOB_H__74F68A12_3271_11D4_98E1_0000C01D2301__INCLUDED_)
#define AFX_ASCIIBLOB_H__74F68A12_3271_11D4_98E1_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Blob.h"
#include "Stream.h"

class Dbb;
class Section;

class AsciiBlob : public Clob, public Stream
{
public:
	AsciiBlob(Dbb * db, int32 recNumber, Section *blobSection);
	AsciiBlob (Blob *blob);
	AsciiBlob (Clob *clob);
	AsciiBlob (int minSegmentSize);
	AsciiBlob();
	virtual ~AsciiBlob();

	void			init (bool populated);
	virtual void	unsetData();
	virtual Stream* getStream();
	virtual void	putSegment (int32 length, const char *segment);
	virtual void	putSegment (const char *string);
	void			populate();
	virtual const char* getSegment (int pos);
	virtual int		getSegmentLength (int pos);
	virtual void	putSegment (int length, const char *data, bool copyFlag);
	virtual void	getSubString (int32 pos, int32 length, char *buffer);
	virtual int		length();
	virtual int		release();
	virtual void	addRef();

	int			useCount;
	Dbb			*dbb;
	Section		*section;
	int32		recordNumber;
	bool		populated;
};

#endif // !defined(AFX_ASCIIBLOB_H__74F68A12_3271_11D4_98E1_0000C01D2301__INCLUDED_)
