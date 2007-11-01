AUTOMAKE_OPTIONS = foreign no-dependencies

EXTRA_DIST = select.c poll.c epoll.c epoll_sub.c devpoll.c kqueue.c \
	evport.c evsignal.h signal.c event-internal.h README compat/sys

lib_LTLIBRARIES = libevent.la

SYS_LIBS =
SYS_SRC =
SYS_INCLUDES =

libevent_la_SOURCES = event.c buffer.c evbuffer.c log.c event_tagging.c \
	http.c evhttp.h http-internal.h evdns.c evdns.h strlcpy.c \
	strlcpy-internal.h strlcpy-internal.h
libevent_la_LIBADD = @LTLIBOBJS@
libevent_la_LDFLAGS = -release 1.3e -version-info 1:3:0

include_HEADERS = event.h evhttp.h evdns.h log.h

INCLUDES = -Icompat -I$(top_srcdir)/include
