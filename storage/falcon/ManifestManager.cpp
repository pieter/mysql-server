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

// ManifestManager.cpp: implementation of the ManifestManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "ManifestManager.h"
#include "Manifest.h"
#include "ManifestClass.h"
#include "JavaLoad.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ManifestManager::ManifestManager()
{
	manifests = NULL;
	memset (classes, 0, sizeof (classes));
}

ManifestManager::~ManifestManager()
{
	for (Manifest *manifest; manifest = manifests;)
		{
		manifests = (Manifest*) manifest->next;
		delete manifest;
		}
}

void ManifestManager::insert(ManifestClass *cls)
{
	int slot = cls->name.hash (MANIFEST_HASH_SIZE);

	for (ManifestClass *p = classes [slot]; p; p = p->collision)
		if (p->name == cls->name)
			{
			cls->duplicate = p->duplicate;
			p->duplicate = cls;
			return;
			}

	cls->collision = classes [slot];
	classes [slot] = cls;
}

ManifestClass* ManifestManager::findClass(const char *name)
{
	for (ManifestClass *cls = classes [JString::hash (name, MANIFEST_HASH_SIZE)];
		 cls; cls = cls->collision)
		if (cls->name == name)
			return cls;

	return NULL;
}

Manifest* ManifestManager::findManifest(const char *name)
{
	for (Manifest *manifest = manifests; manifest; manifest = (Manifest*) manifest->next)
		if (manifest->name == name)
			return manifest;

	return NULL;
}

Manifest* ManifestManager::createManifest(const char *name, JavaLoad *javaLoad)
{
	Manifest *manifest = new Manifest (this, name, javaLoad);
	manifest->next = manifests;
	manifests = manifest;

	return manifest;
}

bool ManifestManager::validate(const char *name, JavaLoad *javaLoad)
{
	UCHAR *gook = NULL;
	bool result = true;

	for (ManifestClass *manifestClass = classes [JString::hash (name, MANIFEST_HASH_SIZE)];
		 manifestClass; manifestClass = manifestClass->collision)
		if (manifestClass->name == name)
			{
			if (!gook)
				{
				javaLoad->rewind();
				gook = new UCHAR [javaLoad->fileLength];
				int l = javaLoad->read (gook, javaLoad->fileLength);
				}
			if (!manifestClass->validate (javaLoad->fileLength, gook))
				result = false;
			}

	if (gook)
		delete [] gook;

	return result;
}

void ManifestManager::manifestDeleted(Manifest *manifest)
{
	for (Manifest **ptr = &manifests; *ptr; ptr = (Manifest**) &(*ptr)->next)
		if (*ptr == manifest)
			{
			*ptr = (Manifest*) manifest->next;
			break;
			}

	for (ManifestClass *cls = manifest->classes; cls; cls = cls->next)
		remove (cls);
}

void ManifestManager::remove(ManifestClass *cls)
{
	for (ManifestClass **ptr = classes + cls->name.hash (MANIFEST_HASH_SIZE); *ptr;
		 ptr = &(*ptr)->collision)
		if (*ptr == cls)
			{
			*ptr = cls->collision;
			break;
			}
}
