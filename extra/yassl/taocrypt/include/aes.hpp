/* aes.hpp                                
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

/* aes.hpp defines AES
*/


#ifndef TAO_CRYPT_AES_HPP
#define TAO_CRYPT_AES_HPP

#include <string.h>
#include "misc.hpp"
#include "modes.hpp"
#include "block.hpp"

namespace TaoCrypt {

enum { AES_BLOCK_SIZE = 16 };


// AES encryption and decryption, see FIPS-197
class AES : public Mode_BASE {
public:
    enum { BLOCK_SIZE = AES_BLOCK_SIZE };

    AES(CipherDir DIR, Mode MODE)
        : Mode_BASE(BLOCK_SIZE), dir_(DIR), mode_(MODE) {}

    void Process(byte*, const byte*, word32);
    void SetKey(const byte* iv, word32 sz, CipherDir fake = ENCRYPTION);

    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;
private:
    CipherDir dir_;
    Mode      mode_;

    static const word32 Te0[256];
    static const word32 Te1[256];
    static const word32 Te2[256];
    static const word32 Te3[256];
    static const word32 Te4[256];

    static const word32 Td0[256];
    static const word32 Td1[256];
    static const word32 Td2[256];
    static const word32 Td3[256];
    static const word32 Td4[256];

    static const word32 rcon_[];

    word32      rounds_;
    Word32Block key_;

    void encrypt(const byte*, const byte*, byte*) const;
    void decrypt(const byte*, const byte*, byte*) const;

    AES(const AES&);            // hide copy
    AES& operator=(const AES&); // and assign
};


typedef BlockCipher<ENCRYPTION, AES, ECB> AES_ECB_Encryption;
typedef BlockCipher<DECRYPTION, AES, ECB> AES_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, AES, CBC> AES_CBC_Encryption;
typedef BlockCipher<DECRYPTION, AES, CBC> AES_CBC_Decryption;



} // naemspace

#endif // TAO_CRYPT_AES_HPP
