# Copyright (C) 2004-2005 MySQL AB
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

noinst_LIBRARIES = libdbtup.a

libdbtup_a_SOURCES = \
        DbtupExecQuery.cpp \
        DbtupBuffer.cpp \
        DbtupRoutines.cpp \
        DbtupCommit.cpp \
        DbtupFixAlloc.cpp \
        DbtupTrigger.cpp \
        DbtupAbort.cpp \
        DbtupLCP.cpp \
        DbtupUndoLog.cpp \
        DbtupPageMap.cpp \
        DbtupPagMan.cpp  \
        DbtupStoredProcDef.cpp \
        DbtupMeta.cpp \
        DbtupTabDesMan.cpp \
	DbtupGen.cpp \
        DbtupSystemRestart.cpp \
	DbtupIndex.cpp \
	DbtupScan.cpp \
        DbtupDebug.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbtup.dsp

libdbtup.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libdbtup_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
