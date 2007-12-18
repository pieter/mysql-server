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

// BigInt.cpp: implementation of the BigInt class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include "Engine.h"
#include "BigInt.h"
#include "Value.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define MPBASE 32
#define B 0x100000000ull // Number base (MPBASE bits)

// Workaround for Visual Studio compiler bug where 32-bit shift right fails in optimized binaries.
// http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=106414
// TBD: Remove this macro when Pushbuild is upgraded to Visual Studio 2005 SP1

#ifdef _WIN32
#define SHR(x, y) ((y) < 32 ? (x) >> (y) : ((x) >> 31 >> 1))
#else
#define SHR(x, y) ((x) >> (y))
#endif

static BigInt* powerTable[maxPowerOfTen];
static bool powerTableInit;

static const int powersOfTen [] = {
	1,				// 0
	10,				// 1
	100,			// 2
	1000,			// 3
	10000,			// 4
	100000,			// 5
	1000000,		// 6
	10000000,		// 7
	100000000,		// 8
	1000000000		// 9
	};

static int byteShifts [] = { 24, 16, 8, 0 };

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BigInt::BigInt()
{
	clear();
}

BigInt::~BigInt()
{

}

BigInt::BigInt(BigInt *bigInt)
{
	set(bigInt);
}

void BigInt::set(BigInt *bigInt)
{
	neg = bigInt->neg;
	length = bigInt->length;
	scale = bigInt->scale;

	for (int n = 0; n < length; ++n)
		words[n] = bigInt->words[n];
}

void BigInt::set(int64 value, int valueScale)
{
	scale = valueScale;
	neg = value < 0;
	length = 0;

	if (value)
		{
		if (neg)
			value = -value;

		words[length++] = LOW_WORD(value);
		BigWord highWord = HIGH_WORD(value);

		if (highWord)
			words[length++] = highWord;
		}
}

void BigInt::subtract(int index, BigWord value)
{
	while (value)
		{
		BigDWord acc = FETCH_WORD(index) - value;
		words[index++] = LOW_WORD(acc);
		value = HIGH_WORD(acc);
		}

	if (index > length)
		length = index;
}

int64 BigInt::getInt()
{
	int64 value = ((int64) FETCH_WORD(1) << bigWordBits) + FETCH_WORD(0);

	return (neg) ? -value : value;
}

void BigInt::print(const char *msg)
{
	char temp[128];
	getString(sizeof(temp), temp);
	printf ("%s(%d): %s\n", msg, length, temp);
}

void BigInt::multiply(BigWord value)
{
	BigInt org(this);
	length = 0;

	for (int n = 0; n < org.length; ++n)
		{
		BigDWord acc = (BigDWord) value * org.words[n];
		add(n, LOW_WORD(acc));
		add(n + 1, HIGH_WORD(acc));
		}

}

void BigInt::add(int64 value)
{
	add(0, LOW_WORD(value));
	add(1, HIGH_WORD(value));
}

BigWord BigInt::divide(BigWord value)
{
	BigWord rem = 0;

	for (int n = length; n; --n)
		{
		BigDWord acc = GET_INT64(rem, words[n - 1]);
		words[n - 1] = (BigWord) (acc / value);
		rem = (BigWord) (acc % value);
		}

	normalize();
	
	return rem;
}

void BigInt::set(BigWord value)
{
	scale = 0;
	neg = false;
	
	if (value)
		{
		words[0] = value;
		length = 1;
		}
	else
		length = 0;
		
}

int BigInt::getByteLength(void)
{
	if (length == 0)
		return 0;
	
	BigWord highWord = words[length - 1];

	for (int n = 0; n < 4; ++n)
		{
		unsigned char byte = (unsigned char) (highWord >> byteShifts[n]);
		
		if (byte)
			return length * sizeof(BigWord) - n + ((byte & 0x80) ? 1 : 0);
		}
		
	return length * sizeof(BigWord);
}

void BigInt::getBytes(char* bytes)
{
	if (length == 0)
		return;

	char *p = bytes;
	int position = length;
	BigWord highWord = words[--position];
	bool sig = false;
	
	// Handle first word -- skip zero high order bytes, handle sign, etc
	
	for (int n = 0; n < 4; ++n)
		{
		char byte = (char) (highWord >> byteShifts[n]);
		
		if (sig)
			*p++ = byte;
		else if (byte)
			{
			if (byte & 0x80)
				*p++ = 0;
			
			*p++ = byte;
			sig = true;
			}
		}
	
	// The rest of the words are boring -- just squirt out the byte in the right order
	
	while (position > 0)
		{
		BigWord word = words[--position];
		
		for (int m = 0; m < 4; ++m)
			*p++ = (char) (word >> byteShifts[m]);
		}
	
	if (neg)
		*bytes |= 0x80;
}

void BigInt::setBytes(int scl, int byteLength, const char* bytes)
{
	if (byteLength == 0)
		{
		clear();
		
		return;
		}
		
	scale = scl;
	neg = (*bytes & 0x80) != 0;
	const UCHAR *p = (const UCHAR*) bytes;
	int position = (byteLength + sizeof(BigWord) - 1) / sizeof(BigWord);
	length = (short) position;
	int partial = byteLength % sizeof(BigWord);
	
	if (partial == 0)
		partial = sizeof(BigWord);

	BigWord word = *p++ & 0x7f;
	
	for (int n = 1; n < partial; ++n)
		{
		word <<= 8;
		word |= *p++;
		}

	words[--position] = word;
	
	while (position > 0)
		{
		for (uint n = 0; n < sizeof(BigWord); ++n)
			{
			word <<= 8;
			word |= *p++;
			}
			
		words[--position] = word;
		}

	normalize();
}

void BigInt::scaleTo(int toScale)
{
	BigWord intResult;
	BigInt bigInt;
	
	if (scale <= 9)
		scaleTo(toScale, &intResult);
	else
		scaleTo(toScale, &bigInt);
}

BigInt BigInt::scaleTo(int toScale, BigInt* result)
{
	int delta = toScale - scale;
	scale = toScale;
	result->clear();
	
	if (!powerTableInit)
		buildPowerTable();
	
	if (delta == 0)
		return *result;
	
	if (delta > 9)
		{
		*result = divide(powerTable[delta]);
		return *result;
		}
	
	while (delta < -9)
		{
		multiply(powersOfTen[9]);
		delta += 9;
		}

	multiply(powersOfTen[-delta]);
	
	return *result;
}

BigWord BigInt::scaleTo(int toScale, BigWord* result)
{
	int delta = toScale - scale;
	scale = toScale;
	*result = 0;
	
	if (delta == 0)
		return *result;
	
	if (delta > 0)
		{
		while (delta > 9)
			{
			divide(powersOfTen[9]);
			delta -= 9;
			}
			
		*result = divide(powersOfTen[delta]);
		return *result;
		}
	
	while (delta < -9)
		{
		multiply(powersOfTen[9]);
		delta += 9;
		}

	multiply(powersOfTen[-delta]);
	
	return *result;
}

void BigInt::setString(int stringLength, const char* string)
{
	length = 0;
	neg = false;
	scale = 0;
	bool decimalPoint = false;
	char c;
	
	for (const char *p = string, *end = string + stringLength; p < end && (c = *p++);)
		{
		if (c >= '0' && c <= '9')
			{
			multiply(10);
			add(0, c - '0');
			
			if (decimalPoint)
				--scale;
			}
		else if (c == '-')
			neg = !neg;
		else if (c == '.')
			decimalPoint = true;
		}	
}

void BigInt::getString(int bufferLength, char* buffer)
{
	char temp[128];
	char *p = temp;
	BigInt number(this);
	
	while (number.length)
		{
		int n = number.divide(10);
		*p++ = '0' + n;
		}
	
	char *q = buffer;
	char *decimalPoint = (scale) ? buffer + (p - temp) + scale : buffer + bufferLength;
		
	if (neg)
		*q++ = '-';
	
	while (p > temp)
		{
		if (q == decimalPoint)
			*q++ = '.';
		
		*q++ = *--p;
		}
	
	*q = 0;
}

double BigInt::getDouble(void)
{
	double d = 0;
	
	for (int n = length - 1; n >= 0; --n)
		d = d * 4294967296. + words[n];
	
	if (scale)
		d = Value::scaleDouble(d, -scale);
	
	/***
	if (scale < 0)
		d /= powersOfTen[-scale];
	else if (scale > 0)
		d *= powersOfTen[scale];
	***/
	
	return (neg) ? -d : d;
}

// If we're higher than the other guy, return 1; if lower, return -1; if equal, return 0

int BigInt::compare(BigInt* bigInt)
{
	// If we're zero, do a quick look at the other guy
	
	if (length == 0)
		{
		if (bigInt->length == 0)
			return 0;
		
		return (bigInt->neg) ? 1 : -1;
		}
	
	// And if the other guy is zero, look at it
	
	if (bigInt->length == 0)
		return (neg) ? -1 : 1;
	
	// If we have different signs, things are also easy
	
	if (neg != bigInt->neg)
		return (neg) ? -1 : 1;
	
	// If the scales are the same, we get off easily
	
	int ret;
	
	if (scale == bigInt->scale)
		ret = compareRaw(bigInt);
	else if (scale < bigInt->scale)
		{
		BigInt temp(bigInt);
		temp.scaleTo(scale);
		ret = compareRaw(&temp);
		}
	else
		{
		BigInt temp(this);
		temp.scaleTo(bigInt->scale);
		ret = temp.compareRaw(bigInt);
		}
	
	return (neg) ? -ret : ret;
}

int BigInt::fitsInInt64(void)
{
	if (length < 2)
		return 1;
	
	if (length > 2)
		return 0;
	
	return ((int32)words[1] >= 0) ? 1 : 0;
}

// Count number of leading zero bits
// TBD: Use native CLZ (for gcc use _builtin_clz, for windows see below)

int BigInt::nlz(uint64 x)
{
   int n;

   if (x == 0) return(64);
   n = 0;
   if (x <= 0x00000000FFFFFFFFull) {n = n + 32; x = x << 32;}
   if (x <= 0x0000FFFFFFFFFFFFull) {n = n + 16; x = x << 16;}
   if (x <= 0x00FFFFFFFFFFFFFFull) {n = n + 8;  x = x << 8;}
   if (x <= 0x0FFFFFFFFFFFFFFFull) {n = n + 4;  x = x << 4;}
   if (x <= 0x3FFFFFFFFFFFFFFFull) {n = n + 2;  x = x << 2;}
   if (x <= 0x7FFFFFFFFFFFFFFFull) {n = n + 1;}
   return n;
}

#if 0
uint32 clz (uint32 a)
{
	_asm
		{
		mov	eax, [a]
		bsr	eax, eax
		xor	eax, 31
		mov	[a], eax
		}

	return a;
}
#endif


BigInt BigInt::divide(const BigInt* divisor)
{
	BigInt quotient;
	BigInt remainder;
	
	// If the divisor is larger than the dividend (this), then set the quotient
	// to 0 return the dividend as the remainder.
	
	if (divisor->length > length)
		{
		remainder = *this;
		clear();
		}
	else
		{
		divide(this, divisor, &quotient, &remainder);
		memcpy(this->words, quotient.words, sizeof(this->words));
		this->length = quotient.length;
		}
	
	return(remainder);
}

// Arbitrary precision division based upon Knuth Algorithm D

int BigInt::divide(const BigInt* dividend, const BigInt* divisor, BigInt* quotient, BigInt* remainder)
	{
	uint32* q = quotient->words;
	uint32* r = remainder->words;
	const uint32* u = dividend->words;
	const uint32* v = divisor->words;
	int m = dividend->length;
	int n = divisor->length;

	uint32 *un, *vn;		// normalized form of u, v
	uint64 qhat;			// estimated quotient digit
	uint64 rhat;			// remainder
	uint64 p;				// product of two digits
	int i, j;
	int64 t, k;

	quotient->clear();
	remainder->clear();

	// Check for invalid parameters
	
	if (m < n || n <= 0 || v[n-1] == 0)
		return(1);

	// Take care of the case of a single-digit divisor
	
	if (n == 1)	
		{
		k = 0;
		
		for (j = m - 1; j >= 0; j--)
			{
			q[j] = (uint32)((k*B + u[j]) / v[0]);
			k = (k*B + u[j]) - q[j] *v[0];
			}

		if (r != NULL) r[0] = (uint32)k;

		// Take a best guess at the quotient length, adjust as necessary
		
		quotient->length = m - n + 1;
		remainder->length = n;
		quotient->normalize();
		remainder->normalize();

		return(0);
		}

	// Normalize by shifting v left just enough so that its high-order bit is on,
	// and shift u left the same amount. We may have to append a high-order digit
	// on the dividend; we do that unconditionally.

	// Step D1
	
	int s = nlz(v[n-1]) - MPBASE; // 0 <= s <= MPBASE.
	int shift = MPBASE - s;
	
	vn = (uint32 *)malloc(4*n);

	// Workaround for compiler bug
	
	for (i = n - 1; i > 0; i--)
		//vn[i] = (v[i] << s) | (uint32)(((uint64)v[i-1]) >> shift);
		vn[i] = (v[i] << s) | (uint32)SHR(uint64(v[i-1]), shift);

	vn[0] = v[0] << s;

	un = (uint32 *)malloc(4*(m+1));
//	un[m] = (uint32)(((uint64)u[m-1]) >> shift);
	un[m] = (uint32)SHR(uint64(u[m-1]), shift);

	for (i = m - 1; i > 0; i--)
		un[i] = (u[i] << s) | (uint32)SHR(uint64(u[i-1]), shift);
		//un[i] = (u[i] << s) | (uint32)(((uint64)u[i-1]) >> shift);

	un[0] = u[0] << s;

	// Step D2: Main loop
	
	for (j = m - n; j >= 0; j--)
		{
		// Step D3: Compute estimate qhat of q[j]
		
		qhat = un[j+n]*B;
		qhat += un[j+n-1];
		qhat /= vn[n-1];
		//	qhat = (un[j+n]*B + un[j+n-1]) / vn[n-1];

		rhat = un[j+n]*B;
		rhat += un[j+n-1];
		uint64 q2 = qhat*vn[n-1];
		rhat -= q2;
		//	rhat = (un[j+n]*B + un[j+n-1]) - qhat*vn[n-1];

		again:

		if (qhat >= B || qhat*vn[n-2] > B*rhat + un[j+n-2])
			{
			qhat = qhat - 1;
			rhat = rhat + vn[n-1];
			if (rhat < B) goto again;
			}

		// Step D4: Multiply and subtract
		
		k = 0;
		
		for (i = 0; i < n; i++)
			{
			p = qhat*vn[i];
			t = un[i+j] - k;
			t -= (p & 0xFFFFFFFF);
			//t = un[i+j] - k - (p & 0xFFFFFFFF);
			
			un[i+j] = (uint32)t;
			//k = (p >> MPBASE) - (t >> MPBASE);
			k = SHR(p, MPBASE) - SHR(t, MPBASE);
			}

		t = un[j+n] - k;
		un[j+n] = (uint32)t;

		// Step D5: Store quotient digit
		
		q[j] = (uint32)qhat;
		
		if (t < 0)
			{
			// Step D6: If we subtracted too much, add back
			
			q[j] = q[j] - 1;
			k = 0;
			
			for (i = 0; i < n; i++)
				{
				t = (uint64)un[i+j] + (uint64)vn[i] + k;
				//t = un[i+j] + vn[i] + k;
				un[i+j] = (uint32)t;
				//k = ((uint64)t) >> MPBASE;
				k = SHR(uint64(t), MPBASE);
				}
				
			un[j+n] = (uint32) (un[j+n] + k);
			}

		} // Step D7: End j

	// Step D8: Unnormalize remainder
	
	if (r != NULL)
		{
		for (i = 0; i < n; i++)
			r[i] = (uint32)SHR(un[i], s) | (uint32)(uint64(un[i+1]) << shift);
			//r[i] = (un[i] >> s) | (uint32)(((uint64)un[i+1]) << shift);
		}

	// Take a best guess at the quotient length, adjust as necessary
	
	quotient->length = m - n + 1;
	remainder->length = n;
	quotient->normalize();
	remainder->normalize();

	free(un);
	free(vn);
	return(0);
	}

// Generate a table of BigInt powers of 10

void BigInt::buildPowerTable()
	{
	BigInt* bigInt = ::new BigInt;
	bigInt->set(powersOfTen[9], 0);
	powerTable[9] = bigInt;

	for (int i = 10; i < maxPowerOfTen; i++)
		{
		powerTable[i] = ::new BigInt(powerTable[i-1]);
		powerTable[i]->multiply(10);
		}

	powerTableInit = true;
	}
