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

// KeyGen.h: interface for the KeyGen class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KEYGEN_H__7D3A23E9_1599_11D6_B8F7_00E0180AC49E__INCLUDED_)
#define AFX_KEYGEN_H__7D3A23E9_1599_11D6_B8F7_00E0180AC49E__INCLUDED_

#include "JString.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#define MRSA
struct Rsa_key;

class KeyGen  
{
public:
	static JString extractPublicKey(const char *key);
	JString makeKey(int type, Rsa_key *key);
	void check(int ret);
	void generate (int keyLength, const char *seed);
	KeyGen();
	virtual ~KeyGen();

	JString		publicKey;
	JString		privateKey;
	int			prngIndex;
};

#endif // !defined(AFX_KEYGEN_H__7D3A23E9_1599_11D6_B8F7_00E0180AC49E__INCLUDED_)
