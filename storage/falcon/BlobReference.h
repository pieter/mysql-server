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

// BlobReference.h: interface for the BlobReference class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BLOBREFERENCE_H__5F7BB0F4_7817_44E5_8A1F_16CE6148A3A4__INCLUDED_)
#define AFX_BLOBREFERENCE_H__5F7BB0F4_7817_44E5_8A1F_16CE6148A3A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JString.h"

#define ZERO_REPOSITORY_PLACE				((int32) 0x80000000)

#ifndef _WIN32
#define __int64			long long
#endif

typedef __int64				QUAD;
typedef unsigned char		UCHAR;

class Repository;

class BlobReference  
{
public:
	int getReference ( int size, UCHAR *buffer);
	void setReference (int length, UCHAR *buffer);
	int getReferenceLength();
	void copy(BlobReference *source);
#ifdef ENGINE
	virtual Stream* getStream();
	void getReference (Stream *stream);
	void setReference (int length, Stream *stream);
	void setRepository (Repository *repo);
#endif
	virtual bool isBlobReference();
	virtual void unsetBlobReference();
	virtual void setBlobReference (JString repo, int volume, QUAD id);
	virtual void unsetData();
	BlobReference();
	virtual ~BlobReference();

	JString		repositoryName;
	int			repositoryVolume;
	QUAD		blobId;
	Repository	*repository;
	bool		dataUnset;
};

#endif // !defined(AFX_BLOBREFERENCE_H__5F7BB0F4_7817_44E5_8A1F_16CE6148A3A4__INCLUDED_)
