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

// BEREncode.h: interface for the BEREncode class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BERENCODE_H__9B209F59_FF17_4094_9511_932E110E2A90__INCLUDED_)
#define AFX_BERENCODE_H__9B209F59_FF17_4094_9511_932E110E2A90__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Stream.h"
#include "JString.h"

class BEREncode  
{
public:
	int getLength();
	BEREncode(int bufferLength, UCHAR *buffer);
	static int encodeObjectNumbers (int count, int *numbers, int bufferLength, UCHAR *buffer);
	JString getJString();
	void dump(const char *filename);
	//static JString wrapPrivKey (const char *key);
	virtual void put (UCHAR c);
	BEREncode();
	virtual ~BEREncode();

	Stream	stream;
	UCHAR	*ptr;
	UCHAR	*end;
	UCHAR	*start;
};

#endif // !defined(AFX_BERENCODE_H__9B209F59_FF17_4094_9511_932E110E2A90__INCLUDED_)
