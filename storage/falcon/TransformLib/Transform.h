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

// Transform.h: interface for the Transform class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TRANSFORM_H__C476BAAD_19F9_4BFD_ACF5_C9F9B82A8E93__INCLUDED_)
#define AFX_TRANSFORM_H__C476BAAD_19F9_4BFD_ACF5_C9F9B82A8E93__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Engine.h"

/*
	Transform classes are designed to daisy chain together to do interesting things.  
	A typical example is taking a SHA digest of a file.  This involves three transforms, 
	a FileTransform to fetch data from a named file, the SHATransform to perform the 
	hash, and a HexTransform to mutate the hash gook into Ascii.  The code might look like this:

		FileTransform file ("something.txt");
		SHATransform sha (0, &file);          // encryption Transforms except a mode; sha doesn't
		HexTransform hex (true, &sha);        // boolean says "encode"
		UCHAR hash[20];
		int len = hex.get(sizeof(hash), hash);

	To simplify things, I added some templates to automate the process:

		EncodeTransform<FileTransform,SHATransform,HexTranform> transform  ("something.txt");
		UCHAR hash[20];
		int len = transform .get(sizeof(hash), hash);

	EncodeTransform, the template, is itself a Transform.

	A slightly more interest example is a function to validate a site password.  The password 
	is hash with SHA encoded in base64:

		bool checkPassword (const char *password)
			{
			DecodeTransform<StringTransform,SHATransform> passwd (password);
			DecodeTransform<FileTransform,Base64Transform> file ("site_password");
			UCHAR hash1[20], hash2[25];
			int len1 = passwd.get(sizeof(hash1), hash1);
			int len2 = file.get(sizeof(hash2), hash2);

			return len1 == len2 && memcmp(hash1, hash2, len1) == 0;
			}

	Transform can also be used to fetch, decode, and prepare keys in the same manner as encryptions; 
	key are normally provided to encryption Transforms as a pointer to a transform from which it 
	sucks the key.  I also have a BER decoded that, surprise, takes a transform for input.

	I now have a good number of handy dandy transforms: StringTransform and FileTransform for data 
	sources, HexTransform and Base64Transform for converting between binary and Ascii, a SHATransform, 
	a DESKeyTransform to generate keys, a DESTransform (undevelopment w/ Tom's Crypto), an RSATransform 
	to SSLeay and another underway for Tom's, and a NullTransform for who knows what.  Finally, 
	there are the template transforms EncodeTransform, DecodeTransform, EncryptTransform, and 
	DecryptTransforms.

	My intention with the Transform Library is to produce an abstract layer to simplify and hide 
	the differences between crypto libraries.  None of the transform classes give a hoot how other 
	transforms work, just that they can make a given transform for an upper bound on the number 
	of bytes to be delivered and later, get the bytes in convenient sized hunks for any purpose.



	The semantics are simple.  The method "getLength" returns a reliable upper bound of the final 
	length.  The method "get" must return all available bytes up to the size of the given buffer, 
	returning zero when no bytes are remaining.  The method "reset" prepares for another data cycle, 
	resetting transient state but retaining keys and modes.

	Each transformation is independent -- none knows the implementation of any other.  Transforms 
	are designed to be daisy chained, each calling another transform for input data.  While all 
	transforms export the same base API, they can be loosely grouped into source transforms 
	(StringTransform, FileTransform, BlobTransform), encoders (Base64Transform, HexTransform, 
	DESKeyTransform), and encrypting transforms (DESTransform, RSATransform, SHATransform, etc).  
	The main difference between the types are in their constructors, where source transforms 
	get a character string or byte array, encoders get a source transform and an encode/decode 
	flag, while encrypting transforms get a source transform and an encryption type or mode.  
	There are also a number of template transforms to aid the process of forming daisy chains.  
	The EncryptTransform, for sample, takes as template parameters the name of a source class, 
	an encryption class, and an encoding class and is, in essence, a composite transform.  
	Encryption transforms general export ad hoc methods for key specification, usually as 
	transforms.  These methods are necessarily outside of the base class.
*/


class Transform  
{
public:
	virtual ~Transform() {}
	virtual unsigned int get (unsigned int bufferLength, UCHAR *buffer) = 0;
	virtual unsigned int getLength() = 0;
	virtual void reset() = 0;
};

#endif // !defined(AFX_TRANSFORM_H__C476BAAD_19F9_4BFD_ACF5_C9F9B82A8E93__INCLUDED_)
