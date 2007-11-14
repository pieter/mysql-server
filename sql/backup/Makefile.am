# Copyright (C) 2000 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

## Process this file with automake to create Makefile.in

# TODO: check how to correctly specify libraries (pkglib_LIBRARIES?)

noinst_LTLIBRARIES = libbackup.la libbackupstream.la

INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/sql \
	-I$(top_srcdir)/regex

libbackup_la_SOURCES = \
	stream.cc \
	logger.cc \
	string_pool.cc \
	archive.cc \
	meta_backup.cc \
	data_backup.cc \
	sql_backup.cc \
      be_default.cc \
      be_snapshot.cc \
      buffer_iterator.cc

libbackupstream_la_SOURCES= \
  stream_v1_transport.c \
  stream_v1.c

noinst_HEADERS = \
	api_types.h \
	backup_engine.h \
	backup_kernel.h \
    debug.h \
    error.h \
    stream.h \
    backup_aux.h \
    logger.h \
    map.h \
    string_pool.h \
    archive.h \
    meta_backup.h \
      be_default.h \
      be_snapshot.h \
      buffer_iterator.h \
	stream_v1.h \
	stream_v1_services.h

DEFS = \
	-DMYSQL_SERVER \
	-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
	-DDATADIR="\"$(MYSQLDATAdir)\"" \
	-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
	-DLIBDIR="\"$(MYSQLLIBdir)\"" \
	@DEFS@

EXTRA_DIST =	CMakeLists.txt 
	
# Don't update the files from bitkeeper
%::SCCS/s.%
