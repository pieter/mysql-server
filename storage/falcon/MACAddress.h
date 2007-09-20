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

// MACAddress.h: interface for the MACAddress class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MACADDRESS_H__34604675_218A_42E6_96D6_FA5842BE3BCB__INCLUDED_)
#define AFX_MACADDRESS_H__34604675_218A_42E6_96D6_FA5842BE3BCB__INCLUDED_

//#include "Socket.h"	// Added by ClassView
//#include "JString.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class MACAddress  
{
public:
	static JString getAddressString (QUAD address);
	static bool isAddress (QUAD address);
	static int getHexDigit (char c);
	static QUAD getAddress (const char *string);
	static QUAD getAddress (int index);
	static int getAddressCount();
	static QUAD getAddress (int length, UCHAR *bytes);
	static int getAddresses();
	MACAddress();
	virtual ~MACAddress();

};

#endif // !defined(AFX_MACADDRESS_H__34604675_218A_42E6_96D6_FA5842BE3BCB__INCLUDED_)
