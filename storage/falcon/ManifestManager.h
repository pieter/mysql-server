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

// ManifestManager.h: interface for the ManifestManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MANIFESTMANAGER_H__E0889BF3_8281_11D4_98F1_0000C01D2301__INCLUDED_)
#define AFX_MANIFESTMANAGER_H__E0889BF3_8281_11D4_98F1_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MANIFEST_HASH_SIZE	101

class Manifest;
class ManifestClass;
class JavaLoad;

class ManifestManager  
{
public:
	void remove (ManifestClass *cls);
	void manifestDeleted (Manifest *manifest);
	bool validate (const char *name, JavaLoad *javaLoad);
	Manifest* createManifest (const char *name, JavaLoad *javaLoad);
	Manifest* findManifest (const char *name);
	ManifestClass* findClass(const char *name);
	void insert (ManifestClass *cls);
	ManifestManager();
	virtual ~ManifestManager();

	ManifestClass	*classes [MANIFEST_HASH_SIZE];
	Manifest		*manifests;
};

#endif // !defined(AFX_MANIFESTMANAGER_H__E0889BF3_8281_11D4_98F1_0000C01D2301__INCLUDED_)
