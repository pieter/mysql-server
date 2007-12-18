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

#ifndef __INTERLOCK_H
#define __INTERLOCK_H

#define INTERLOCKED_INCREMENT(variable)		interlockedIncrement(&variable)
#define INTERLOCKED_DECREMENT(variable)		interlockedDecrement(&variable)
#define INTERLOCKED_EXCHANGE(ptr, value)	interlockedExchange(ptr, value)
#define INTERLOCKED_ADD(ptr, value)			interlockedAdd(ptr, value)

#ifdef _WIN32

#define COMPARE_EXCHANGE(target,compare,exchange)\
	(InterlockedCompareExchange(target,exchange,compare)==compare)

#ifdef _WIN64
#include <intrin.h>
#define COMPARE_EXCHANGE_POINTER(target,compare,exchange)\
	(InterlockedCompareExchangePointer((void *volatile*) target,(void*)exchange,(void*)compare)==(void*)compare)
#define InterlockedCompareExchangePointer	_InterlockedCompareExchangePointer
void* _InterlockedCompareExchangePointer(void *volatile *Destination, void *Exchange, void *Comperand);
#else
#define COMPARE_EXCHANGE_POINTER(target,compare,exchange)\
	(InterlockedCompareExchange((volatile int*) target,(int)exchange,(int)compare)==(int)compare)
#endif

#define InterlockedIncrement				_InterlockedIncrement
#define InterlockedDecrement				_InterlockedDecrement
#define InterlockedExchange					_InterlockedExchange
#define InterlockedExchangeAdd				_InterlockedExchangeAdd

#define InterlockedCompareExchange			_InterlockedCompareExchange

#ifndef InterlockedCompareExchangePointer
//#define InterlockedCompareExchangePointer	_InterlockedCompareExchangePointer
#endif

#ifndef __MACHINEX64
extern "C" 
	{
	long  InterlockedIncrement(long* lpAddend);
	long  InterlockedDecrement(long* lpAddend);
	long  InterlockedExchange(long* volatile addend, long value);
	long  InterlockedExchangeAdd(long* volatile addend, long value);
	long  InterlockedCompareExchange(volatile int *Destination, int Exchange, int Comperand);
	//void* InterlockedCompareExchangePointer(void *volatile* *Destination, void *Exchange, void *Comperand);
	}
#endif

#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)
//#pragma intrinsic(_InterlockedCompareExchangePointer)

#endif

#if defined (__i386) || (__x86_64__) || defined (__sparc__)

#define COMPARE_EXCHANGE(target,compare,exchange)\
	(inline_cas(target,compare,exchange))
#define COMPARE_EXCHANGE_POINTER(target,compare,exchange)\
	(inline_cas_pointer((volatile void**)target,(void*)compare,(void*)exchange))

#endif


inline INTERLOCK_TYPE interlockedIncrement(volatile INTERLOCK_TYPE *ptr)
{
#ifndef _WIN32
	INTERLOCK_TYPE ret = 1;
	asm (
		"lock\n\t" "xaddl %0,%1\n\t" 
		: "+r" (ret)
		: "m" (*ptr)
        : "memory"
		);
	return ret + 1;
#else
	return InterlockedIncrement ((long*) ptr);
#endif
}

inline INTERLOCK_TYPE interlockedDecrement(volatile INTERLOCK_TYPE *ptr)
{
#ifndef _WIN32
	INTERLOCK_TYPE ret = -1;
	asm (
		"lock\n\t" "xaddl %0,%1\n\t" 
		: "+r" (ret)
		: "m" (*ptr)
        : "memory"
		);
	return ret - 1;
#else
	return InterlockedDecrement ((long*) ptr);
#endif
}

inline INTERLOCK_TYPE interlockedAdd(volatile long* addend, long value)
{
#ifndef _WIN32
	long ret = value;
	asm (
		"lock\n\t" "xadd %0,%1\n\t" 
		: "=r" (ret)
		: "m" (*(addend)), "0" (ret)
        : "memory"
		);
	return ret + value;
#else
	return InterlockedExchangeAdd((long*) addend, value);
#endif
}

/***
00134     LONG result;
00135 
00136     __asm__ __volatile__(
00137              "lock; xchgl %0,(%1)"
00138              : "=r" (result)
00139              : "r" (Target), "0" (Value)
00140              : "memory" 
00141              );
00142     return result;
00143 }
***/

inline INTERLOCK_TYPE interlockedExchange(volatile long* addend, long value)
{
#ifndef _WIN32
	long ret = value;
	asm  volatile (
		"lock\n\t" "xchg %0,%1\n\t" 
		: "=r" (ret)
		: "m" (*(addend)), "0" (ret)
		: "memory"
		);
	return ret;
#else
	return InterlockedExchange((long*) addend, value);
#endif
}


inline int inline_cas (volatile int *target, int compare, int exchange)
{
#if defined(__i386) || (__x86_64__)
	char ret;

	__asm __volatile ("lock; cmpxchg %2, %1 ; sete %0"
		    : "=q" (ret), "+m" (*(target))
		    : "r" (exchange), "a" (compare)
			: "cc", "memory"); 

	return ret;

#elif defined(__sparc__)
	__asm__ __volatile__(
		       "cas     [%2], %3, %0\n\t"
		    : "=&r" (exchange)
		    : "0" (exchange), "r" (target), "r" (compare)
		    : "memory");
#else
	return -2;
#endif
}

inline char inline_cas_pointer (volatile void **target, void *compare, void *exchange)
{
#if defined(__i386) || defined(__x86_64__)
	char ret;

	__asm __volatile ("lock; cmpxchg %2, %1 ; sete %0"
		    : "=q" (ret), "+m" (*(target))
		    : "r" (exchange), "a" (compare)
			: "cc", "memory"); 

	return ret;

#elif defined(__sparc__)
	__asm__ __volatile__(
		       "casx    [%2], %3, %0\n\t"
		    : "=&r" (exchange)
		    : "0" (exchange), "r" (target), "r" (compare)
		    : "memory");
#else
	return NULL;
#endif
}

#endif
