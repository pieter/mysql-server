dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_USE_BUNDLED_LIBEVENT
dnl
dnl SYNOPSIS
dnl   MYSQL_USE_BUNDLED_LIBEVENT()
dnl
dnl DESCRIPTION
dnl  Add defines so libevent is built and linked with
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_USE_BUNDLED_LIBEVENT], [

  libevent_dir="libevent"
  AC_SUBST([libevent_dir])

  libevent_libs="\$(top_builddir)/extra/libevent/libevent.a"
  libevent_includes="-I\$(top_builddir)/extra/libevent"
  libevent_test_option="--mysqld=--thread-handling=pool-of-threads"
  AC_SUBST(libevent_libs)
  AC_SUBST(libevent_includes)
  AC_SUBST(libevent_test_option)

  AC_DEFINE([HAVE_LIBEVENT], [1], [If we want to use libevent and have connection pooling])
  AC_MSG_RESULT([using bundled libevent])

  dnl Things that libevent needs
  AC_CHECK_HEADERS(inttypes.h stdint.h poll.h signal.h sys/epoll.h sys/time.h \
                   sys/queue.h sys/event.h sys/devpoll.h netinet/in6.h)

if test "x$ac_cv_header_sys_queue_h" = "xyes"; then
	AC_MSG_CHECKING(for TAILQ_FOREACH in sys/queue.h)
	AC_EGREP_CPP(yes,
[
#include <sys/queue.h>
#ifdef TAILQ_FOREACH
 yes
#endif
],	[AC_MSG_RESULT(yes)
	 AC_DEFINE(HAVE_TAILQFOREACH, 1,
		[Define if TAILQ_FOREACH is defined in <sys/queue.h>])],
	AC_MSG_RESULT(no)
	)
fi

if test "x$ac_cv_header_sys_time_h" = "xyes"; then
	AC_MSG_CHECKING(for timeradd in sys/time.h)
	AC_EGREP_CPP(yes,
[
#include <sys/time.h>
#ifdef timeradd
 yes
#endif
],	[ AC_DEFINE(HAVE_TIMERADD, 1,
		[Define if timeradd is defined in <sys/time.h>])
	  AC_MSG_RESULT(yes)] ,AC_MSG_RESULT(no)
)
fi

if test "x$ac_cv_header_sys_time_h" = "xyes"; then
	AC_MSG_CHECKING(for timercmp in sys/time.h)
	AC_EGREP_CPP(yes,
[
#include <sys/time.h>
#ifdef timercmp
 yes
#endif
],	[ AC_DEFINE(HAVE_TIMERCMP, 1,
		[Define if timercmp is defined in <sys/time.h>])
	  AC_MSG_RESULT(yes)] ,AC_MSG_RESULT(no)
)
fi

if test "x$ac_cv_header_sys_time_h" = "xyes"; then
	AC_MSG_CHECKING(for timerclear in sys/time.h)
	AC_EGREP_CPP(yes,
[
#include <sys/time.h>
#ifdef timerclear
 yes
#endif
],	[ AC_DEFINE(HAVE_TIMERCLEAR, 1,
		[Define if timerclear is defined in <sys/time.h>])
	  AC_MSG_RESULT(yes)] ,AC_MSG_RESULT(no)
)
fi

if test "x$ac_cv_header_sys_time_h" = "xyes"; then
	AC_MSG_CHECKING(for timerisset in sys/time.h)
	AC_EGREP_CPP(yes,
[
#include <sys/time.h>
#ifdef timerisset
 yes
#endif
],	[ AC_DEFINE(HAVE_TIMERISSET, 1,
		[Define if timerisset is defined in <sys/time.h>])
	  AC_MSG_RESULT(yes)] ,AC_MSG_RESULT(no)
)
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(vasprintf strsep getaddrinfo getnameinfo inet_ntop)

if test "x$ac_cv_func_clock_gettime" = "xyes"; then
   AC_DEFINE(DNS_USE_CPU_CLOCK_FOR_ID, 1, [Define if clock_gettime is available in libc])
else
   AC_DEFINE(DNS_USE_GETTIMEOFDAY_FOR_ID, 1, [Define is no secure id variant is available])
fi

AC_MSG_CHECKING(for F_SETFD in fcntl.h)
AC_EGREP_CPP(yes,
[
#define _GNU_SOURCE
#include <fcntl.h>
#ifdef F_SETFD
yes
#endif
],	[ AC_DEFINE(HAVE_SETFD, 1,
	      [Define if F_SETFD is defined in <fcntl.h>])
	  AC_MSG_RESULT(yes) ], AC_MSG_RESULT(no))

needsignal=no
if test "x$ac_cv_func_select" = "xyes" ; then
	 AC_LIBOBJ(select)
	 needsignal=yes
fi

if test "x$ac_cv_func_poll" = "xyes" ; then
	 AC_LIBOBJ(poll)
	 needsignal=yes
fi

haveepoll=no
AC_CHECK_FUNCS(epoll_ctl, [haveepoll=yes], )
if test "x$haveepoll" = "xyes" ; then
	AC_DEFINE(HAVE_EPOLL, 1,
		[Define if your system supports the epoll system calls])
	AC_LIBOBJ(epoll)
	needsignal=yes
fi

havedevpoll=no
if test "x$ac_cv_header_sys_devpoll_h" = "xyes"; then
	AC_DEFINE(HAVE_DEVPOLL, 1,
		    [Define if /dev/poll is available])
        AC_LIBOBJ(devpoll)
fi

havekqueue=no
if test "x$ac_cv_header_sys_event_h" = "xyes"; then
	AC_CHECK_FUNCS(kqueue, [havekqueue=yes], )
	if test "x$havekqueue" = "xyes" ; then
		AC_MSG_CHECKING(for working kqueue)
		AC_TRY_RUN(
#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int
main(int argc, char **argv)
{
	int kq;
	int n;
	int fd[[2]];
	struct kevent ev;
	struct timespec ts;
	char buf[[8000]];

	if (pipe(fd) == -1)
		exit(1);
	if (fcntl(fd[[1]], F_SETFL, O_NONBLOCK) == -1)
		exit(1);

	while ((n = write(fd[[1]], buf, sizeof(buf))) == sizeof(buf))
		;

        if ((kq = kqueue()) == -1)
		exit(1);

	ev.ident = fd[[1]];
	ev.filter = EVFILT_WRITE;
	ev.flags = EV_ADD | EV_ENABLE;
	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	if (n == -1)
		exit(1);
	
	read(fd[[0]], buf, sizeof(buf));

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	n = kevent(kq, NULL, 0, &ev, 1, &ts);
	if (n == -1 || n == 0)
		exit(1);

	exit(0);
}, [AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_WORKING_KQUEUE, 1,
		[Define if kqueue works correctly with pipes])
    AC_LIBOBJ(kqueue)], AC_MSG_RESULT(no), AC_MSG_RESULT(no))
	fi
fi

haveepollsyscall=no
if test "x$ac_cv_header_sys_epoll_h" = "xyes"; then
	if test "x$haveepoll" = "xno" ; then
		AC_MSG_CHECKING(for epoll system call)
		AC_TRY_RUN(
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/epoll.h>
#include <unistd.h>

int
epoll_create(int size)
{
	return (syscall(__NR_epoll_create, size));
}

int
main(int argc, char **argv)
{
	int epfd;

	epfd = epoll_create(256);
	exit (epfd == -1 ? 1 : 0);
}, [AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_EPOLL, 1,
	[Define if your system supports the epoll system calls])
    needsignal=yes
    AC_LIBOBJ(epoll_sub)
    AC_LIBOBJ(epoll)], AC_MSG_RESULT(no), AC_MSG_RESULT(no))
	fi
fi

haveeventports=no
AC_CHECK_FUNCS(port_create, [haveeventports=yes], )
if test "x$haveeventports" = "xyes" ; then
	AC_DEFINE(HAVE_EVENT_PORTS, 1,
		[Define if your system supports event ports])
	AC_LIBOBJ(evport)
	needsignal=yes
fi
if test "x$needsignal" = "xyes" ; then
	AC_LIBOBJ(signal)
fi

AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPE(uint64_t, unsigned long long)
AC_CHECK_TYPE(uint32_t, unsigned int)
AC_CHECK_TYPE(uint16_t, unsigned short)
AC_CHECK_TYPE(uint8_t, unsigned char)
AC_CHECK_TYPES([struct in6_addr], , ,
[#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif])

AC_MSG_CHECKING([for socklen_t])
AC_TRY_COMPILE([
 #include <sys/types.h>
 #include <sys/socket.h>],
  [socklen_t x;],
  AC_MSG_RESULT([yes]),
  [AC_MSG_RESULT([no])
  AC_DEFINE(socklen_t, unsigned int,
	[Define to unsigned int if you dont have it])]
)

])


dnl ------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_LIBEVENT
dnl
dnl SYNOPSIS
dnl   MYSQL_CHECK_LIBEVENT
dnl
dnl ------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_LIBEVENT], [

  AC_CONFIG_FILES(extra/libevent/Makefile)

  AC_MSG_CHECKING(for libevent)
  AC_ARG_WITH([libevent],
      [  --with-libevent         use libevent and have connection pooling],
      [with_libevent=$withval],
      [with_libevent=no]
  )

  if test "$with_libevent" != "no"
  then
    MYSQL_USE_BUNDLED_LIBEVENT
  fi
  AM_CONDITIONAL([HAVE_LIBEVENT], [ test "$with_libevent" != "no" ])
])
