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

// Images.h: interface for the Images class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IMAGES_H__42BED353_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
#define AFX_IMAGES_H__42BED353_8E56_11D3_AB7E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define IMAGE_HASH_SIZE		101

class Database;
class Image;
class ImageManager;
class Extends;
class Application;
class Module;

class Images  
{
public:
	void addModule (Module *module);
	void deleteImage (const char *imageName);
	void load();
	void insert (Image *image, bool rehash);
	Image* findImage (const char *imageName);
	void rehash();
	Images(ImageManager *manager, const char *applicationName, Application *application);
	virtual ~Images();

	const char	*name;				// application name
	JString		dbName;				// name in database
	Database	*database;
	Image		*images;
	Image		*hashTable [IMAGE_HASH_SIZE];
	Images		*parent;
	Images		*children;
	Images		*sibling;
	Images		*collision;
	Images		*modules;
	Images		*nextModule;
	Images		*primary;
	ImageManager *manager;
};

#endif // !defined(AFX_IMAGES_H__42BED353_8E56_11D3_AB7E_0000C01D2301__INCLUDED_)
