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

// Crypto.h: interface for the Crypto class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CRYPTO_H__43638664_6110_11D6_B90C_00E0180AC49E__INCLUDED_)
#define AFX_CRYPTO_H__43638664_6110_11D6_B90C_00E0180AC49E__INCLUDED_

//#include "JString.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Crypto  
{
public:
	static void DECStringToKey (const char *string, UCHAR key [8]);
	static int DESprocess (bool encrypt, const UCHAR *key, int length, const char *input, char *output);
	static int DESdecryption (const UCHAR *key, int length, const char *input, char *output);
	int DESBlockSize();
	static int DESencryption (const UCHAR *key, int length, const char*data, char *output);
	static int fromBase64(const char *base64, int bufferLength, UCHAR *buffer);
	static JString toBase64(int length, void *gook, int count);
	Crypto();
	virtual ~Crypto();

};

#endif // !defined(AFX_CRYPTO_H__43638664_6110_11D6_B90C_00E0180AC49E__INCLUDED_)
