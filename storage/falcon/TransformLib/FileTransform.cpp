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

// FileTransform.cpp: implementation of the FileTransform class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "FileTransform.h"
#include "TransformException.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FileTransform::FileTransform()
{
	file = NULL;
}

FileTransform::~FileTransform()
{
	close();
}

FileTransform::FileTransform(const char *fileName, bool binaryFile)
{
	file = NULL;
	setString(fileName, binaryFile);
}

void FileTransform::setString(const char *fileName, bool binaryFile)
{
	close();

	if (!(file = fopen(fileName, (binaryFile) ? "rb" : "r")))
		throw TransformException("can't open \"%d\"", fileName);

	struct stat buffer;
	//fstat(((FILE*) file)->_file, &buffer);
	stat(fileName, &buffer);
	length = buffer.st_size;
	offset = 0;
}

unsigned int FileTransform::getLength()
{
	return (unsigned int) (length - offset);
}

unsigned int FileTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	if (!file)
		throw TransformException("file not open");

	if (offset >= length)
		return 0;

	IPTR len = fread(buffer, 1, bufferLength, (FILE*) file);

	if (len < 0)
		throw TransformException("file read error");

	offset += len;

	return (unsigned int) len;
}

void FileTransform::close()
{
	if (file)
		{
		fclose ((FILE*) file);
		file = NULL;
		}
}

void FileTransform::reset()
{
	close();
}

void FileTransform::setString(unsigned int length, const UCHAR *data, bool flag)
{
	char fileName [256];

	if (length >= sizeof(fileName))
		throw TransformException("file name too long");

	memcpy(fileName, data, length);
	fileName[length] = 0;
	setString(fileName, flag);
}
