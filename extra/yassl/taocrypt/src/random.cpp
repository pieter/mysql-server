/* random.cpp                                
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


/* random.cpp implements a crypto secure Random Number Generator using an OS
   specific seed, switch to /dev/random for more security but may block
*/

#include "runtime.hpp"
#include "random.hpp"
#include <string.h>


#if defined(_WIN32)
    #define _WIN32_WINNT 0x0400
    #include <windows.h>
    #include <wincrypt.h>
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif // _WIN32

namespace TaoCrypt {


// Get seed and key cipher
RandomNumberGenerator::RandomNumberGenerator()
{
    byte key[32];
    seed_.GenerateSeed(key, sizeof(key));
    cipher_.SetKey(key, sizeof(key));
}


// place a generated block in output
void RandomNumberGenerator::GenerateBlock(byte* output, word32 sz)
{
    memset(output, 0, sz);
    cipher_.Process(output, output, sz);
}


byte RandomNumberGenerator::GenerateByte()
{
    byte b;
    GenerateBlock(&b, 1);

    return b;
}


#if defined(_WIN32)

OS_Seed::OS_Seed()
{
    if(!CryptAcquireContext(&handle_, 0, 0, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT))
        error_.SetError(WINCRYPT_E);
}


OS_Seed::~OS_Seed()
{
    CryptReleaseContext(handle_, 0);
}


void OS_Seed::GenerateSeed(byte* output, word32 sz)
{
    if (!CryptGenRandom(handle_, sz, output))
        error_.SetError(CRYPTGEN_E);
}


#else // _WIN32


OS_Seed::OS_Seed() 
{
    fd_ = open("/dev/urandom",O_RDONLY);
    if (fd_ == -1)
        error_.SetError(OPEN_RAN_E);
}


OS_Seed::~OS_Seed() 
{
    close(fd_);
}


// may block
void OS_Seed::GenerateSeed(byte* output, word32 sz)
{
    while (sz) {
        int len = read(fd_, output, sz);
        if (len == -1) {
            error_.SetError(READ_RAN_E);
            return;
        }

        sz     -= len;
        output += len;

        if (sz)
            sleep(1);
    }
}

#endif // _WIN32



} // namespace
