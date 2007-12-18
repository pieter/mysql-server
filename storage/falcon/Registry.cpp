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

// Registry.cpp: implementation of the Registry class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "Registry.h"
#include "SQLError.h"
#include "StringTransform.h"
#include "FileTransform.h"
#include "EncryptTransform.h"
#include "SHATransform.h"
#include "Base64Transform.h"
#include "EncodeTransform.h"
#include "DecodeTransform.h"
#include "TransformUtil.h"

#ifndef PATH_MAX
#define PATH_MAX		256
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#ifdef _WIN32
#define KEY		"SOFTWARE\\Netfrastructure\\Netfrastructure\\Databases"
#define SUB_KEY	NULL
#define PATH_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Netfrastructure"
#else
#define KEY		""
#endif

#define UNIX_DEFAULT		"/opt/netfrastructure"
#define UNIX_OVERRIDE		"NETFRASTRUCTURE"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Registry::Registry()
{

}

Registry::~Registry()
{

}

const char* Registry::findDatabase(const char * name, int bufferSize, char *buffer)
{
#ifdef _WIN32
	char keyString [256];
	sprintf (keyString, "%s\\%s", KEY, name);
	HKEY key;
	long ret = RegOpenKey (HKEY_LOCAL_MACHINE, keyString, &key);
	
	if (ret != ERROR_SUCCESS)
		return NULL;

	long length = bufferSize;
	ret = RegQueryValue (key, SUB_KEY, buffer, &length);

	if (ret != ERROR_SUCCESS)
		{
		RegCloseKey (key);
		return NULL;
		}

	RegCloseKey (key);

	return buffer;
#else
	char link [PATH_MAX];
	int n = readlink (genLinkName (name, link, sizeof (link)), buffer, bufferSize - 1);

	if (n <= 0)
		return NULL;

	buffer [n] = 0;

	return buffer;
#endif

}

void Registry::defineDatabase(const char * name, const char * fileName)
{
#ifdef _WIN32
	char expandedName [256], *baseName;
	GetFullPathName (fileName, sizeof (expandedName), expandedName, &baseName);
	char keyString [256];
	sprintf (keyString, "%s\\%s", KEY, name);
	HKEY key;
	long ret = RegCreateKey (HKEY_LOCAL_MACHINE, keyString, &key);

	if (ret != ERROR_SUCCESS)
		throw SQLError (RUNTIME_ERROR, "RegCreateKey failed creating \"%s\"", keyString);
		
	ret = RegSetValue (key, SUB_KEY, REG_SZ, expandedName, (int) strlen (expandedName));

	if (ret != ERROR_SUCCESS)
		throw SQLError (RUNTIME_ERROR, "RegSetValue failed setting \"%s\"", expandedName);
		
	ret = RegCloseKey (key);
#else
	char link [PATH_MAX];
	char expandedName [PATH_MAX];
	expandedName [0] = 0;
	
	const char *path = realpath (fileName, expandedName);

	if (!path)
		if (errno == ENOENT && expandedName [0])
			path = expandedName;
		else
			{
			path = fileName;
			perror ("realpath");
			printf ("expandedName: %s\n", expandedName);
			}

	genLinkName (name, link, sizeof (link));
    unlink (link);

	if (symlink (path, link))
		throw SQLError (RUNTIME_ERROR, "can't create symbol link %s to %s\n", link, path);
#endif
}

JString Registry::getInstallPath()
{
#ifdef _WIN32
	char keyString [256];
	HKEY key;
	long ret = RegOpenKey (HKEY_LOCAL_MACHINE, PATH_KEY, &key);

	DWORD length = sizeof (keyString);
	ret = RegQueryValueEx (key, "Path", NULL, NULL, (LPBYTE) keyString, &length);

	if (ret != ERROR_SUCCESS)
		{
		RegCloseKey (key);
		return "";
		}

	RegCloseKey (key);

	return keyString;
#else
	const char *path = getenv (UNIX_OVERRIDE);

	if (path)
		return path;

	return UNIX_DEFAULT;
#endif
}

char* Registry::genLinkName(const char *name, char * buffer, int bufferSize)
{
	char *p = buffer;
	JString installPath = getInstallPath();
	const char *q;
    
	for (q = installPath; *q;)
		*p++ = *q++;

	for (q = "/db."; *q;)
		*p++ = *q++;

	for (q = name; *q; ++q)
		*p++ = UPPER (*q);

	*p = 0;

	return buffer;
}

void Registry::deleteDatabase(const char *name)
{
#ifdef _WIN32
	char keyString [256];
	sprintf (keyString, "%s\\%s", KEY, name);
	long ret = RegDeleteKey (HKEY_LOCAL_MACHINE, keyString);

	if (ret != ERROR_SUCCESS)
		throw SQLError (RUNTIME_ERROR, "can't delete registry symbol %s\n", keyString);
#else
	char link [PATH_MAX];
	genLinkName (name, link, sizeof (link));
    
	if (unlink (link))
		throw SQLError (RUNTIME_ERROR, "can't delete symbol link %s\n", link);
#endif
}

const char* Registry::getKey()
{
	return KEY;
}

JString Registry::getSitePasswordFilename()
{
	return getInstallPath() + "/site_password";
}

bool Registry::checkSitePassword(const char *password)
{
	/***
	FILE *file = fopen (getSitePasswordFilename(), "r");

	if (!file)
		return false;

	const char *sp = digest;

	for (char c; (c = getc (file)) && c != EOF && (c == '\n' || c == '\r' || c == *sp++);)
		;

	fclose (file);

	return *sp == 0;
	***/
	EncodeTransform<StringTransform,SHATransform> encode (password);
	DecodeTransform<FileTransform,Base64Transform> decode(getSitePasswordFilename());

	return TransformUtil::compareDigests(&encode, &decode);
}
