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

// Unicode.cpp: implementation of the Unicode class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Unicode.h"

static const unsigned char utf8Lengths [256] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 00 - 0f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 10 - 1f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 20 - 2f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 30 - 3f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 40 - 4f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 50 - 5f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 60 - 6f
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // 70 - 7f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 80 - 8f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 90 - 9f
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // a0 - af
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // b0 - bf
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   // c0 - cf
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   // d0 - df
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,   // e0 - ef
        4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7    // 07 - ff
        };

static const unsigned char utf8Values [256] = {
          0,   1,   2,   3,   4,   5,   6,   7,   // 00 - 07
          8,   9,  10,  11,  12,  13,  14,  15,   // 08 - 0f
         16,  17,  18,  19,  20,  21,  22,  23,   // 10 - 17
         24,  25,  26,  27,  28,  29,  30,  31,   // 18 - 1f
         32,  33,  34,  35,  36,  37,  38,  39,   // 20 - 27
         40,  41,  42,  43,  44,  45,  46,  47,   // 28 - 2f
         48,  49,  50,  51,  52,  53,  54,  55,   // 30 - 37
         56,  57,  58,  59,  60,  61,  62,  63,   // 38 - 3f
         64,  65,  66,  67,  68,  69,  70,  71,   // 40 - 47
         72,  73,  74,  75,  76,  77,  78,  79,   // 48 - 4f
         80,  81,  82,  83,  84,  85,  86,  87,   // 50 - 57
         88,  89,  90,  91,  92,  93,  94,  95,   // 58 - 5f
         96,  97,  98,  99, 100, 101, 102, 103,   // 60 - 67
        104, 105, 106, 107, 108, 109, 110, 111,   // 68 - 6f
        112, 113, 114, 115, 116, 117, 118, 119,   // 70 - 77
        120, 121, 122, 123, 124, 125, 126, 127,   // 78 - 7f
          0,   0,   0,   0,   0,   0,   0,   0,   // 80 - 87
          0,   0,   0,   0,   0,   0,   0,   0,   // 88 - 8f
          0,   0,   0,   0,   0,   0,   0,   0,   // 90 - 97
          0,   0,   0,   0,   0,   0,   0,   0,   // 98 - 9f
          0,   0,   0,   0,   0,   0,   0,   0,   // a0 - a7
          0,   0,   0,   0,   0,   0,   0,   0,   // a8 - af
          0,   0,   0,   0,   0,   0,   0,   0,   // b0 - b7
          0,   0,   0,   0,   0,   0,   0,   0,   // b8 - bf
          0,   1,   2,   3,   4,   5,   6,   7,   // c0 - c7
          8,   9,  10,  11,  12,  13,  14,  15,   // c8 - cf
         16,  17,  18,  19,  20,  21,  22,  23,   // d0 - d7
         24,  25,  26,  27,  28,  29,  30,  31,   // d8 - df
          0,   1,   2,   3,   4,   5,   6,   7,   // e0 - e7
          8,   9,  10,  11,  12,  13,  14,  15,   // e8 - ef
          0,   1,   2,   3,   4,   5,   6,   7,   // f0 - f7
          0,   1,   2,   3,   0,   1,   0,   1    // 01 - ff
        };

static const int utf8Flags [] = {
	0,		// 0	illegal
	0,		// 1	0xxxxxxx
	0xC0,	// 2	110xxxxx 10xxxxxx
	0xE0,	// 3	1110xxxx 10xxxxxx 10xxxxxx
	0xF0,	// 4	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	0xF8,	// 5	111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
	0xFC	// 6	1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
	};
			
static const unsigned short win1252toUnicode [32] = {
	0x20ac,		// Unicode equiv to win1232 0x80 128
	' ',		// 81
	0x201a,		// 82
	0x0192,		// 83
	0x201e,		// 84	
	0x2026,		// 85	
	0x2020,		// 86	
	0x2021,		// 87	
	0x02c6,		// 88	
	0x02c6,		// 89	
	0x0160,		// 8a	
	0x2039,		// 8b	
	0x0152,		// 8c	
	' ',		// 8d	
	0x007e,		// 8e
	0x007f,		// 8f
	' ',		// 90
	0x2018,		// 91
	0x2019,		// 92
	0x201c,		// 93
	0x201d,		// 94
	0x2022,		// 95
	0x2013,		// 96
	0x2014,		// 97
	0x02dc,		// 98
	0x2122,		// 99
	0x0161,		// 9a
	0x203a,		// 9b
	0x0153,		// 9c
	' ',		// 9d
	0x017e,		// 9e
	0x0178,		// 9f
	};
				
//#define BOOTSTRAP

#ifdef BOOTSTRAP
#include <stdio.h>

main()
{
	int n;
	const char *sep = "";
	printf ("static const unsigned char utf8Lengths [256] = {\n\t");

	for (n = 0; n < 256; ++n)
		{
		int l;
		for (l = 0; (l < 7) && (n & (1 << (7 - l))); ++l)
			;
		if (l == 0)
			l = 1;
		else if (l == 1)
			l = 0;
		printf (sep, n - 16, n - 1);
		printf ("%d", l);
		sep = ((n + 1) % 16 == 0) ? ",   // %.2x - %.2x\n\t" : ", ";
		}

	printf ("    // %.2x - %.2x\n\t};\n");

	printf ("static const unsigned char utf8Values [256] = {\n\t");
	sep = "";

	for (n = 0; n < 256; ++n)
		{
		int l;
		for (l = 0; (l < 7) && (n & (1 << (7 - l))); ++l)
			;
		if (l == 0)
			l = 1;
		else if (l == 1)
			l = 0;
		//printf ("%d %d %x\n", n, l, (0xff << (8 - l)));
		int value = n & ~((0xff << (8 - l)));
		if (l == 0)
			value = 0;
		printf (sep, n - 8, n - 1);
		printf ("%3d", value);
		sep = ((n + 1) % 8 == 0) ? ",   // %.2x - %.2x\n\t" : ", ";
		}

	printf ("    // %.2x - %.2x\n\t};\n");

	return 0;
}
#endif
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


int Unicode::getNumberCharacters(const char *chars)
{
	const UCHAR *utf8 = (UCHAR*) chars;
	int count = 0;

	while (*utf8)
		{
		++count;
		int len = utf8Lengths [*utf8];

		if (len > 1 && (utf8[1] & 0xC0) == 0x80)
			utf8 += len;
		else
			++utf8;
		/***
		if (len != 1 && (len == 0 || (utf8[1] & 0xc0) != 0x80))
			++utf8;
		else
			utf8 += len;
		***/
		}

	return count;
}

int Unicode::getNumberCharacters(int length, const char *chars)
{
	const UCHAR *utf8 = (UCHAR*) chars;
	int count = 0;

	for (const UCHAR *end = utf8 + length; utf8 < end;)
		{
		++count;
		int len = utf8Lengths [*utf8];

		if (len > 1 && (utf8[1] & 0xC0) == 0x80)
			utf8 += len;
		else
			++utf8;
		/***
		if (len != 1 && (len == 0 || (utf8[1] & 0xc0) != 0x80))
			++utf8;
		else
			utf8 += len;
		***/
		}

	return count;
}

unsigned short* Unicode::convert(int length, const char *chars, unsigned short *utf16)
{
	const UCHAR *utf8 = (UCHAR*) chars;

	for (const UCHAR *end = utf8 + length; utf8 < end;)
		{
		UCHAR c = *utf8++;
		int code = utf8Values [c];
		int len = utf8Lengths [c];

		if (len > 1 && (*utf8 & 0xC0) == 0x80)
			for (; length > 1; --length)
				code = (code << 6) | (*utf8++ & 0x3f);
		else
			code = c;

		*utf16++ = code;
		}

	return utf16;
}

unsigned short* Unicode::convert(const char *chars, unsigned short *utf16)
{
	for (const UCHAR *utf8 = (UCHAR*) chars; *utf8;)
		{
		UCHAR c = *utf8++;
		int code = utf8Values [c];
		int length = utf8Lengths [c];

		if (length > 1 && (*utf8 & 0xC0) == 0x80)
			for (; length > 1; --length)
				code = (code << 6) | (*utf8++ & 0x3f);
		else
			code = c;

		*utf16++ = code;
		}

	return utf16;
}

int Unicode::getUtf8Length(int length, const unsigned short *chars)
{
	int len = 0;

	while (--length >= 0)
		len += getUtf8Length(*chars++);

	return len;
}

UCHAR* Unicode::convert(int length, const unsigned short *utf16, char *chars)
{
	UCHAR *utf8 = (UCHAR*) chars;

	while (--length >= 0)
		{
		int code = *utf16++;
		int len = getUtf8Length(code);
		
		if (len == 1)
			*utf8++ = code;
		else
			{
			utf8 += len;
			UCHAR *p = utf8;
			
			for (int n = 1; n < len; ++n)
				{
				*--p = 0x80 | (code & 0x3f);
				code >>= 6;
				}

			*--p = utf8Flags[len] | code;
			}
		}

	return utf8;
}

int Unicode::getUtf8Length(int length, const char *chars)
{
	const UCHAR *utf8 = (UCHAR*) chars;
	int count = 0;

	for (const UCHAR *end = utf8 + length; utf8 < end;)
		{
		int l = utf8Lengths [*utf8];

		if (l != 1 && (l == 0 || (utf8[1] & 0xc0) != 0x80))
			{
			l = getUtf8Length(*utf8);
			++utf8;
			}
		else
			utf8 += l;

		count += l;
		}

	return count;
}

char* Unicode::convert(int length, const char *source, char *destination)
{
	UCHAR *out = (UCHAR*) destination;
	const UCHAR *p = (const UCHAR*) source;
	const UCHAR *start = p;

	for (const UCHAR *end = p + length; p < end;)
		{
		int len = utf8Lengths [*p];

		if (len != 1 && (len == 0 || (p[1] & 0xc0) != 0x80))
			{
			if (p > start)
				{
				memcpy(out, start, p - start);
				out += p - start;
				}
			int code = *p++;
			len = getUtf8Length(code);
			out += len;
			UCHAR *q = out;
			
			for (int n = 1; n < len; ++n)
				{
				*--q = 0x80 | (code & 0x3f);
				code >>= 6;
				}

			*--q = utf8Flags[len] | code;
			start = p;
			}
		else
			p += len;
		}

	if (p > start)
		{
		memcpy(out, start, p - start);
		out += p - start;
		}

	return (char*) out;
}

void Unicode::convert(int inLength, const char **inPtr, int outLength, char **outPtr)
{
	const UCHAR *in = (const UCHAR*) *inPtr;
	const UCHAR *endIn = in + inLength;
	UCHAR *out = (UCHAR*) *outPtr;
	const UCHAR *endOut = out + outLength;

	while (in < endIn && out < endOut)
		{
		int code = *in;
		int len = utf8Lengths [code];

		if (len != 1 && ((in[1] & 0xc0) != 0x80))
			{
			len = getUtf8Length(code);

			if (out + len > endOut)
				break;

			out += len;
			UCHAR *q = out;
			
			for (int n = 1; n < len; ++n)
				{
				*--q = 0x80 | (code & 0x3f);
				code >>= 6;
				}

			*--q = utf8Flags[len] | code;
			++in;
			}
		else
			{
			if (out + len > endOut)
				break;
			while (--len >= 0)
				*out++ = *in++;
			}
		}

	*inPtr = (const char*) in;
	*outPtr = (char*) out;
}

int Unicode::validate(const char *chars)
{
	bool valid = true;
	int count = 0;

	for (const UCHAR *utf8 = (const UCHAR*) chars; *utf8;)	
		{
		int l = utf8Lengths [*utf8];

		if (l != 1 && (l == 0 || (utf8[1] & 0xc0) != 0x80))
			{
			unsigned short code = getUnicode(*utf8);
			l = getUtf8Length(code);
			++utf8;
			valid = false;
			}
		else
			utf8 += l;

		count += l;
		}

	return (valid) ? 0 : count;
}

void Unicode::fixup(const char *from, char *to)
{
	UCHAR *out = (UCHAR*) to;
	unsigned short code;

	for (const UCHAR *utf8 = (const UCHAR*) from; (code = *utf8++);)	
		{
		int l = utf8Lengths [code];

		if (l == 1)
			*out++ = (UCHAR) code;
		else if (l == 0 || (*utf8 & 0xc0) != 0x80)
			{
			code = getUnicode(code);
			l = getUtf8Length(code);
			out += l;
			UCHAR *p = out;
			
			for (int n = 1; n < l; ++n)
				{
				*--p = 0x80 | (code & 0x3f);
				code >>= 6;
				}

			*--p = utf8Flags[l] | code;
			}
		else
			while (--l > 0)
				*out++ = *utf8++;

		}

	*out = 0;
}

unsigned short Unicode::getUnicode(unsigned int c)
{
	if (c >= 0x80 && c < 0xa0)
		return win1252toUnicode[c - 0x80];

	return c;
}
