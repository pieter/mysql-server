/* Copyright (C) 2000 MySQL AB

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

/*
  thread safe version of some common functions:
  my_inet_ntoa

  This file is also used to make handling of sockets and ioctl()
  portable accross systems.

*/

#ifndef _my_net_h
#define _my_net_h
C_MODE_START

#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if !defined(MSDOS) && !defined(__WIN__) && !defined(HAVE_BROKEN_NETINET_INCLUDES) && !defined(__BEOS__)
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#if !defined(alpha_linux_port)
#include <netinet/tcp.h>
#endif
#endif

#if defined(__EMX__)
#include <sys/ioctl.h>
#define ioctlsocket(A,B,C) ioctl((A),(B),(void *)(C),sizeof(*(C)))
#undef HAVE_FCNTL
#endif	/* defined(__EMX__) */

#if defined(MSDOS) || defined(__WIN__)
#define O_NONBLOCK 1    /* For emulation of fcntl() */
#endif

/* Thread safe or portable version of some functions */

void my_inet_ntoa(struct in_addr in, char *buf);

/*
  Handling of gethostbyname_r()
*/

#if !defined(HPUX10)
struct hostent;
#endif /* HPUX */
#if !defined(HAVE_GETHOSTBYNAME_R)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
void my_gethostbyname_r_free();
#elif defined(HAVE_PTHREAD_ATTR_CREATE) || defined(_AIX) || defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
#define my_gethostbyname_r_free()
#if !defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE) && !defined(HPUX10)
#define GETHOSTBYNAME_BUFF_SIZE sizeof(struct hostent_data)
#endif /* !defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE) */

#elif defined(HAVE_GETHOSTBYNAME_R_RETURN_INT)
#define GETHOSTBYNAME_BUFF_SIZE sizeof(struct hostent_data)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
#define my_gethostbyname_r_free()
#else
#define my_gethostbyname_r(A,B,C,D,E) gethostbyname_r((A),(B),(C),(D),(E))
#define my_gethostbyname_r_free()
#endif /* !defined(HAVE_GETHOSTBYNAME_R) */

#ifndef GETHOSTBYNAME_BUFF_SIZE
#define GETHOSTBYNAME_BUFF_SIZE 2048
#endif

/* On SCO you get a link error when refering to h_errno */
#ifdef SCO
#undef h_errno
#define h_errno errno
#endif

C_MODE_END
#endif
