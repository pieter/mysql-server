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

// BigInteger.h: interface for the BigInteger class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BIGINTEGER_H__5DAC7F41_3038_11D5_9919_0000C01D2301__INCLUDED_)
#define AFX_BIGINTEGER_H__5DAC7F41_3038_11D5_9919_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum BigOp {
	bigAdd,
	bigSubtract,
	bigMultiply,
	bigDivide,
	bigRemainder,
	bigSquare,
	bigGcd,
	bigModPow,
	bigModInversion,
	bigGeneratePrime,
	};

class BigInteger  
{
public:
	static void modInversion(BigInteger a, BigInteger b, BigInteger result);
	static void generatePrime(BigInteger a, BigInteger result);
	static void gcd(BigInteger a, BigInteger b, BigInteger result);
	static void remainder(BigInteger a, BigInteger b, BigInteger result);
	static void divide(BigInteger a, BigInteger b, BigInteger result);
	static void multiply(BigInteger a, BigInteger b, BigInteger result);
	static void subtract(BigInteger a, BigInteger b, BigInteger result);
	static void add (BigInteger a, BigInteger b, BigInteger result);
	static int getResultLength (BigOp op, BigInteger a, BigInteger b);

    int		length;
	char	*bytes;
};

#endif // !defined(AFX_BIGINTEGER_H__5DAC7F41_3038_11D5_9919_0000C01D2301__INCLUDED_)
