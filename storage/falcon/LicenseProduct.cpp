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

// LicenseProduct.cpp: implementation of the LicenseProduct class.
//
//////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "Engine.h"
#include "Connection.h"
#include "LicenseProduct.h"
#include "License.h"
#include "LicenseToken.h"
#include "Sync.h"

//#define UNLIMITED			true

#ifndef UNLIMITED
#define UNLIMITED			false
#endif

#undef SERVER_PRODUCT_UNITS
#define SERVER_PRODUCT_UNITS	5

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LicenseProduct::LicenseProduct(LicenseManager *mgr, const char *productName)
{
	manager = mgr;
	product = productName;
	licenses = NULL;
	tokensAllocated = 0;
	activeTokens = NULL;
	availableTokens = 0;
	computeUnits();
}

LicenseProduct::~LicenseProduct()
{
	for (License *license; license = licenses;)
		{
		licenses = license->next;
		delete license;
		}

	LicenseToken *token;

	while (token = availableTokens)
		{
		availableTokens = token->next;
		delete token;
		}

	while (token = activeTokens)
		{
		activeTokens = token->next;
		delete token;
		}

}

License* LicenseProduct::addLicense(const char *id, const char *string)
{
	License *license;

	// Avoid adding duplicates

	for (license = licenses; license; license = license->next)
		if (license->license == string)
			return license;

	license = new License (string);

	if (!addLicense (license))
		{
		delete license;
		return NULL;
		}

	return license;
}


bool LicenseProduct::addLicense(License *license)
{
	if (!license->valid || !license->product.equalsNoCase (product))
		return false;

	license->next = licenses;
	licenses = license;
	computeUnits();

	return true;
}

void LicenseProduct::tokenReleased(LicenseToken *token)
{
	Sync sync (&syncObject, "LicenseProduct::tokenReleased");
	sync.lock (Exclusive);

	if (token->next)
		token->next->prior = token->prior;

	if (token->prior)
		token->prior->next = token->next;
	else
		activeTokens = token->next;

	if (tokensAllocated <= units)
		{
		token->next = availableTokens;
		availableTokens = token;
		}
	else
		{
		--tokensAllocated;
		delete token;
		}
}

LicenseToken* LicenseProduct::getToken()
{
	Sync sync (&syncObject, "LicenseProduct::getToken");
	sync.lock (Exclusive);
	LicenseToken *token;

	if (token = availableTokens)
		{
		token->addRef();
		availableTokens = token->next;
		}
	else
		{
		if (!unlimited && tokensAllocated >= units)
			return NULL;
		++tokensAllocated;
		token = new LicenseToken (this);
		}

	token->prior = NULL;

	if (activeTokens)
		activeTokens->prior = token;

	token->next = activeTokens;
	activeTokens = token;

	return token;
}

void LicenseProduct::computeUnits()
{
	units = 0;
	unlimited = UNLIMITED;

	if (product.equalsNoCase (SERVER_PRODUCT))
		units += SERVER_PRODUCT_UNITS;

	for (License *license = licenses; license; license = license->next)
		{
		JString s = license->getAttribute ("Units");
		if (s == "")
			unlimited = true;
		else
			units += atoi (s);
		}
}

void LicenseProduct::scavenge(DateTime *now)
{
	bool recompute = false;

	for (License *license, **ptr = &licenses; license = *ptr;)
		if (license->isExpired (now))
			{
			*ptr = license->next;
			delete license;
			recompute = true;
			}
		else
			 ptr = &license->next;

	if (recompute)
		computeUnits();
}

void LicenseProduct::deleteLicense(const char *licenseId)
{
	for (License *license, **ptr = &licenses; license = *ptr;)
		if (license->id == licenseId)
			{
			*ptr = license->next;
			delete license;
			}
		else
			ptr = &(*ptr)->next;
}

bool LicenseProduct::isLicensed()
{
	DateTime now;
	now.setNow();

	for (License *license = licenses; license; license = license->next)
		if (license->valid && !license->isExpired (&now))
			return true;

	return false;
}
