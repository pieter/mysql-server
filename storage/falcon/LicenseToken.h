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

// LicenseToken.h: interface for the LicenseToken class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LICENSETOKEN_H__6BB1AE62_6B95_11D4_88A8_0CC1A0000000__INCLUDED_)
#define AFX_LICENSETOKEN_H__6BB1AE62_6B95_11D4_88A8_0CC1A0000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class LicenseProduct;


class LicenseToken  
{
public:
	void addRef();
	void release();
	LicenseToken(LicenseProduct *licenseProduct);
	virtual ~LicenseToken();

	LicenseProduct	*product;
	LicenseToken	*next;
	LicenseToken	*prior;
	int				useCount;
};

#endif // !defined(AFX_LICENSETOKEN_H__6BB1AE62_6B95_11D4_88A8_0CC1A0000000__INCLUDED_)
