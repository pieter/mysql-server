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

// Image.cpp: implementation of the Image class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Image.h"
#include "Interlock.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Image::Image(const char *imageName, int imageWidth, int imageHeight, const char *imageAlias, Images *parent)
{
	name = imageName;
	width = imageWidth;
	height = imageHeight;
	alias = imageAlias;
	useCount = 1;
	images = parent;
}

Image::~Image()
{

}

void Image::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Image::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}
