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

// Registry.h: interface for the Registry class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGISTRY_H__ED96F5D0_B205_11D2_AB5D_0000C01D2301__INCLUDED_)
#define AFX_REGISTRY_H__ED96F5D0_B205_11D2_AB5D_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "JString.h"

class Registry  
{
public:
	static bool checkSitePassword (const char *digest);
	static JString getSitePasswordFilename();
	static const char* getKey();
	void deleteDatabase (const char *name);
	char* genLinkName (const char *name, char *buffer, int bufferSize);
	static JString getInstallPath();
	void defineDatabase (const char *name, const char *fileName);
	const char* findDatabase (const char *name, int bufferSize, char *buffer);
	Registry();
	virtual ~Registry();

};

#endif // !defined(AFX_REGISTRY_H__ED96F5D0_B205_11D2_AB5D_0000C01D2301__INCLUDED_)
