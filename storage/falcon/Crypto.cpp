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

// Crypto.cpp: implementation of the Crypto class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include "Engine.h"
#include "Crypto.h"
//#include "libdes/des.h"
#include "DESTransform.h"
#include "DESKeyTransform.h"

//using namespace CryptoPP;
static char digits [64];
static char lookup [128];

static int initialize();
static int foo = initialize();


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int initialize()
{
	int n = 0;
	int c;

	for (const char *p = "./"; *p;)
		{
		lookup [*p] = n;
		digits [n++] = *p++;
		}

	for (c = '0'; c <= '9'; ++c)
		{
		lookup [c] = n;
		digits [n++] = c;
		}

	for (c = 'A'; c <= 'Z'; ++c)
		{
		lookup [c] = n;
		digits [n++] = c;
		}

	for (c = 'a'; c <= 'z'; ++c)
		{
		lookup [c] = n;
		digits [n++] = c;
		}

	/***
	const char *source = "I wish I were an Oscar Meyer weiner.";
	char gook [256];
	char clearText [256];
	const char *key = "well, duh";
	int length = Crypto::DESencryption (key, strlen (source) + 1, source, gook);
	Crypto::DESdecryption (key, length, gook, clearText);
	***/

	return 0;
}	

Crypto::Crypto()
{

}

Crypto::~Crypto()
{

}

JString Crypto::toBase64(int length, void *gook, int count)
{
	JString string;
	int max = length * 4 / 3 + 4;

	if (count)
		max += (length + count - 1) / count;

	char *out = string.getBuffer (max);
	int bits = 0;
	UCHAR *p = (UCHAR*) gook, *end = p + length;
	unsigned long reg = 0;
	int run = 0;

	for (int bitsRemaining = length * 8; bitsRemaining > 0; bitsRemaining -= 6)
		{
		if (bits < 6)
			{
			reg <<= 8;
			bits += 8;
			if (p < end)
				reg |= *p++;
			}
		bits -= 6;
		*out++ = digits [(reg >> bits) & 0x3f];
		if (count && ++run == count)
			{
			*out++ = '\n';
			run = 0;
			}
		}

	int pad = length % 3;

	if (pad > 0)
		{
		*out++ = '*';
		if (pad == 1)
			*out++ = '*';
		}

	*out = 0;			
	string.releaseBuffer();
	int l = string.length();

	return string;
}

int Crypto::fromBase64(const char *base64, int bufferLength, UCHAR *buffer)
{
	UCHAR *binary = buffer;
	UCHAR *end = buffer + bufferLength;
	const char *p = base64;
	int bits = 0;
	unsigned long reg = 0;

	if (*p)
		{
		for (;;)
			{
			if (bits >= 8)
				{
				bits -= 8;
				if (binary >= end)
					return -1;
				*binary++ = (UCHAR) (reg >> bits);
				}
			char c = *p++;
			if (!c)
				break;
			UCHAR n = lookup [c];
			if (digits [n] == c)
				{
				reg <<= 6;
				reg |= n;
				bits += 6;
				}
			else if (c == '*')
				bits = 0;
			}
		if (bits && binary < end)
			*binary++ = bits;
		}

	return binary - buffer;
}

int Crypto::DESencryption(const UCHAR *key, int length, const char *input, char *output)
{
	
	//return DESprocess (true, key, length, input, output);
}

int Crypto::DESdecryption(const UCHAR *key, int length, const char *input, char *output)
{
	//return DESprocess (false, key, length, input, output);
}

int Crypto::DESBlockSize()
{
	return 8;
}

int Crypto::DESprocess(bool encrypt, const UCHAR *key, int length, const char *input, char *output)
{
	int32 in[2], out[2];
	const char *p = input;
	char *q = output;
	des_key_schedule schedule;
	des_set_key((des_cblock*) key, schedule);

	for (int remaining = length; remaining > 0; p += 8, q += 8, remaining -=8)
		{
		if (remaining >= 8)
			memcpy (&in, p, 8);
		else
			{
			memcpy (&in, p, remaining);
			memset ((char*) &in + remaining, 0, 8 - remaining);
			}
		des_ecb_encrypt((des_cblock*) in, (des_cblock*) out, schedule, encrypt);
		memcpy (q, &out, 8);
		}

	return q - output;
}

void Crypto::DECStringToKey(const char *string, UCHAR key[8])
{
	memset (key, 0, 8);
	int length = strlen (string);

	for (int n = 0; n < length; ++n)
		{
		UCHAR c = (UCHAR) string [n];
		if ((n % 16) < 8)
			key [n % 8] ^= c << 1;
		else
			{
			c = (UCHAR) (((c << 4) & 0xf0) | ((c >> 4) & 0x0f));
			c = (UCHAR) (((c << 2) & 0xcc) | ((c >> 2) & 0x33));
			c = (UCHAR) (((c << 1) & 0xaa) | ((c >> 1) & 0x55));
			key [7 - (n % 8)] ^= c;
			}
		}
}
