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

// KeyGen.cpp: implementation of the KeyGen class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "KeyGen.h"
#include "tomcrypt.h"
#include "TransformException.h"
#include "DateTime.h"
#include "StringTransform.h"
#include "HexTransform.h"
#include "DecodeTransform.h"
#include "EncodeTransform.h"
#include "StreamTransform.h"
#include "BERDecode.h"
#include "BEREncode.h"
#include "BERItem.h"

static prng_state	prngState;

static int rsaSignature[] = { 1, 2, 840, 113549, 1, 1, 1 };
static UCHAR asn1Zero [] = { 0 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

KeyGen::KeyGen()
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
}

KeyGen::~KeyGen()
{
}

void KeyGen::generate(int keyLength, const char *seed)
{
	rsa_key	 rsaKey;
	check(yarrow_add_entropy((const UCHAR*) seed, strlen(seed), &prngState));
	//check(rsa_make_key(&prngState, prngIndex,  keyLength / 8, 65537, &rsaKey));
	check(rsa_make_key(&prngState, prngIndex,  keyLength / 8, 17, &rsaKey));
	publicKey = makeKey(PK_PUBLIC, &rsaKey);
	privateKey = makeKey(PK_PRIVATE, &rsaKey);
	rsa_free(&rsaKey);
}


void KeyGen::check(int ret)
{
	if (ret != CRYPT_OK)
		throw TransformException(error_to_string(ret));
}

JString KeyGen::makeKey(int type, Rsa_key *rsaKey)
{
	UCHAR numbers[2048];
	unsigned long outlen = sizeof(numbers);
	check(rsa_export(numbers, &outlen, type, rsaKey));
	BERItem keySequence(0, SEQUENCE, outlen, numbers);
	UCHAR key[1024];
	BEREncode encodeKey(sizeof(key) - 1, key + 1);
	keySequence.encode(&encodeKey);

	UCHAR signature [512];
	int signatureLength = BEREncode::encodeObjectNumbers(
				sizeof(rsaSignature)/sizeof(rsaSignature[0]),
				rsaSignature, sizeof(signature), signature);
	BERItem ans1Key(0, SEQUENCE, 0, NULL);
	BERItem *algorithm = new BERItem (0, SEQUENCE, 0, NULL);
	algorithm->addChild(new BERItem(0, OBJECT_TAG, signatureLength, signature));
	algorithm->addChild(new BERItem(0, NULL_TAG, 0, NULL));

	if (type == PK_PRIVATE)
		{
		ans1Key.addChild(new BERItem (0, INTEGER, sizeof (asn1Zero), asn1Zero));
		ans1Key.addChild(algorithm);
		ans1Key.addChild(new BERItem(0, OCTET_STRING, encodeKey.getLength(), key + 1));
		}
	else
		{
		ans1Key.addChild(algorithm);
		key[0] = 0;
		ans1Key.addChild(new BERItem(0, BIT_STRING, encodeKey.getLength() + 1, key));
		}


	BEREncode encoder;
	ans1Key.encode(&encoder);
	JString string = encoder.getJString();

	/***
	StreamTransform stream(&encoder.stream);
	HexTransform hex(false, &stream);
	BERDecode decode(&hex);
	***/
	//decode.print();

	return string;
}

JString KeyGen::extractPublicKey(const char *hex)
{
	DecodeTransform<StringTransform,HexTransform> decode (hex);
	UCHAR orgBERKey [1024];
	int len = decode.get(sizeof(orgBERKey), orgBERKey);
	BERDecode decoder(len, orgBERKey);
	decoder.print();
	decoder.next(SEQUENCE);
	decoder.pushDown();
	decoder.next(INTEGER);
	decoder.next(SEQUENCE);
	decoder.next(OCTET_STRING);
	decoder.pushDown();
	decoder.next(SEQUENCE);
	decoder.pushDown();
	decoder.next(INTEGER);	// version number
	const UCHAR *numbersStart = decoder.ptr;
	decoder.next(INTEGER);	// modulus
	const UCHAR *modulus = decoder.content;
	int modulusLength = decoder.contentLength;
	decoder.next(INTEGER);	// public exponent
	decoder.next(INTEGER);	// private exponent
	const UCHAR *exponent = decoder.content;
	int exponentLength = decoder.contentLength;

	BERItem pubKey(0, SEQUENCE, 0, NULL);
	pubKey.addChild(new BERItem(0, INTEGER, modulusLength, modulus));
	pubKey.addChild(new BERItem(0, INTEGER, exponentLength, exponent));
	UCHAR rawKey[1024];
	BEREncode rawKeyEncoder(sizeof(rawKey) - 1, rawKey + 1);
	pubKey.encode(&rawKeyEncoder);
	int rawLength = rawKeyEncoder.getLength();

	UCHAR signature [512];
	int signatureLength = BEREncode::encodeObjectNumbers(
				sizeof(rsaSignature)/sizeof(rsaSignature[0]),
				rsaSignature, sizeof(signature), signature);
	BERItem ans1Key(0, SEQUENCE, 0, NULL);
	BERItem *algorithm = new BERItem (0, SEQUENCE, 0, NULL);
	algorithm->addChild(new BERItem(0, OBJECT_TAG, signatureLength, signature));
	algorithm->addChild(new BERItem(0, NULL_TAG, 0, NULL));

	ans1Key.addChild(algorithm);
	rawKey[0] = 0;
	ans1Key.addChild(new BERItem(0, BIT_STRING, rawLength + 1, rawKey));
	ans1Key.prettyPrint(0);

	BEREncode encoder;
	ans1Key.encode(&encoder);
	JString string = encoder.getJString();

	//printf ("%s\n", (const char*) string);

	return string;
}
