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

#ifndef _atomic_h_cleanup_
#define _atomic_h_cleanup_ "atomic/generic-msvc.h"

/*
  We don't implement anything specific for MY_ATOMIC_MODE_DUMMY, always use
  intrinsics.
*/
#define MY_ATOMIC_MODE "msvc-intrinsics"
#define IL_EXCHG_ADD32   InterlockedExchangeAdd
#define IL_COMP_EXCHG32  InterlockedCompareExchange
#define IL_COMP_EXCHGptr InterlockedCompareExchangePointer
#define IL_EXCHG32       InterlockedExchange
#define IL_EXCHGptr      InterlockedExchangePointer
#define make_atomic_add_body(S) \
  v= IL_EXCHG_ADD ## S (a, v)
#define make_atomic_cas_body(S)                                 \
  int ## S initial_cmp= *cmp;                                   \
  int ## S initial_a= IL_COMP_EXCHG ## S (a, set, initial_cmp); \
  if (!(ret= (initial_a == initial_cmp))) *cmp= initial_a;
#define make_atomic_swap_body(S) \
  v= IL_EXCHG ## S (a, v)
#define make_atomic_load_body(S)       \
  ret= 0; /* avoid compiler warning */ \
  ret= IL_COMP_EXCHG ## S (a, ret, ret);

#else /* cleanup */

#undef IL_EXCHG_ADD32
#undef IL_COMP_EXCHG32
#undef IL_COMP_EXCHGptr
#undef IL_EXCHG32
#undef IL_EXCHGptr

#endif
