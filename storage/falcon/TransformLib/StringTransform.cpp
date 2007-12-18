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

// StringTransform.cpp: implementation of the StringTransform class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <memory.h>
#include "StringTransform.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StringTransform::StringTransform()
{
	ptr = NULL;
	end = NULL;
	data = NULL;
}

StringTransform::~StringTransform()
{
	delete data;
}

StringTransform::StringTransform(const char *string, bool copyFlag) 
{
	data = NULL;
	setString(strlen(string), (const UCHAR*) string, copyFlag);
}

StringTransform::StringTransform(unsigned int length, const UCHAR *stuff, bool copyFlag)
{
	data = NULL;
	setString(length, stuff, copyFlag);	
}

void StringTransform::setString(UIPTR length, const UCHAR *stuff, bool copyFlag)
{
	delete [] data;
	data = NULL;

	if (copyFlag)
		{
		ptr = data = new UCHAR[length];
		memcpy (data, stuff, length);
		}
	else
		{
		ptr = stuff;
		data = NULL;
		}

	end = ptr + length;

}

unsigned int StringTransform::getLength()
{
	return (unsigned int) (end - ptr);
}

unsigned int StringTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	if (ptr >= end)
		return 0;

	int len = MIN((unsigned int) (end - ptr), bufferLength);
	memcpy(buffer, ptr, len);
	ptr += len;

	return len;
}

void StringTransform::setString(const char *string, bool copyFlag)
{
	setString(strlen(string), (const UCHAR*) string, copyFlag);
}

void StringTransform::reset()
{

}
