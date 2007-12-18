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

// FileTransform.h: interface for the FileTransform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILETRANSFORM_H__E1A30038_4034_4962_8E68_207ECC113D74__INCLUDED_)
#define AFX_FILETRANSFORM_H__E1A30038_4034_4962_8E68_207ECC113D74__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Transform.h"

class FileTransform : public Transform  
{
public:
	virtual void setString(unsigned int length, const UCHAR *data, bool binaryFile = false);
	virtual void reset();
	void close();
	virtual unsigned int get(unsigned int bufferLength, UCHAR *buffer);
	virtual unsigned int getLength();
	void setString(const char *fileName, bool binaryFile = false);
	FileTransform(const char *fileName, bool binaryFile = false);
	FileTransform();
	virtual ~FileTransform();

	void	*file;
	int64	length;
	int64	offset;
};

#endif // !defined(AFX_FILETRANSFORM_H__E1A30038_4034_4962_8E68_207ECC113D74__INCLUDED_)
