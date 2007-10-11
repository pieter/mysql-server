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

// Manifest.h: interface for the Manifest class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MANIFEST_H__E0889BF4_8281_11D4_98F1_0000C01D2301__INCLUDED_)
#define AFX_MANIFEST_H__E0889BF4_8281_11D4_98F1_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "License.h"

class ManifestClass;
class ManifestManager;
class JavaLoad;

class Manifest : public License  
{
public:
	bool invalidManifest(const char* why);
	JString getManifest (JavaLoad *javaLoad);
	JString getManifest ();
	bool parse (const char *text);
	const char* match (const char *string, const char *line);
	Manifest(ManifestManager *manifestManager, const char *manifestName, JavaLoad *javaLoad);
	virtual ~Manifest();

	JString			name;
	ManifestClass	*classes;
	ManifestManager	*manager;
	JavaLoad		*javaLoad;
};

#endif // !defined(AFX_MANIFEST_H__E0889BF4_8281_11D4_98F1_0000C01D2301__INCLUDED_)
