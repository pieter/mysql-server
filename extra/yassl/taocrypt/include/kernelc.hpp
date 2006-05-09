/* kernelc.hpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* kernelc.hpp provides support for C std lib when compiled in kernel mode
*/

#ifndef TAOCRYPT_KERNELC_HPP
#define TAOCRYPT_KERNELC_HPP

#include <linux/types.h>   // get right size_t

// system functions that c++ doesn't like headers for 

extern "C" void* memcpy(void*, const void*, size_t);
extern "C" void* memset(void*, int, size_t);
extern "C" void  printk(char *fmt, ...);

#define KERN_ERR "<3>"   /* error conditions */

#if defined(NDEBUG)
    #define assert(p)  	((void)0)
#else
    #define assert(expr)   \
    if (!(expr))         { \
         printk(KERN_ERR "Assertion failed! %s,%s,%s,line=%d\n", \
         #expr,__FILE__,__FUNCTION__,__LINE__); }
#endif



#endif // TAOCRYPT_KERNELC_HPP
