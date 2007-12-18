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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

#include "mysql_priv.h"
#include "ha_falcon.h"

typedef longlong	int64;

#include "ScaledBinary.h"
#include "BigInt.h"
#include "TimeTest.h"
#include "Value.h"

C_MODE_START
#include <decimal.h>
int decimal_shift(decimal_t *dec, int shift);
C_MODE_END


typedef unsigned char UCHAR;

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

TimeTest::TimeTest(void)
{
}

TimeTest::~TimeTest(void)
{
}

void TimeTest::start(void)
{
	startTime = clock();
}

void TimeTest::finish(const char *msg, ...)
{
	double delta = (double) (clock() - startTime) / CLOCKS_PER_SEC;

	va_list		args;
	va_start	(args, msg);
	char		temp [1024];

	vsnprintf (temp, sizeof (temp) - 1, msg, args);
	printf ("%s: %d iterations in %g seconds\n", temp, iterations, delta);
}

void TimeTest::testScaled(int precision, int fractions, int count)
{
	iterations = count;
	char data[60 * 20];
	char temp[20];
	int n;
	int iteration;
	int64 value;
	int numberBytes = ScaledBinary::numberBytes(precision, fractions);
	union {
		int	i;
		char c[4];
		} fred;
	
	fred.i = 50;
	
	for (n = 0, value = 1; n < precision; ++n, value *= 10)
		ScaledBinary::putBinaryDecimal(value, data + n * 20, precision, fractions);
	
	start();
	
	for (iteration = 0; iteration < iterations; ++iteration)
		for (n = 0; n < precision; ++n)
			{
			char *p = data + n * 20;
			Value value;
			int64 number = ScaledBinary::getInt64FromBinaryDecimal(p, precision, fractions);
			value.setValue((int32) number, fractions);
			Value addend;
			addend.setValue((int32) n, fractions);
			
			for (int z = 0; z < 10; ++z)
				value.add(&addend);
			
			Value multiplier;
			multiplier.setValue((int32)15, 1);
			value.multiply(&value);
				
			number = value.getQuad(fractions);
			ScaledBinary::putBinaryDecimal(number, temp, precision, fractions);
			}
	
	finish("Falcon %d.%d", precision, fractions);
	
	start();
	
	for (iteration = 0; iteration < iterations; ++iteration)
		for (n = 0; n < precision; ++n)
			{
			char *p = data + n * 20;
			my_decimal decimal_value;
			binary2my_decimal(E_DEC_FATAL_ERROR, p, &decimal_value, precision, fractions);
			my_decimal addend;
			longlong2decimal(n, &addend);
			decimal_shift(&addend, -fractions);
			
			for (int z = 0; z < 10; ++z)
				{
				my_decimal sum;
				decimal_add(&decimal_value, &addend, &sum);
				decimal_value = sum;
				}
			
			my_decimal multiplier;
			longlong2decimal(15, &multiplier);
			decimal_shift(&multiplier, -1);
			my_decimal product;
			decimal_mul(&decimal_value, &multiplier, &product);
			decimal_value = product;
				
			my_decimal2binary(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                                         &decimal_value, temp, precision, fractions);
			}
	
	finish("server %d.%d", precision, fractions);
}
