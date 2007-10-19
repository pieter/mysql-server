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

// Digest.cpp: implementation of the Digest class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "Digest.h"
#include "StringTransform.h"
#include "EncryptTransform.h"
#include "SHATransform.h"
#include "Base64Transform.h"
#include "EncodeTransform.h"
#include "TransformUtil.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


JString Digest::computeDigest(const char *text)
{
	EncryptTransform<StringTransform,SHATransform,Base64Transform>
		transform(text, 0);

	return TransformUtil::getString(&transform);
}

void Digest::computeDigest(const char *text, int length, UCHAR *digest)
{
	EncodeTransform<StringTransform,SHATransform> 
		transform(length, (UCHAR*) text);
	transform.get(DIGEST_LENGTH, digest);
}
