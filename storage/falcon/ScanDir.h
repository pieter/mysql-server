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

// ScanDir.h: interface for the ScanDir class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCANDIR_H__D3AE9FF3_1295_11D6_B8F7_00E0180AC49E__INCLUDED_)
#define AFX_SCANDIR_H__D3AE9FF3_1295_11D6_B8F7_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _WIN32
#ifndef _WINDOWS_
#undef ERROR
#include <windows.h>
#endif
#else
#include <dirent.h>
#endif


class ScanDir  
{
public:
	bool isDots();
	const char* getFilePath();
	bool isDirectory();
	bool match (const char *pattern, const char *name);
	const char* getFileName();
	bool next();
	ScanDir(const char *dir, const char *pattern);
	virtual ~ScanDir();

	JString	directory;
	JString	pattern;
	JString	fileName;
	JString	filePath;
#ifdef _WIN32
	WIN32_FIND_DATA	data;
	HANDLE			handle;
#else
	DIR				*dir;
	dirent			*data;
#endif
};

#endif // !defined(AFX_SCANDIR_H__D3AE9FF3_1295_11D6_B8F7_00E0180AC49E__INCLUDED_)
