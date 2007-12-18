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

// LicenseToken.cpp: implementation of the LicenseToken class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "LicenseToken.h"
#include "LicenseProduct.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LicenseToken::LicenseToken(LicenseProduct *licenseProduct)
{
	product = licenseProduct;
	useCount = 1;
}

LicenseToken::~LicenseToken()
{

}

void LicenseToken::release()
{
	if (--useCount == 0)
		product->tokenReleased (this);
}

void LicenseToken::addRef()
{
	++useCount;
}
