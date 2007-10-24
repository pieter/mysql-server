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

// BigInt.h: interface for the BigInt class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BIGINT_H__E2229172_017A_4430_B5AE_FF8B6C6D4CAA__INCLUDED_)
#define AFX_BIGINT_H__E2229172_017A_4430_B5AE_FF8B6C6D4CAA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

static const int maxBigWords	= 10;
static const int bigWordBits	= 32;
static const int maxPowerOfTen  = 65;

#define LOW_WORD(n)			((BigWord) n)
#define HIGH_WORD(n)		((BigWord)(n >> bigWordBits))
#define FETCH_WORD(index)	((index < length) ? words[index] : 0)
#define GET_INT64(high,low)	(((int64) (high) << bigWordBits) + (low))

typedef uint32	BigWord;
typedef uint64	BigDWord;

class BigInt  
{
public:
	BigInt (BigInt *bigInt);
	BigInt();
	virtual ~BigInt();
	
	virtual void	multiply(BigWord value);
	virtual BigWord	divide (BigWord value);
	virtual BigInt	divide(const BigInt* divisor);
	virtual int		divide(const BigInt * dividend, const BigInt * divisor, BigInt * quotient, BigInt * remainder);
	virtual void	scaleTo(int toScale);
	virtual BigWord	scaleTo(int toScale, BigWord* result);
	virtual BigInt	scaleTo(int toScale, BigInt* result);
	virtual void	set(BigWord value32);
	virtual int		getByteLength(void);
	virtual void	getBytes(char* bytes);
	virtual void	setBytes(int scale, int byteLength, const char* bytes);
	virtual int		fitsInInt64(void);
	
	void			add (int64 value);
	void			print (const char *msg = "");
	int64			getInt();
	void			subtract(int index, BigWord value);
	
	void			set(int64 value, int scale=0);
	void			set(BigInt *bigInt);
	void			setString(int stringLength, const char* str);
	void			getString(int bufferLength, char* buffer);
	double			getDouble(void);
	int				compare(BigInt* bigInt);
	int				nlz(uint64 x);
	void			buildPowerTable();
	

	short		length;
	short		scale;
	int			neg;
	BigWord		words[maxBigWords];

	inline void clear()
		{
		length = 0;
		scale = 0;
		neg = false;
		for (int i = 0; i < maxBigWords; i++)
			words[i] = 0;
		}
		
	inline void add(int index, BigWord value)
		{
		while (index > length)
			words[length++] = 0;

		while (value)
			{
			BigDWord acc = (BigDWord) FETCH_WORD(index) + value;
			words[index++] = LOW_WORD(acc);
			value = HIGH_WORD(acc);
			}

		if (index > length)
			length = index;
		}

	inline void normalize()
		{
		while (length && words[length - 1] == 0)
			--length;
		}
	
	inline int compareRaw(BigInt *bigInt)
		{
		if (length > bigInt->length)
			return 1;
		
		if (length < bigInt->length)
			return -1;
		
		for (int n = length - 1; n >= 0; --n)
			if (words[n] != bigInt->words[n])
				return (words[n] > bigInt->words[n]) ? 1 : -1;
		
		return 0;
		}
};

#endif // !defined(AFX_BIGINT_H__E2229172_017A_4430_B5AE_FF8B6C6D4CAA__INCLUDED_)
