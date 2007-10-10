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

// Digest.h: interface for the Digest class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DIGEST_H__7B440227_38F1_4E65_AED2_654612D6AF5D__INCLUDED_)
#define AFX_DIGEST_H__7B440227_38F1_4E65_AED2_654612D6AF5D__INCLUDED_

//#include "JString.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define DIGEST_LENGTH	20

class Digest  
{
public:
	//static void getDigest(const char *digestText, UCHAR *digest);
	static void computeDigest(const char *text, int length, UCHAR *digest);
	static JString computeDigest(const char *text);
	//static JString genDigest(const char *string);
	//Digest();
	//virtual ~Digest();

};

#endif // !defined(AFX_DIGEST_H__7B440227_38F1_4E65_AED2_654612D6AF5D__INCLUDED_)
