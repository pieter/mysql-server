# Copyright (C) 2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#Makefile.am example for a plugin

MYSQLLIBdir=		$(pkglibdir)

INCLUDES=		-I$(top_builddir)/include -I$(top_srcdir)/include

EXTRA_LTLIBRARIES=	adt_null.la
pkglib_LTLIBRARIES=	@plugin_audit_null_shared_target@
adt_null_la_LDFLAGS=	-module -rpath $(MYSQLLIBdir)
adt_null_la_CFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
adt_null_la_SOURCES=	audit_null.c

EXTRA_LIBRARIES=	libadtnull.a
noinst_LIBRARIES=	@plugin_audit_null_static_target@
libadtnull_a_CFLAGS=	$(AM_CFLAGS)
libadtnull_a_SOURCES=	audit_null.c

EXTRA_DIST=		plug.in

# Don't update the files from bitkeeper
%::SCCS/s.%
