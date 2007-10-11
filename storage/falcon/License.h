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

// License.h: interface for the License class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LICENSE_H__92E67AA6_5BF5_11D4_98EE_0000C01D2301__INCLUDED_)
#define AFX_LICENSE_H__92E67AA6_5BF5_11D4_98EE_0000C01D2301__INCLUDED_

#include "DateTime.h"
#include "JString.h"	// Added by ClassView

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define LICENSE_HDR	"-----License-----\n"
#define AUTHORITY	"\n-----Authority-----\n"
#define KEY			"\n-----Key-----\n"
#define SIGNATURE	"\n-----Signature-----\n"
#define SIGNKEY		"\n-----SignatureKey-----\n"
#define END			"\n-----End-----"

#define LICENSE_ID	"Id"
#define PRODUCT		"Product"
#define DIGEST		"Digest"
#define KEYDIGEST	"KeyDigest"
#define EXPIRES		"Expires"
#define IPADDRESS	"IPAddress"
#define MACADDRESS	"MACAddress"
#define SESSIONS	"Sessions"

class DateTime;
class Transform;

class License  
{
public:
	int32 getIpAddress (int defaultValue);
	bool invalidLicense (const char *why);
	static JString encrypt (const char *text, const char *encryptKey, const char *seed);
	static JString loadFile (const char *fileName);
	static DateTime getDateAttribute (const char *attribute, const char *textBlock);
	static bool validateDigest (const char *digestString, const char *text);
	bool validateSignature (const char *signature);
	static JString deformat (const char *string, int length);
	static int attributeLength (const char *attribute);
	bool isExpired(DateTime *now);
	JString getAttribute (const char *name);
	bool validateAttribute (const char *attribute, const char *string, const char *value);
	bool validate();
	static JString getAttribute (const char *attribute, const char *string);
	static const char* findAttribute (const char *attribute, const char *string);
	void getGook (const char **cipherText, UCHAR **cipherBinary, const UCHAR *endBuffer);
	static const char* find (const char *pattern, const char *string);
	static JString decrypt(const char *cypherText, const char *key);
	License(const char *text);
	virtual ~License();

	JString		license;
	JString		authority;
	JString		signature;
	JString		body;
	JString		product;
	JString		id;
	bool		valid;
	License		*next;
	DateTime	expiration;
protected:
	License();
};

#endif // !defined(AFX_LICENSE_H__92E67AA6_5BF5_11D4_98EE_0000C01D2301__INCLUDED_)
