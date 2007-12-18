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

// License.cpp: implementation of the License class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "Engine.h"


#include "License.h"
#include "SQLError.h"
#include "Stream.h"
#include "Socket.h"
#include "MACAddress.h"
#include "StringTransform.h"
#include "DecodeTransform.h"
#include "RSATransform.h"
#include "EncryptTransform.h"
#include "DecryptTransform.h"
#include "SHATransform.h"
#include "Base64Transform.h"
#include "EncodeTransform.h"
#include "HexTransform.h"
#include "TransformException.h"
#include "TransformUtil.h"
#include "KeyGen.h"

#ifdef ENGINE
#include "Log.h"
#endif

#define MAX_LINE		60

static const char *publicKey =
	"3082011F300D06092A864886F70D01010105000382010C003082010702818100ED096864ADBAF1F5"
	"2AB1120D9416E13AC64F10B911774501F3F2C8805A010A0FE900F69574BB39EE9BD35DE78EF08696"
	"78B7C817F0ABE4DCB100E708D9315DD0BC67FF71BEF3D3010070EF3BFE938FCD56AAAA82FAB89A31"
	"02082445F90C407C006D5091BCCB968475779281D0DB64A79AA347A5FA5864A67A0418C24FEB8DA7"
	"0281802BAA2770DCA26FF087DD3FF50711B039FC1C09D13FD956D133ACB9251E0DAA464C9B1FF31C"
	"3D6FB96D8BFD1671E8F05F163CCD47C74815F2C24A4581A145B9B391FF015C7D13999FE3F8EECD13"
	"A2C653E6062C173A5BC7992D7BF8F74A6304C022E05D6216FA9019C9997F69BA7C6845127ECE8914"
	"FF77F19FE9F35AEA7F041B";

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


License::License(const char *text)
{
	//license = text;
	char *p = license.getBuffer(strlen(text));

	for (char c; c = *text++;)
		if (c != '\r')
			*p++ = c;

	*p = 0;
	license.releaseBuffer();
	valid = validate();
}

License::License()
{
}

License::~License()
{

}

JString License::decrypt(const char *cipherText, const char *key)
{
	int len = strlen(cipherText);
	int bytes = len * 6 / 8;
	DecryptTransform<StringTransform,Base64Transform,RSATransform> 
			decrypt (cipherText, RSA_publicDecrypt);
	DecodeTransform<StringTransform, HexTransform> privateKey(key);
	decrypt.decrypt.setPublicKey(&privateKey);
	decrypt.decode.ignoreStrays = true;

	return TransformUtil::getString(&decrypt);
}

const char* License::find(const char *pattern, const char *string)
{
	char first = *pattern;

	for (const char *s = string; *s; ++s)
		if (*s == first)
			{
			const char *p = pattern + 1;
			for (const char *t = s + 1; *p && *p == *t; ++p, ++t)
				;
			if (!*p)
				return s;
			}

	return NULL;
}

void License::getGook(const char **cipherText, UCHAR **cipherBinary, const UCHAR *endBuffer)
{
	const char *cp = *cipherText;
	UCHAR *g = *cipherBinary;

	for (char c; (c = *cp++) && c != 'T' && g < endBuffer;)
		{
		if (c == '\n' || c == '\r')
			if (!(c = *cp++))
				break;
		UCHAR v = (c - 'A') << 4;
		c = *cp++;
		if (c == '\n')
			if (!(c = *cp++))
				break;
		v |= (c - 'a');
		*g++ = v;
		}

	*cipherText = cp;
	*cipherBinary = g;
}

const char* License::findAttribute(const char *attribute, const char *string)
{
	for (const char *p = string; *p;)
		{
		for (const char *a = attribute; *a == *p; ++a, ++p)
			;
		if (*p == ':')
			{
			++p;
			while (*p == ' ' || *p == '\t')
				++p;
			return p;
			}
		while (*p && *p++ != '\n')
			;
		}

	return NULL;
}

JString License::getAttribute(const char *attribute, const char *string)
{
	const char *value = findAttribute (attribute, string);

	if (!value)
		return "";

	const char *p = value;

	while (*p && *p != '\n')
		++p;

	return JString (value, p - value);
}

bool License::validate()
{
	try
		{
		id = getAttribute (LICENSE_ID, license);

		// Find beginning of license

		const char *p = find (LICENSE_HDR, license);

		if (!p)
			return invalidLicense ("missing license");

		const char *t = p + sizeof (LICENSE_HDR) - 1;

		// Find end of body and beginning of authority

		if (!(p = find (AUTHORITY, t)))
			return invalidLicense ("missing authority block");

		body = JString (t, p - t);

		if (!validateSignature (p))
			return false;

		// Check product of authority against product of body

		if (!validateAttribute (PRODUCT, body, product))
			return invalidLicense ("wrong product");

		// Signature looks ok.  Check digest of body against digest in signature

		JString digestText = getAttribute (DIGEST, signature);

		if (!validateDigest (digestText, body))
			return invalidLicense ("inconsistent license digest");

		// Check for expiration

		expiration = getDateAttribute (EXPIRES, body);

		// Check IP address of server

		int32 ipAddress = getIpAddress (0);

		if (ipAddress && !Socket::validateLocalAddress (ipAddress))
			return invalidLicense ("wrong ip address");

		JString value = getAttribute (MACADDRESS, body);

		if (!value.IsEmpty())
			{
			int64 address = MACAddress::getAddress (value);
			if (!MACAddress::isAddress (address))
				return invalidLicense ("wrong MAC address");
			}
		}
	catch (TransformException &exception)
		{
		exception;
		return invalidLicense (exception.getText());
		}

	return true;
}

bool License::validateAttribute(const char *attribute, const char *string, const char *value)
{
	const char *p = findAttribute (attribute, string);

	if (!p)
		return false;

	for (; *value == *p; ++value, ++p)
		;

	return (*value == 0) && (*p == 0 || *p == '\n');
}

JString License::getAttribute(const char *name)
{
	return getAttribute (name, body);
}

bool License::isExpired(DateTime *now)
{
	if (expiration.isNull())
		return false;

	return expiration.before (*now);
}

int License::attributeLength(const char *attribute)
{
	const char *p = attribute;

	while (*p && *p != '\n')
		++p;

	return p - attribute;
}

JString License::deformat(const char *text, int length)
{
	JString string;
	char *p = string.getBuffer (length);

	for (const char *q = text, *end = q + length; q < end; ++q)
		if (*q != '\n' && *q != '\r')
			*p++ = *q;

	*p = 0;

	return string;
}

bool License::validateSignature(const char *license)
{
	try
		{
		// Find beginning of the authority section

		const char *p = find (AUTHORITY, license);

		if (!p)
			return invalidLicense ("missing authority");

		const char *t = p + sizeof (AUTHORITY) - 1;
		
		// Find end of authority and beginning of signature key

		if (!(p = find (KEY, t)))
			return invalidLicense ("missing key");

		// Extract authority gook and decrypt using public key.
		// From authority, get both product and signature decrypt
		// key digest.

		JString gook = JString (t, p - t);
		authority = decrypt (gook, publicKey);
		t = p + sizeof (KEY) - 1;

		// Find end of signature key
		
		if (!(p = find (SIGNATURE, t)))
			return invalidLicense ("missing signature");
		
		// Get signature key.  Check digest of signature key against authority

		JString signatureKey = deformat (t, p - t);
		product = getAttribute (PRODUCT, authority);
		JString signatureDigest = getAttribute (KEYDIGEST, authority);

		if (!validateDigest (signatureDigest, signatureKey))
			return invalidLicense ("invalid digest");

		t = p + sizeof (SIGNATURE) - 1;
				
		// Find end of signature (and end of license)

		if (!(p = find (END, t)))
			return invalidLicense ("missing end");

		// Extract and decode signature string

		gook = JString (t, p - t);
		signature = decrypt (gook, signatureKey);

		// Check product of authority against product of signature
		/***
		if (!validateAttribute (PRODUCT, signature, product))
			return false;
		***/

		}
	catch (TransformException &exception)
		{
		exception;
		return invalidLicense (exception.getText());
		}

	return true;
}

bool License::validateDigest(const char *digestText, const char *text)
{
	DecodeTransform<StringTransform,Base64Transform> decode(digestText);
	EncodeTransform<StringTransform,SHATransform> encode(text);

	return TransformUtil::compareDigests(&decode, &encode);
}

DateTime License::getDateAttribute(const char *attribute, const char *textBlock)
{
	DateTime date;
	const char *value = findAttribute ("Expires", textBlock);

	if (value)
		{
		int length = attributeLength (value);
		date = DateTime::convert (value, length);
		}
	else
		date.setSeconds (0);

	return date;
}


JString License::loadFile(const char *fileName)
{
	FILE *file = fopen (fileName, "r");

	if (!file)
		return "";

	Stream stream;
	char buffer [1024];

	while (fgets (buffer, sizeof (buffer), file))
		{
		for (char *p = buffer, *q = buffer;; ++q)
			if (*q != '\r' && (*p++ = *q) == 0)
				break;
		stream.putSegment (buffer);
		}

	fclose (file);

	return stream.getJString();
}

JString License::encrypt(const char *text, const char *encryptKey, const char *seed)
{
	EncryptTransform<StringTransform,RSATransform,Base64Transform> 
			encrypt (text, RSA_publicEncrypt);
	DecodeTransform<StringTransform, HexTransform> publicKey(encryptKey);
	encrypt.encrypt.setPublicKey(&publicKey);

	return TransformUtil::getString(&encrypt);
}

bool License::invalidLicense(const char *why)
{
#ifdef ENGINE
	Log::log ("License %s invalid: %s\n", (const char*) id, why);
#endif

	return false;
}

int32 License::getIpAddress(int defaultValue)
{
	JString value = getAttribute (IPADDRESS, body);

	if (value.IsEmpty())
		return defaultValue;

	return Socket::translateAddress (getAttribute (IPADDRESS, body));
}


