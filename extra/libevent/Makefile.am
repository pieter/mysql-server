AUTOMAKE_OPTIONS = foreign no-dependencies

EXTRA_DIST = README compat/sys

noinst_LIBRARIES = libevent.a

libevent_a_SOURCES = event.c buffer.c evbuffer.c log.c event_tagging.c \
	http.c evhttp.h http-internal.h evdns.c evdns.h strlcpy.c \
	strlcpy-internal.h strlcpy-internal.h event-internal.h \
        select.c poll.c epoll.c epoll_sub.c devpoll.c kqueue.c \
	evport.c evsignal.h signal.c

include_HEADERS = event.h evhttp.h evdns.h log.h

INCLUDES = -Icompat -I$(top_srcdir)/include
