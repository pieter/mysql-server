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

// StreamTransform.h: interface for the StreamTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STREAMTRANSFORM_H__5A925009_0E8B_4DEB_80B1_402D7FBCC58B__INCLUDED_)
#define AFX_STREAMTRANSFORM_H__5A925009_0E8B_4DEB_80B1_402D7FBCC58B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class Stream;

class StreamTransform : public Transform  
{
public:
	virtual void reset();
	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	StreamTransform(Stream *inputStream);
	virtual ~StreamTransform();

	Stream	*stream;
	int		offset;
};

#endif // !defined(AFX_STREAMTRANSFORM_H__5A925009_0E8B_4DEB_80B1_402D7FBCC58B__INCLUDED_)
