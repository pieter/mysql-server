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

// ManifestClass.h: interface for the ManifestClass class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MANIFESTCLASS_H__E0889BF5_8281_11D4_98F1_0000C01D2301__INCLUDED_)
#define AFX_MANIFESTCLASS_H__E0889BF5_8281_11D4_98F1_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define DIGEST_LENGTH	20

class ManifestClass  
{
public:
	bool validate (int length, const UCHAR *classFile);
	static void computeDigest (int length, const UCHAR *stuff, UCHAR *digest);
	ManifestClass(JString className, UCHAR *classDigest);
	virtual ~ManifestClass();

	JString			name;
	ManifestClass	*collision;
	ManifestClass	*duplicate;
	ManifestClass	*next;
	UCHAR			digest [DIGEST_LENGTH];
};

#endif // !defined(AFX_MANIFESTCLASS_H__E0889BF5_8281_11D4_98F1_0000C01D2301__INCLUDED_)
