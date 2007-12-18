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

// Unicode.h: interface for the Unicode class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_UNICODE_H__D945DFFA_8083_4FF5_B070_6D3E27BFE640__INCLUDED_)
#define AFX_UNICODE_H__D945DFFA_8083_4FF5_B070_6D3E27BFE640__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef unsigned char	*Utf8Ptr;

class Unicode  
{
public:
	static unsigned short getUnicode(unsigned int c);
	static void fixup(const char *from, char *to);
	static int validate(const char *string);
	static char* convert(int length, const char *stuff, char *utf8);
	static int getUtf8Length(int length, const unsigned short *chars);
	static int getUtf8Length(int length, const char *utf8);
	static unsigned short* convert(const char *utf8, unsigned short *utf16);
	static unsigned short* convert(int length, const char *utf8, unsigned short *utf16);
	static void convert(int inLength, const char **inPtr, int outLength, char **outPtr);
	static int getNumberCharacters(int length, const char *ptr);
	static int getNumberCharacters(const char *p);
	static UCHAR* convert(int length, const unsigned short *utf16, char *utf8);


	static int getUtf8Length(uint32 code)
		{
		return (code <= 0x7f)      ? 1 :
			   (code <= 0x7ff)     ? 2 :
			   (code <= 0xffff)    ? 3 :
			   (code <= 0x1fffff)  ? 4 :
			   (code <= 0x3ffffff) ? 5 : 6;
		}
};

#endif // !defined(AFX_UNICODE_H__D945DFFA_8083_4FF5_B070_6D3E27BFE640__INCLUDED_)
