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

// Socket.cpp: implementation of the Socket class.
//
//////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "Engine.h"
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "Log.h"

#ifdef ENGINE
#include "SyncObject.h"
#include "Sync.h"
#endif

#undef ERROR
#include "Engine.h"

#ifdef _WIN32
#undef ERROR
#define SOCKET(fd)		(active_fds.fd_array [fd])
#define MAX_FD			(active_fds.fd_count)
#define socklen_t		int
#define sockopt_t		char*
#define SOCKET_TIMEOUT	WSAETIMEDOUT

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#define SOCKET(fd)		(fd)
#define MAX_FD			max_fd + 1
#define sockopt_t		void*
#define SOCKET_TIMEOUT	EWOULDBLOCK
#endif

#undef ERROR
#include "Engine.h"
#include "Socket.h"
#include "SQLError.h"
#include "Protocol.h"

#define DEFAULT_BUFFER_SIZE		2048

static int foo = Socket::initialize();

#ifdef ENGINE
SyncObject	hostSyncObject;
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Socket::Socket()
{
	init();
	socket = CLOSED_SOCKET;
}

Socket::Socket(socket_t sock, sockaddr_in *addr)
{
	init();
	socket = sock;

	if (addr)
		address = *addr;
	//FD_SET (socket, &fd_reads);
}

void Socket::init()
{
	socket = CLOSED_SOCKET;
	bufferLength = DEFAULT_BUFFER_SIZE;
	readBuffer = NULL;
	writeBuffer = NULL;
	shutdownInProgress = false;
	swap = false;
}

Socket::~Socket()
{
	if (readBuffer)
		delete [] readBuffer;

	if (writeBuffer)
		delete [] writeBuffer;
}

void Socket::bind(int ipAddress, int port)
{
#ifdef ENGINE
	ASSERT (INADDR_ANY == 0);
#endif
	create();
	address.sin_family = AF_INET;
	//address.sin_addr.s_addr = INADDR_ANY;
	address.sin_addr.s_addr = htonl (ipAddress);
	address.sin_port = htons (port);

	int optVal = 1;
	//int ret = 
	setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char*) &optVal, sizeof(optVal));

	if (::bind (socket, (struct sockaddr*) &address, sizeof(address)) < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "bind failed (%d)", getSocketError());

}

#ifndef ENGINE
Socket* Socket::acceptSocket()
{
	struct sockaddr_in	address;
	socklen_t size = sizeof(address);
    fd_set fd_reads;
	FD_ZERO (&fd_reads);
	FD_SET (socket, &fd_reads);

	int n = select (FD_SETSIZE, &fd_reads, NULL, NULL, NULL);

	if (shutdownInProgress)
		return NULL;

	int sock = ::accept (socket, (struct sockaddr*) &address, &size);

	if (sock < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "ACCEPT failed");

	return new Socket (sock, &address);
}
#else
Protocol* Socket::acceptProtocol()
{
	struct sockaddr_in	address;
	socklen_t size = sizeof(address);
    fd_set fd_reads;
	FD_ZERO (&fd_reads);
	timeval tv;

	for (;;)
		{
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_SET (socket, &fd_reads);
		//int n = 
		select(FD_SETSIZE, &fd_reads, NULL, NULL, &tv);
		
		if (shutdownInProgress)
			return NULL;
			
		if (FD_ISSET (socket, &fd_reads))
			break;
		}

	socket_t sock = ::accept(socket, (struct sockaddr*) &address, &size);

	if (sock < 0)
		throw SQLEXCEPTION(NETWORK_ERROR, "ACCEPT failed");

	return new Protocol(sock, &address);
}
#endif

int Socket::read()
{
	if (!readBuffer)
		{
		readBuffer = new char [bufferLength];
		readPtr = end = readBuffer;
		}

	int n = recv (socket, readBuffer, bufferLength, MSG_NOSIGNAL);

	if (n <= 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "recv failed");

	readPtr = readBuffer;
	end = readBuffer + n;

	return n;
}

int Socket::flush()
{
	if (socket == CLOSED_SOCKET)
		throw SQLEXCEPTION(NETWORK_ERROR, "socket not open");

	int length = (int) (writePtr - writeBuffer);

	if (length)
		{
		int n = send(socket, writeBuffer, length, MSG_NOSIGNAL);
		
		if (n <= 0)
			throw SQLEXCEPTION(NETWORK_ERROR, "send failed");
		}

	writePtr = writeBuffer;

	return bufferLength;
}

void Socket::getBytes(int count, void * buffer)
{
	if (!readBuffer)
		{
		readBuffer = new char [bufferLength];
		readPtr = end = readBuffer;
		}

	char *p = (char*) buffer;

	for (int length = count; length;)
		{
		int l = (int) (end - readPtr);
		
		while (l == 0)
			l = read();
			
		if (length < l)
			l = length;
			
		if (l)
			{
			memcpy (p, readPtr, l);
			length -= l;
			p += l;
			readPtr += l;
			}
		}
}

void Socket::putBytes(int count, const void * buffer)
{
	if (!writeBuffer)
		{
		writeBuffer = new char [bufferLength];
		writePtr = writeBuffer;
		}

	char *p = (char*) buffer;

	for (int length = count; length;)
		{
		int l = (int) (writeBuffer + bufferLength - writePtr);
		
		if (l == 0)
			l = flush();
			
		if (length < l)
			l = length;
			
		if (l)
			{
			memcpy (writePtr, p, l);
			length -= l;
			p += l;
			writePtr += l;
			}
		}
}

void Socket::putLong(int value)
{
	if (swap)
		swapBytes(sizeof(value), &value);

	putBytes(sizeof(value), &value);
}

int Socket::getLong()
{
	int value;

	getBytes(sizeof(value), &value);

	if (swap)
		swapBytes(sizeof(value), &value);

	return value;
}

void Socket::close()
{
	shutdownInProgress = true;
	close (socket);
	socket = CLOSED_SOCKET;
}

void Socket::connect(const char * host, int port)
{
	char	hostname [256], c, *q = hostname;
	int		portNumber = port;

	// See if host has an implicit socket number

	if (host)
		{
		const char *p;
		for (p = host; (c = *p++) && c != ':';)
			*q++ = c;

		*q = 0;

		if (c)
			portNumber = atoi (p);
		}

	create();
	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons (portNumber);

#ifdef ENGINE
	Sync sync (&hostSyncObject, "Socket::connect");
#endif

	if (host)
		{
#ifdef ENGINE
		sync.lock (Exclusive);
#endif
		struct hostent	*hp = gethostbyname (hostname);
		if (!hp)
			{
			close();
			throw SQLEXCEPTION (NETWORK_ERROR, 
									"gethostbyname for %s (%s) failed with %d", 
									hostname, host, getSocketError());
			}
		memcpy (&address.sin_addr, hp->h_addr, hp->h_length);
		}
	else
		address.sin_addr.s_addr = INADDR_ANY;

	if (::connect (socket, (struct sockaddr*) &address, sizeof(address)) < 0)
		{
		close();
		throw SQLEXCEPTION (NETWORK_ERROR, 
				"connection to \"%s\" on port %d failed with error %d", hostname, portNumber, getSocketError());
		}

	readPtr = end = readBuffer;
	writePtr = writeBuffer;
}

int Socket::initialize()
{
#ifdef _WIN32
	WSADATA	wsa;

	int n = WSAStartup (MAKEWORD (1, 1), &wsa);
	if (n)
		throw SQLError (NETWORK_ERROR, "WSAStartup failed with %d\n", n);
#endif

	return 0;
}

void Socket::putString(int length, const char * string)
{
	if (string)
		{
		putLong (length);
		putBytes(length, (void*) string);
		}
	else
		putLong (-1);
}

char* Socket::getString()
{
	int32 length = getLong();

	if (length < 0)
		return NULL;

	char *string = new char [length + 1];
	getBytes(length, string);
	string [length] = 0;

	return string;
}

void Socket::putHandle(int32 handle)
{
	putBytes(sizeof(handle), &handle);
}

int32 Socket::getHandle()
{
	int32 handle;
	getBytes(sizeof(handle), &handle);
	
	return handle;	
}

void Socket::putString(const char * string)
{
	putString((int) strlen (string), string);
}

void Socket::putShort(short value)
{
	if (swap)
		swapBytes(sizeof(value), &value);

	putBytes(sizeof(value), &value);
}

short Socket::getShort()
{
	short value;

	getBytes(sizeof(value), &value);

	if (swap)
		swapBytes(sizeof(value), &value);

	return value;
}

void Socket::putDouble(double value)
{
	if (swap)
		swapBytes(sizeof(value), &value);

	putBytes(sizeof(value), &value);
}

double Socket::getDouble()
{
	double value;

	getBytes(sizeof(value), &value);

	if (swap)
		swapBytes(sizeof(value), &value);

	return value;
}

void Socket::putQuad(QUAD value)
{
	if (swap)
		swapBytes(sizeof(value), &value);

	putBytes(sizeof(value), &value);
}

QUAD Socket::getQuad()
{
	QUAD value;

	getBytes(sizeof(value), &value);
	
	if (swap)
		swapBytes(sizeof(value), &value);

	return value;
}

void Socket::putBoolean(bool value)
{
	char byte = value;
	putBytes(sizeof(byte), &byte);
}

bool Socket::getBoolean()
{
	char value;

	getBytes(sizeof(value), &value);

	return (value) ? true : false;
}

void Socket::listen(int count)
{
	//int n = 
	::listen (socket, 5);
}

/***
int32 Socket::getAddress()
{
	char hostname [256];

	if (gethostname (hostname, sizeof(hostname)))
		return 0;

	struct hostent	*hp = gethostbyname (hostname);

	if (!hp)
		return 0;

	UCHAR *bytes = (UCHAR*) hp->h_addr_list [0];
	ULONG address = (bytes [0] << 24) |
					(bytes [1] << 16) |
					(bytes [2] << 8) |
					bytes [3];

	return address;
}
***/

int Socket::getInetFamily()
{
	return AF_INET;
}

void Socket::create()
{
	socket = ::socket (AF_INET, SOCK_STREAM, 0);
	
	if (socket < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "socket allocation failed");
}

void Socket::shutdown()
{
	//Synchronize::shutdown();
	close();
}

char Socket::getByte()
{
	char c;
	getBytes(1, &c);

	return c;
}

void Socket::writeString(const char * string)
{
	putBytes((int) strlen (string), string);
}

int32 Socket::getSocketAddress()
{
	socklen_t n = sizeof(address);

	if (getsockname (socket, (struct sockaddr*) &address, &n) < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "getsockname failed");

	return ntohl (address.sin_addr.s_addr);
}

void Socket::setSwapBytes(bool flag)
{
	swap = true;
}

void Socket::swapBytes(int length, void *bytes)
{
	char *p = (char*) bytes;

	for (int l1 = 0, l2 = length - 1; l1 < l2; ++l1, --l2)
		{
		char c = p [l1];
		p [l1] = p [l2];
		p [l2] = c;
		}
}

int Socket::getLocalPort()
{
	struct sockaddr_in	address;
	socklen_t n = sizeof(address);

	if (getsockname (socket, (struct sockaddr*) &address, &n) < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "getsockname failed");

	return ntohs(address.sin_port);
}

JString Socket::getLocalName()
{
#ifdef ENGINE
	Sync sync (&hostSyncObject, "Socket::getLocalName");
	sync.lock (Exclusive);
#endif

	char buffer [256];
	gethostname(buffer, sizeof(buffer));
	hostent *hp = gethostbyname(buffer);

	if (hp)
		return hp->h_name;
	
	return buffer;
}

void Socket::putByte(char c)
{
	putBytes(1, &c);
}

int32 Socket::getPartnerAddress()
{
	return ntohl(address.sin_addr.s_addr);
}

int32 Socket::translateAddress(const char *hostname)
{
#ifdef ENGINE
	Sync sync (&hostSyncObject, "Socket::translateAddress");
	sync.lock (Exclusive);
#endif

	hostent	*hp = gethostbyname (hostname);

	if (!hp)
		throw SQLEXCEPTION (NETWORK_ERROR, "can't resolve network address %s", hostname);

	struct sockaddr_in		address;
	memcpy(&address.sin_addr, hp->h_addr, hp->h_length);

	return ntohl(address.sin_addr.s_addr);
}

bool Socket::validateLocalAddress(int32 localAddress)
{
	socket_t socket = ::socket (AF_INET, SOCK_STREAM, 0);

	if (socket < 0)
		return false;

	struct sockaddr_in		address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl (localAddress);
	address.sin_port = 0;

	int ret = ::bind(socket, (struct sockaddr*) &address, sizeof(address));
	close (socket);

	return ret >= 0;
}

void Socket::close(socket_t socket)
{
	if (socket == CLOSED_SOCKET)
		return;

	int ret = ::shutdown(socket, 2);

#if _WIN32
	ret = closesocket(socket);
#else
	ret = ::close (socket);
#endif

	socket = 0;
}

int Socket::getSocketError()
{
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

void Socket::setReadTimeout(int milliseconds)
{
	int value = milliseconds;
	socklen_t size = sizeof(value);
	void *p = &value;

#ifndef _WIN32
	timeval timeout;
	timeout.tv_sec = value / 1000;
	timeout.tv_usec = value % 1000 * 1000;
	p = &timeout;
	size = sizeof(timeout);
#endif

	if (setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (sockopt_t) p,  size) < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "setsockopt SO_RCVTIMEO failed with %d\n",
							getSocketError());
}

void Socket::setWriteTimeout(int milliseconds)
{
	int value = milliseconds;
	socklen_t size = sizeof(value);
	void *p = &value;

#ifndef _WIN32
	timeval timeout;
	timeout.tv_sec = value / 1000;
	timeout.tv_usec = value % 1000 * 1000;
	p = &timeout;
	size = sizeof(timeout);
#endif

	if (setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (sockopt_t) p,  size) < 0)
		throw SQLEXCEPTION (NETWORK_ERROR, "setsockopt SO_SNDTIMEO failed with %d\n",
							getSocketError());
}
