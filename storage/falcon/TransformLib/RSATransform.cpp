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

// RSATransform.cpp: implementation of the RSATransform class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "RSATransform.h"
#include "TransformException.h"
#include "BERDecode.h"
#include "DateTime.h"

static prng_state	prngState;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RSATransform::RSATransform(int operatingMode, Transform *src)
{
	prngIndex = find_prng("yarrow");

	if (prngIndex < 0)
		{
		int ret = yarrow_start(&prngState);
		int32 date = DateTime::getNow();
		check(yarrow_add_entropy((const UCHAR*) &date, sizeof(date), &prngState));
		check(yarrow_ready(&prngState));
		register_prng(&yarrow_desc);
		prngIndex = find_prng("yarrow");
		}

	hashIndex = find_hash("sha1");

	if (hashIndex < 0)
		{
		register_hash(&sha1_desc);
		hashIndex = find_hash("sha1");
		}

	hashLength = hash_descriptor[hashIndex].hashsize;
	mode = operatingMode;
	source = src;
	reset();
}

RSATransform::~RSATransform()
{
	rsa_free(&rsaKey);
}

int RSATransform::getSize()
{
	int modulus_bitlen = mp_count_bits(&(rsaKey.N));

	return modulus_bitlen / 8;
}

int RSATransform::getBlockSize()
{
	return getSize() - 2 * hashLength - 2;
}

unsigned int RSATransform::getLength()
{
	int len = source->getLength();
	int blockSize = getBlockSize();
	int size = getSize();

	switch (mode)
		{
		case RSA_publicEncrypt:
		case RSA_privateEncrypt:
			return (len + blockSize - 1) / blockSize * size;

		default:
			//return (len + blockSize - 1) / blockSize * blockSize;
			return len / size * blockSize;
		}
}

unsigned int RSATransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	UCHAR *p = buffer;
	UCHAR *endBuffer = buffer + bufferLength;
	UCHAR input[1024];
	UCHAR temp[1024];
	unsigned int size = getSize();
	bool encrypt = mode == RSA_publicEncrypt || mode == RSA_privateEncrypt;
	unsigned int blockSize = (encrypt) ? getBlockSize() : size;

	if (size > sizeof(input))
		throw TransformException("RSA keysize too big for buffer");

	// If there's anything left around from a previous block, copy it now

	if (ptr < end)
		{
		unsigned int len = MIN(bufferLength, (unsigned int) (end - ptr));
		memcpy (p, ptr, len);
		ptr += len;
		p += len;
		if (p == end)
			return p - buffer;
		}


	while (p < endBuffer)
		{
		unsigned int len = source->get(blockSize, input);
		if (len == 0)
			break;

		/***
		if (len < size)
			memset(input + len, 0, size - len);
		***/

		int res;
		unsigned long outLen = size;

		switch(mode)
			{
			case RSA_publicEncrypt:
				prngState.yarrow.ctr.padlen = 0;
				check(pkcs_1_oaep_encode(input, len, NULL, 0,
										 size*8, &prngState, prngIndex, hashIndex,
										 temp, &outLen));
				check(rsa_exptmod(temp, outLen, output, &outLen, PK_PUBLIC, &rsaKey));
				break;

			case RSA_privateDecrypt:
				if (len < blockSize)
					throw TransformException("partial RSA block");
				check(rsa_exptmod(input, len, temp, &outLen, PK_PRIVATE, &rsaKey));
				check(pkcs_1_oaep_decode(temp, outLen, NULL, 0,
										 size*8, hashIndex, output, &outLen, &res));
				break;

			case RSA_publicDecrypt:
				if (len < blockSize)
					throw TransformException("partial RSA block");
				check(rsa_exptmod(input, len, temp, &outLen, PK_PUBLIC, &rsaKey));
				check(pkcs_1_oaep_decode(temp, outLen, NULL, 0,
										 size*8, hashIndex, output, &outLen, &res));
				break;

			case RSA_privateEncrypt:
				check(pkcs_1_oaep_encode(input, len, NULL, 0,
										 size*8, &prngState, prngIndex, hashIndex,
										 temp, &outLen));
				check(rsa_exptmod(temp, outLen, output, &outLen, PK_PRIVATE, &rsaKey));
				break;

			}

		len = MIN((unsigned int)(endBuffer - p), outLen);
		memcpy(p, output, len);
		p += len;

		if (len < outLen)
			{
			ptr = output + outLen;
			break;
			}
		}

	return p - buffer;
}

/* 
 * Set a public key.  This is complicated by a nasty bit of backward compatibility.
 */
void RSATransform::setPublicKey(Transform *transform)
{
	UCHAR key [1024];
	int keyLength = transform->get(sizeof(key), key);
	BERDecode decode (keyLength, key);
	//decode.print();
	decode.next(SEQUENCE);
	const UCHAR *keyStart = decode.content;
	keyLength = decode.contentLength;
	decode.pushDown();
	
	if (!decode.next())
		throwBadKey();

	// If we have an sequence, this is a X.509 public key.  Snarf it.

	if (decode.tagNumber == SEQUENCE)
		{
		decode.next(BIT_STRING);
		decode.pushDown();
		decode.next(SEQUENCE);
		keyStart = decode.content;
		keyLength = decode.contentLength;
		decode.pushDown();
		decode.next(INTEGER);
		}

	if (decode.tagNumber != INTEGER)
		throwBadKey();

	// If the next integer looks like a modulus, it's a public key

	if (decode.contentLength > 4)
		{
		check(rsa_import(keyStart, keyLength, &rsaKey));
		return;
		}

	const UCHAR *modulus = decode.ptr;

	if (!decode.next())				// modulus or algorithm
		throw TransformException("invalid RSA key");

	// If this is a sequence, it's an algorithm identifier for X.509

	if (decode.tagNumber == SEQUENCE)
		{
		decode.next(OCTET_STRING);
		decode.pushDown();
		decode.next(SEQUENCE);
		decode.pushDown();
		decode.next(INTEGER);		// version
		modulus = decode.ptr;
		decode.next(INTEGER);		// modulus
		}

	// We have an RSA private key.  Turn it into a public key.

	UCHAR privateKey [1024];
	int modulusLength = decode.ptr - modulus;
	memcpy(privateKey, modulus, modulusLength);
	decode.next(INTEGER);		// public exponent
	const UCHAR *exponent = decode.ptr;
	decode.next(INTEGER);		// private exponent
	int exponentLength = decode.ptr - exponent;
	memcpy(privateKey + modulusLength, exponent, exponentLength);

	check(rsa_import(privateKey, modulusLength + exponentLength, &rsaKey));
}

void RSATransform::setPrivateKey(Transform *transform)
{
	BERDecode key(transform);
	//key.print();
	int length = key.getRawLength();

	if (length > 620)
		{
		key.next(SEQUENCE);
		key.pushDown();
		key.next(INTEGER);
		key.next(SEQUENCE);
		key.next(OCTET_STRING);
		key.pushDown();
		}

	key.next(SEQUENCE);
	key.pushDown();

	int ret = rsa_import(key.content, key.contentLength, &rsaKey);
}

void RSATransform::check(int ret)
{
	if (ret != CRYPT_OK)
		throw TransformException(error_to_string(ret));
}

void RSATransform::reset()
{
	ptr = end = output;
	source->reset();
}

/*
 * Figure out what kind of key it is and do the right thing.  Possibilies:
 *
	RSA PKCS #1 Private Key

		SEQUENCE {
			version			Version,	-- 0 (two prime) or 1 (multi-prime)
			modulus			INTEGER,	-- n
			publicExponent	INTEGER,	-- e
			privateExponent	INTEGER,	-- d
			prime1			INTEGER,	-- p
			prime2			INTEGER,	-- q
			exponent1		INTEGER,	-- d (mod (p - 1)
			exponent2		INTEGER,	-- d (mod (q - 1)
			cofficient		INTEGER,	-- (inverse of q) mod p
			otherPrimes		OtherPrimInfos OPTIONAL
		}

	RSA PKCS #8 Private Key

		SEQUENCE {
			version				INTEGER,	-- 0
			privateKeyAlgorithm Algorithm,
			privateKey			OCTET STRING,
			attributes [0]		IMPLICIT Attributes OPTIONAL 
		}
	
	RSA PKCS #1 Public Key
			
		SEQUENCE {
			modulus			INTEGER,	-- n
			publicExponent	INTEGER,	-- e
		}

	X.509's SubjectPublicKeyInfo

		SEQUENCE {
			algorithm		Algorithm,
			publicKey		BIT STRING
		}

	where "Algorithm" is

		SEQUENCE {
			algorithm		OBJECT,		-- (1,2,840,113549,1,1,1)
			parameters		ANY OPTIONAL
		}
 */

void RSATransform::setKey(Transform *transform)
{
	UCHAR key [1024];
	int keyLength = transform->get(sizeof(key), key);
	BERDecode decode (keyLength, key);
	//decode.print();
	decode.next(SEQUENCE);
	const UCHAR *keyStart = decode.content;
	keyLength = decode.contentLength;
	decode.pushDown();
	
	if (!decode.next())
		throwBadKey();

	// If we have an sequence, this is a X.509 public key.  Snarf it.

	if (decode.tagNumber == SEQUENCE)
		{
		decode.next(BIT_STRING);
		decode.pushDown();
		decode.next(SEQUENCE);
		keyStart = decode.content;
		keyLength = decode.contentLength;
		decode.pushDown();
		decode.next(INTEGER);
		}

	if (decode.tagNumber != INTEGER)
		throwBadKey();

	// If the next integer looks like a modulus, it's a public key

	if (decode.contentLength > 4)
		{
		check(rsa_import(keyStart, keyLength, &rsaKey));
		return;
		}

	if (!decode.next())
		throw TransformException("invalid RSA key");

	// If this is a sequence, it's an algorithm identifier for X.509

	if (decode.tagNumber == SEQUENCE)
		{
		decode.next(OCTET_STRING);
		decode.pushDown();
		decode.next(SEQUENCE);
		keyStart = decode.content;
		keyLength = decode.contentLength;
		}

	check(rsa_import(keyStart, keyLength, &rsaKey));
}

void RSATransform::throwBadKey()
{
	throw TransformException("invalid RSA key");
}
