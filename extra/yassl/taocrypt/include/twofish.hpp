/* twofish.hpp                                
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

/* twofish.hpp defines Twofish
*/


#ifndef TAO_CRYPT_TWOFISH_HPP
#define TAO_CRYPT_TWOFISH_HPP

#include "misc.hpp"
#include "modes.hpp"
#include "algorithm.hpp"

namespace TaoCrypt {

enum { TWOFISH_BLOCK_SIZE = 16 };


// Twofish encryption and decryption, see 
class Twofish : public Mode_BASE {
public:
    enum { BLOCK_SIZE = TWOFISH_BLOCK_SIZE };

    Twofish(CipherDir DIR, Mode MODE)
        : Mode_BASE(BLOCK_SIZE), dir_(DIR), mode_(MODE) {}

    void Process(byte*, const byte*, word32);
    void SetKey(const byte* key, word32 sz, CipherDir fake = ENCRYPTION);
    void SetIV(const byte* iv) { memcpy(r_, iv, BLOCK_SIZE); }
private:
    CipherDir dir_;
    Mode      mode_;

	static const byte     q_[2][256];
	static const word32 mds_[4][256];

	word32 k_[40];
	word32 s_[4][256];

	static word32 h0(word32 x, const word32 *key, unsigned int kLen);
	static word32 h(word32 x, const word32 *key, unsigned int kLen);

    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;

    void encrypt(const byte*, const byte*, byte*) const;
    void decrypt(const byte*, const byte*, byte*) const;

    void AsmEncrypt(const byte* inBlock, byte* outBlock) const;
    void AsmDecrypt(const byte* inBlock, byte* outBlock) const;

    Twofish(const Twofish&);            // hide copy
    Twofish& operator=(const Twofish&); // and assign
};


typedef BlockCipher<ENCRYPTION, Twofish, ECB> Twofish_ECB_Encryption;
typedef BlockCipher<DECRYPTION, Twofish, ECB> Twofish_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, Twofish, CBC> Twofish_CBC_Encryption;
typedef BlockCipher<DECRYPTION, Twofish, CBC> Twofish_CBC_Decryption;



} // naemspace

#endif // TAO_CRYPT_TWOFISH_HPP

