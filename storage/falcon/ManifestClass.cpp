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

// ManifestClass.cpp: implementation of the ManifestClass class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "ManifestClass.h"
#include "License.h"
#include "TransformUtil.h"
#include "StringTransform.h"
#include "SHATransform.h"
#include "EncodeTransform.h"
#include "TransformUtil.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ManifestClass::ManifestClass(JString className, UCHAR *classDigest)
{
	name = className;
	duplicate = NULL;
	memcpy (digest, classDigest, sizeof (digest));
}

ManifestClass::~ManifestClass()
{

}

void ManifestClass::computeDigest(int length, const UCHAR *stuff, UCHAR *digest)
{
	EncodeTransform<StringTransform,SHATransform> encode (length, stuff);
	encode.get(DIGEST_LENGTH, digest);
}

bool ManifestClass::validate(int length, const UCHAR *classFile)
{
	EncodeTransform<StringTransform,SHATransform> encode (length, classFile);
	StringTransform existing (sizeof(digest), digest);

	if (TransformUtil::compareDigests(&encode,&existing))
		return true;

	if (duplicate)
		return duplicate->validate (length, classFile);

	return false;
}
