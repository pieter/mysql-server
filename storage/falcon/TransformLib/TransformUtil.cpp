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

// TransformUtil.cpp: implementation of the TransformUtil class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Transform.h"
#include "TransformUtil.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


JString TransformUtil::getString(Transform *transform)
{
	JString string;
	int len = transform->getLength();
	char *p = string.getBuffer(len);
	len = transform->get(len, (UCHAR*) p);
	p[len] = 0;

	return string;
}

bool TransformUtil::compareDigests(Transform *transform1, Transform *transform2)
{
	UCHAR hash1[20], hash2[20];
	transform1->get(sizeof(hash1), hash1);
	transform2->get(sizeof(hash2), hash2);

	return memcmp(hash1, hash2, sizeof(hash2)) == 0;
}
