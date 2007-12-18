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

// Image.h: interface for the Image class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IMAGE_H__42BED352_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
#define AFX_IMAGE_H__42BED352_8E56_11D3_AB7E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class Database;
class Images;

class Image  
{
public:
	void release();
	void addRef();
	Image(const char *imageName, int imageWidth, int imageHeight, const char *imageAlias, Images *parent);

protected:
	virtual ~Image();

public:
	const char		*name;
	JString			alias;
	int				width;
	int				height;
	Image			*collision;
	Image			*next;
	Images			*images;
	volatile INTERLOCK_TYPE	useCount;
};

#endif // !defined(AFX_IMAGE_H__42BED352_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
