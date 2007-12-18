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

// NullTransform.cpp: implementation of the NullTransform class.
//
//////////////////////////////////////////////////////////////////////

#include "NullTransform.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NullTransform::NullTransform(Transform *src)
{
	source = src;
}

NullTransform::NullTransform(bool encrypt, Transform *src)
{
	source = src;
}

NullTransform::NullTransform(int mode, Transform *src)
{
	source = src;
}

NullTransform::~NullTransform()
{

}

unsigned int NullTransform::getLength()
{
	return source->getLength();
}

unsigned int NullTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	return source->get(bufferLength, buffer);
}

void NullTransform::reset()
{
	if (source)
		source->reset();
}
