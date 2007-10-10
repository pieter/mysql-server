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

// StringTransform.h: interface for the StringTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STRINGTRANSFORM_H__7F8C2F45_EAAE_4ADD_A439_01ED8CE60FC7__INCLUDED_)
#define AFX_STRINGTRANSFORM_H__7F8C2F45_EAAE_4ADD_A439_01ED8CE60FC7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class StringTransform : public Transform  
{
public:
	virtual void reset();
	void setString(const char *string, bool copyFlag = false);
	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	void setString(UIPTR length, const UCHAR *stuff, bool copyFlag = false);
	StringTransform(unsigned int length, const UCHAR *data, bool copyFlag = false);
	StringTransform(const char *string, bool copyFlag = true);
	StringTransform();
	virtual ~StringTransform();

	const UCHAR	*ptr;
	const UCHAR	*end;
	UCHAR		*data;
};

#endif // !defined(AFX_STRINGTRANSFORM_H__7F8C2F45_EAAE_4ADD_A439_01ED8CE60FC7__INCLUDED_)
