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

#include "mysql_priv.h"
typedef longlong	int64;

#include "ScaledBinary.h"
#include "BigInt.h"

#define DIGITS_PER_INT			9
#define BASE					1000000000

#undef C64

#ifdef _WIN32
#define C64(x)	x##i64
#endif

#ifdef __GNUC__
#define C64(x)	x##ll
#endif

#ifndef C64
#define C64(x)	x
#endif

static const int bytesByDigit [] = {
	0,			// 0
	1,			// 10
	1,			// 100
	2,			// 1000
	2,			// 10000
	3,			// 100000
	3,			// 1000000
	4,			// 10000000
	4,			// 100000000
	4			// 1000000000
	};

static const int64 powersOfTen [] = {
	C64(1),
	C64(10),
	C64(100),
	C64(1000),
	C64(10000),
	C64(100000),
	C64(1000000),
	C64(10000000),
	C64(100000000),
	C64(1000000000),
	C64(10000000000),
	C64(100000000000),
	C64(1000000000000),
	C64(10000000000000),
	C64(100000000000000),
	C64(1000000000000000),
	C64(10000000000000000),
	C64(100000000000000000),
	C64(1000000000000000000)
	//C64(10000000000000000000)
	};
	
ScaledBinary::ScaledBinary(void)
{
}

ScaledBinary::~ScaledBinary(void)
{
}

int64 ScaledBinary::getInt64FromBinaryDecimal(const char *ptr, int precision, int scale)
{
	int position = 0;
	int64 value = getBinaryNumber(position, precision - scale, ptr, false);

	if (scale)
		{
		position += numberBytes(precision - scale);
		int64 fraction = getBinaryNumber(position, scale, ptr, true);
		value = value * powersOfTen[scale] + fraction;
		}
	
	return (ptr[0] & 0x80) ? value : -value;
}

uint ScaledBinary::getByte(int position, const char* ptr)
{
	char c = ptr[position];
	
	if (!(ptr[0] & 0x80))
		c = ~c;
	
	if (position == 0)
		c ^= 0x80;
	
	return (unsigned char) c;	
}

uint ScaledBinary::getBinaryGroup(int start, int bytes, const char* ptr)
{
	uint group = 0;
	int n;
	int end;
	
	for (n = start, end = start + bytes; n < end; ++n)
		group = (group << 8) + getByte(n, ptr);
	
	return group;
}

int ScaledBinary::numberBytes(int digits)
{
	return (digits / DIGITS_PER_INT) * sizeof(decimal_digit_t) + bytesByDigit[digits % DIGITS_PER_INT];
}

void ScaledBinary::putBinaryDecimal(int64 number, char* ptr, int precision, int scale)
{
	int64 absNumber = (number >= 0) ? number : -number;
	char *p = ptr;
	
	// Shift right by 'scale', store the integer portion
	
	putBinaryNumber(absNumber / powersOfTen[scale], precision - scale, &p, false);
	
	// Store fractional digits
	
	if (scale)
		putBinaryNumber(absNumber % powersOfTen[scale], scale, &p, true);

	// Handle sign
	
	if (number < 0)
		for (char *q = ptr; q < p; ++q)
			*q ^= -1;
	
	*ptr ^= 0x80;
}

int64 ScaledBinary::getBinaryNumber(int start, int digits, const char* ptr, bool isFraction)
{
	int64 value = 0;
	int position = start;
	int groups = digits / DIGITS_PER_INT;
	int partialDigits = digits % DIGITS_PER_INT;
	int partialBytes = bytesByDigit[partialDigits];
	
	// If not a fraction, get the high-order 4 bytes (or less), store in 'value'
	
	if (!isFraction && partialBytes)
		{
		value = getBinaryGroup(position, partialBytes, ptr);
		position += partialBytes;
		partialBytes = 0;
		}
		
	// Get remaining bytes in 4-byte groups, leftshift 'value' then add each group
	
	for (int n = 0; n < groups; ++n)
		{
		value = value * BASE + getBinaryGroup(position, sizeof(decimal_digit_t), ptr);
		position += sizeof(decimal_digit_t);
		}
	
	// If fraction, then rightshift 'value' and add in the remaining digits
	
	if (partialBytes)
		value = value * powersOfTen[partialDigits] + getBinaryGroup(position, partialBytes, ptr);
		
	return value;
}

void ScaledBinary::putBinaryNumber(int64 number, int digits, char** ptr, bool isFraction)
{
	int groups = digits / DIGITS_PER_INT;
	int partialDigits = digits % DIGITS_PER_INT;
	char *p = *ptr;
	int index = digits;
	
	if (!isFraction && partialDigits)
		{
		index -= partialDigits;
		//putBinaryGroup((uint) (number / powersOfTen[index]), partialDigits, &p);
		putBinaryGroup((uint) scaleInt64(number, index), partialDigits, &p);
		partialDigits = 0;
		}
	
	for (int n = 0; n < groups; ++n)
		{
		index -= DIGITS_PER_INT;

		if (index)
			putBinaryGroup((uint)(scaleInt64(number, index) % BASE), DIGITS_PER_INT, &p);
		else
			putBinaryGroup((uint) (number % BASE), DIGITS_PER_INT, &p);
		}
	
	if (partialDigits)
		putBinaryGroup((uint) (number % powersOfTen[partialDigits]), partialDigits, &p);
	
	*ptr = p;
}

void ScaledBinary::putBinaryGroup(uint number, int digits, char** ptr)
{
	char *p = *ptr;
	int bytes = bytesByDigit[digits];
	
	for (int n = (bytes - 1) * 8; n >= 0; n -= 8)
		*p++ = (char) (number >> n);

	*ptr = p;
}

void ScaledBinary::getBigIntFromBinaryDecimal(const char* ptr, int precision, int scale, BigInt *bigInt)
{
	int position = 0;
	getBigNumber(position, precision - scale, ptr, false, bigInt);

	if (scale)
		{
		position += numberBytes(precision - scale);
		getBigNumber(position, scale, ptr, true, bigInt);
		}
	
	bigInt->scale = -scale;
	bigInt->neg = (ptr[0] & 0x80) == 0;
}

void ScaledBinary::getBigNumber(int start, int digits, const char* ptr, bool isFraction, BigInt* bigInt)
{
	int position = start;
	int groups = digits / DIGITS_PER_INT;
	int partialDigits = digits % DIGITS_PER_INT;
	int partialBytes = bytesByDigit[partialDigits];
	
	// Store leading partial group integer digits, always < 9
	
	if (!isFraction && partialBytes)
		{
		BigWord value = getBinaryGroup(position, partialBytes, ptr);
		bigInt->set(value);
		position += partialBytes;
		partialBytes = 0;
		}
		
	// Store remaining digits by collapsing 9-digit groups into int32, then accumulate
	
	for (int n = 0; n < groups; ++n)
		{
		// Get next group of 9 decimal digits, store in int32
		
		BigWord value = getBinaryGroup(position, sizeof(decimal_digit_t), ptr);
		
		// Leftshift current value by 9 digits and add the new value
		
		bigInt->multiply(BASE);
		bigInt->add(0, value);
		position += sizeof(decimal_digit_t);
		}
	
	// If this is a fraction, then rightshift and add in leading digits
		
	if (partialBytes)
		{
		bigInt->multiply((uint32) powersOfTen[partialDigits]);
		BigWord value = getBinaryGroup(position, partialBytes, ptr);
		bigInt->add(0, value);
		}
}

void ScaledBinary::putBigInt(BigInt *bigInt, char* ptr, int precision, int scale)
{
	// Handle value as int64 if possible, although even if the number fits
	// an int64, it can only be scaled within 18 digits or less.
	
	if (bigInt->fitsInInt64() && scale < 19)
		{
		if (scale == -bigInt->scale)
			{
			int64 value = bigInt->getInt();
			putBinaryDecimal(value, ptr, precision, scale);
			
			return;
			}
		}

	char *p = ptr;
	int digits = precision - scale;
	int groups = digits / DIGITS_PER_INT;
	int partialDigits = digits % DIGITS_PER_INT;
	BigInt number(bigInt);
	uint32 fraction;
	BigInt bigFraction;
	uint32 data[20];

	// Scale to isolate the integer but save the fractional digits
	
	if (scale <= 9)
		number.scaleTo(0, &fraction);
	else
		number.scaleTo(0, &bigFraction);

	int n;
	
	// Store each 9-digit group, descale by 10^9 for each
	
	for (n = 0; n < groups; ++n)
		data[n] = (number.length) ? number.divide(BASE) : 0;

	// First put the group of partial (< 9) digits
			
	if (partialDigits)
		putBinaryGroup((number.length) ? number.divide(BASE) : 0, partialDigits, &p);
	
	// Now put each 9-digit group
	
	while (--n >= 0)
		{
		putBinaryGroup(data[n], DIGITS_PER_INT, &p);
		data[n] = 0;
		}
	
	// Now handle fraction

	if (scale <= 9)
		putBinaryNumber(fraction, scale, &p, true);
	else
		{
		groups = scale / DIGITS_PER_INT;
		partialDigits = scale % DIGITS_PER_INT;
		
		// Isolate the trailing partial digits (< 9)
		
		uint32 partial = (uint32)bigFraction.divide((uint32)powersOfTen[partialDigits]);
		
		// Store each 9-digit group, descale by 10^9 for each
		
		for (n = 0; n < groups; ++n)
			data[n] = (bigFraction.length) ? bigFraction.divide(BASE) : 0;

		// Write the 9-digit groups LSB first
		
		while (--n >= 0)
			putBinaryGroup(data[n], DIGITS_PER_INT, &p);

		// Write the trailing digits
		
		if (partialDigits)
			putBinaryGroup(partial, partialDigits, &p);
		}
	
	if (bigInt->neg)
		for (char *q = ptr; q < p; ++q)
			*q ^= -1;
	
	*ptr ^= 0x80;
}

int ScaledBinary::numberBytes(int precision, int fractions)
{
	return numberBytes(precision - fractions) + numberBytes(fractions);
}

int64 ScaledBinary::scaleInt64(int64 number, int delta)
{
	int64 value = number;
	int n = delta;
	
	if (n > 0)
		{
		for (; n > 9; n -= 9)
			value /= powersOfTen[9];
		
		if (n)
			value /= powersOfTen[n];
		}
	else if (n < 0)
		{
		for (; n < -9; n += 9)
			value *= powersOfTen[9];
		
		if (n)
			value *= powersOfTen[-n];
		}
	
	return value;
}
