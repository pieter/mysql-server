/* Copyright (C) 2002 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
				                                                                                      
				 
/* Header file for my_aes.c */
/* Wrapper to give simple interface for MySQL to AES standard encryption */

#ifndef __MY_AES_H
#define __MY_AES_H

#include "my_global.h"
#include <stdio.h>
#include "rijndael.h"

#define AES_KEY_LENGTH 128
/* Must be 128 192 or 256 */

#ifdef __cplusplus
extern "C" {
#endif

/*
my_aes_encrypt  - Crypt buffer with AES encryption algorithm.
source        - Pinter to data for encryption
source_length - size of encruption data
dest          - buffer to place encrypted data (must be large enough)
key           - Key to be used for encryption
kel_length    - Lenght of the key. Will handle keys of any length

returns  - size of encrypted data, or negative in case of error.

*/

int my_aes_encrypt(const char* source, int source_length, const char* dest,
                   const char* key, int key_length);

/*
my_aes_decrypt  - DeCrypt buffer with AES encryption algorithm.
source        - Pinter to data for decryption
source_length - size of encrypted data
dest          - buffer to place decrypted data (must be large enough)
key           - Key to be used for decryption
kel_length    - Lenght of the key. Will handle keys of any length

returns  - size of original data, or negative in case of error.

*/


int my_aes_decrypt(const char* source, int source_length, const char* dest,
                   const char* key, int key_length);


/*
my_aes_get_size - get size of buffer which will be large enough for encrypted
                  data
source_length -  length of data to be encrypted

returns  - size of buffer required to store encrypted data

*/

int my_aes_get_size(int source_length);


#ifdef __cplusplus
 }
#endif


#endif 
