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

// BigInteger.cpp: implementation of the BigInteger class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BigInteger.h"

#define REF(r)		(*(int64*)(r.bytes))

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int BigInteger::getResultLength(BigOp op, BigInteger a, BigInteger b)
{
	return 8;
}

void BigInteger::add(BigInteger a, BigInteger b, BigInteger result)
{
	REF(result) = REF(a) + REF(b);
}

void BigInteger::subtract(BigInteger a, BigInteger b, BigInteger result)
{
	REF(result) = REF(a) - REF(b);
}

void BigInteger::multiply(BigInteger a, BigInteger b, BigInteger result)
{
	REF(result) = REF(a) * REF(b);
}

void BigInteger::divide(BigInteger a, BigInteger b, BigInteger result)
{
	REF(result) = REF(a) / REF(b);
}

void BigInteger::remainder(BigInteger a, BigInteger b, BigInteger result)
{
	REF(result) = REF(a) % REF(b);
}

void BigInteger::gcd(BigInteger a, BigInteger b, BigInteger result)
{
    NOT_YET_IMPLEMENTED;
}

void BigInteger::generatePrime(BigInteger a, BigInteger result)
{
    NOT_YET_IMPLEMENTED;
}

void BigInteger::modInversion(BigInteger a, BigInteger b, BigInteger result)
{
    NOT_YET_IMPLEMENTED;
}
