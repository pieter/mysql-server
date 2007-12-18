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

// BinaryBlob.h: interface for the BinaryBlob class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */


#if !defined(AFX_BINARYBLOB_H__74F68A11_3271_11D4_98E1_0000C01D2301__INCLUDED_)
#define AFX_BINARYBLOB_H__74F68A11_3271_11D4_98E1_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Blob.h"
#include "Stream.h"

class Dbb;
class AsciiBlob;
class Section;

class BinaryBlob : public Blob, public Stream 
{
public:
	BinaryBlob (Blob *blob);
	BinaryBlob (Dbb *db, int32 recordNumber, Section *blobSection);
	BinaryBlob (Clob *blob);
	BinaryBlob (int minSegmentSize);
	BinaryBlob();
	virtual ~BinaryBlob();

	void			init(bool populated);
	virtual void	unsetData();
	Stream*			getStream();
	virtual void	putSegment (int32 length, const void *buffer);
	void			putSegment (Blob *blob);
	void			putSegment (Clob *blob);
	
	virtual void*	getSegment (int pos);
	virtual int		getSegmentLength (int pos);
	void			putSegment (int length, const char *data, bool copyFlag);
	int				length();
	void			getBytes (int32 pos, int32 length, void *address);
	virtual int		release();
	virtual void	addRef();
	void			populate();

	int			useCount;
	int			offset;
	Dbb			*dbb;
	Section		*section;
	int32		recordNumber;
	bool		populated;
};

#endif // !defined(AFX_BINARYBLOB_H__74F68A11_3271_11D4_98E1_0000C01D2301__INCLUDED_)
