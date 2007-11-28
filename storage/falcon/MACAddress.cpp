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

// MACAddress.cpp: implementation of the MACAddress class.
//
//////////////////////////////////////////////////////////////////////


#ifdef _WIN32
//#include <Afx.h>
#include <Windows.h>
#include <Iphlpapi.h>

#else
#include <unistd.h>
#if !defined(__APPLE__) || !defined(__FreeBSD__)
#include <net/if.h>
#include <netinet/in.h>
#endif
#include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "MACAddress.h"

#define MAX_ADDRESSES	10

#ifndef _WIN32
static const char *devices [] = {
	"eth0",
	"eth1",
	"eth2",
	"eth6",
	"xyzzy",
	NULL
	};
#endif

static int count = MACAddress::getAddresses();
static int64 macAddresses [MAX_ADDRESSES];
static const char *hexDigits = "0123456789ABCDEF";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MACAddress::MACAddress()
{

}

MACAddress::~MACAddress()
{

}

int MACAddress::getAddresses()
{
	int count = 0;

#ifdef _WIN32

	char buffer [4096];
	MIB_IFTABLE	*ifTable = (MIB_IFTABLE*) buffer;
	ULONG tableLength = sizeof (buffer);
	DWORD ret = GetIfTable (ifTable, &tableLength, 0);

	if (ret == 0)
		for (DWORD n = 0; n < ifTable->dwNumEntries && count < MAX_ADDRESSES; ++n)
			{
			int length = ifTable->table[n].dwPhysAddrLen;
			if (length > 0 && length <= 8)
				macAddresses [count++] = getAddress (length, ifTable->table[n].bPhysAddr);
			}
#elif defined(__APPLE__) || defined(__FreeBSD__)
	/* do nothing */
#else
	int fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (fd < 0)
		return 0;

	ifreq request;

	//for (const char **device = devices; *device; ++device)
	for (int n = 0; n < 10; ++n)
		{
		//strncpy (request.ifr_name, *device, IFNAMSIZ);
		char device[64];
		sprintf(device, "eth%d", n);
		strncpy (request.ifr_name, device, IFNAMSIZ);

#ifdef SIOCGIFHWADDR
		if (ioctl (fd, SIOCGIFHWADDR, &request) == 0)
			macAddresses [count++] = getAddress (6, (UCHAR*) request.ifr_hwaddr.sa_data);
#else
#ifdef SIOCGENADDR
		if (ioctl (fd, SIOCGENADDR, &request) == 0)
			macAddresses [count++] = getAddress (6, (UCHAR*) request.ifr_ifru.ifru_enaddr[0]);
#endif
#endif
		}

	close (fd);
#endif

	return count;
}

int64 MACAddress::getAddress(int length, UCHAR *bytes)
{
	int64 address = 0;

	for (int n = 0; n < length; ++n)
		address = address << 8 | bytes [n];

	return address;
}

int MACAddress::getAddressCount()
{
	return count;
}

int64 MACAddress::getAddress(int index)
{
	return (index < 0 || index >= count) ? 0 : macAddresses [index];
}

int64 MACAddress::getAddress(const char *string)
{
	int64 address = 0;

	for (const char *p = string; *p;)
		{
		int n = getHexDigit (*p++);
		if (n >= 0)
			{
			int hex = n;
			for (; *p && (n = getHexDigit (*p)) >= 0; ++p)
				hex = hex << 4 | n;
			address = address << 8 | hex;
			}
		}

	return address;
}

int MACAddress::getHexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';

	if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';

	return -1;
}

bool MACAddress::isAddress(int64 address)
{
	for (int n = 0; n < count; ++n)
		if (address == macAddresses [n])
			return true;

	return false;
}

JString MACAddress::getAddressString(int64 address)
{
	char string [32];
	char *p = string;

	for (int shift = 40; shift >= 0; shift -= 8)
		{
		int byte = (int) ((address >> shift) & 0xff);
		*p++ = hexDigits [byte >> 4];
		*p++ = hexDigits [byte & 0xf];
		if (shift)
			*p++ = '-';
		}

	*p = 0;

	return string;
}
