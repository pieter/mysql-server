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


// Blob.h: interface for the Blob class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BLOB_H__84FD196A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_BLOB_H__84FD196A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "BlobReference.h"

class Stream;

class Blob : public BlobReference
{
public:
	virtual void	addRef() = 0;
	virtual int		release() = 0;
	virtual void	getBytes (int32 pos, int32 length, void *buffer) = 0;
	virtual int		length() = 0;
	virtual int		getSegmentLength (int pos) = 0;
	virtual void	*getSegment (int pos) = 0;
	virtual void	putSegment (int32 length, const void *segment) = 0;
	virtual Stream	*getStream() = 0;
};

class Clob : public BlobReference
{
public:
	virtual void	addRef() = 0;
	virtual int		release() = 0;
	virtual void	getSubString (int32 pos, int32 length, char *buffer) = 0;
	virtual int		length() = 0;
	virtual int		getSegmentLength (int pos) = 0;
	virtual const char *getSegment (int pos) = 0;
	virtual void	putSegment (int32 length, const char *segment) = 0;
	virtual Stream	*getStream() = 0;
};

#endif // !defined(AFX_BLOB_H__84FD196A_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
