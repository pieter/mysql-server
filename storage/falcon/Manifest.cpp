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

// Manifest.cpp: implementation of the Manifest class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "Manifest.h"
#include "ManifestClass.h"
#include "ManifestManager.h"
#include "Stream.h"
#include "JavaLoad.h"
#include "Log.h"
//#include "Crypto.h"
//#include "Digest.h"
#include "StringTransform.h"
#include "Base64Transform.h"
#include "DecodeTransform.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Manifest::Manifest(ManifestManager *manifestManager, const char *manifestName, JavaLoad *load)
{
	classes = NULL;
	manager = manifestManager;
	name = manifestName;
	javaLoad = load;
	javaLoad->addRef();

	JString text = getManifest(javaLoad);
	parse (text);
}

Manifest::~Manifest()
{
	javaLoad->release();

	if (manager)
		manager->manifestDeleted (this);

	for (ManifestClass *p; p = classes;)
		{
		classes = p->next;
		delete p;
		}
}

const char* Manifest::match(const char *string, const char *line)
{
	while (*string && *line)
		if (*string++ != *line++)
			return NULL;

	while (*line == ' ' || *line == '\t')
		++line;

	return line;
}

bool Manifest::parse(const char *text)
{
	// Parse manifest proper, stopping at signature

	valid = false;
	JString className;
	JString appClass;
	//UCHAR digest [DIGEST_LENGTH];
	char temp [100];
	const char *p = text;

	while (*p)
		{
		if (match ("----", p))
			break;
		const char *value, *end;
		if (p [0] == '\n')
			className = "";
		else if (value = match ("Name:", p))
			{
			if (end = strpbrk (value, ". \n"))
				className = JString (value, end - value);
			}
		else if (value = match ("ApplicationClass:", p))
			{
			if (end = strpbrk (value, ". \n"))
				appClass = JString (value, end - value);
			if (appClass != name)
				return invalidManifest ("wrong application class");
			}
		else if (value = match ("SHA-Digest:", p))
			{
			if (className != "" && (end = strchr (value, '\n')))
				{
				int l = end - value;
				memcpy (temp, value, l);
				temp [l] = 0;
				//Crypto::fromBase64 (temp, sizeof (digest), digest);
				DecodeTransform<StringTransform,Base64Transform> decode(temp);
				UCHAR digest [DIGEST_LENGTH];
				decode.get(sizeof(digest), digest);
				ManifestClass *manifestClass = new ManifestClass (className, digest);
				manifestClass->next = classes;
				classes = manifestClass;
				}
			}
		while (*p && *p++ != '\n')
			;
		}

	if (!(p = find (AUTHORITY, text)) || p == text)
		return invalidManifest ("missing authority");

	//JString manifest = JString (text, p - text - 1);
	JString manifest = JString (text, p - text);

	// Validate signature

	if (!validateSignature (p - 1))
		return invalidManifest ("invalid signature");

	JString digestText = getAttribute ("Digest", signature);
	//Digest::computeDigest (manifest, p - text, digest);

	if (!validateDigest (digestText, manifest))
		{
		validateDigest (digestText, manifest);
		return invalidManifest ("inconsistent manifest digest");
		}

	valid = true;

	for (ManifestClass *manifestClass = classes; manifestClass;
		 manifestClass = manifestClass->next)
		manager->insert (manifestClass);

	return true;
}

JString Manifest::getManifest()
{
	javaLoad->open();
	JString manifest = getManifest (javaLoad);
	javaLoad->close();

	return manifest;
}

JString Manifest::getManifest(JavaLoad *javaLoad)
{
	char buffer [2000];
	Stream stream (sizeof (buffer));
	javaLoad->rewind();

	for (int length; (length = javaLoad->read (buffer, sizeof (buffer)));)
		{
		char *p = buffer;
		for (char *q = buffer, *end = buffer + length; q < end; q++)
			if (*q != '\r')
				*p++ = *q;
		stream.putSegment (p - buffer, buffer, true);
		}

	return stream.getJString();
}

bool Manifest::invalidManifest(const char* why)
{
	Log::log ("Manifest %s invalid: %s\n", (const char*) name, why);

	return false;
}
