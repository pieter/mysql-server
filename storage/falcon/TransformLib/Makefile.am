# Copyright (C) 2006, 2007 MySQL AB
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

# Re-enable implicit templates and exceptions
CXXFLAGS= @CXXFLAGS@ -fimplicit-templates -fexceptions

INCLUDES= -I$(srcdir)/.. @ZLIB_INCLUDES@

transform_sources=	 Base64Transform.cpp BERDecode.cpp BERException.cpp \
			 FileTransform.cpp HexTransform.cpp NullTransform.cpp \
			 SHATransform.cpp StreamTransform.cpp \
			 StringTransform.cpp TransformException.cpp \
			 TransformUtil.cpp

transform_headers=	 Base64Transform.h BERDecode.h BEREncode.h \
			 BERItem.h BERObject.h \
			 BERException.h DecodeTransform.h DecryptTransform.h \
			 EncodeTransform.h \
			 EncryptTransform.h FileTransform.h HexTransform.h \
			 NullTransform.h SHATransform.h \
			 StreamTransform.h StringTransform.h \
			 TransformException.h Transform.h TransformUtil.h

noinst_LTLIBRARIES=	 libtransform.la
noinst_LIBRARIES=	 libtransform.a
noinst_HEADERS=		$(transform_headers)

libtransform_la_CXXFLAGS= $(AM_CXXFLAGS) $(INCLUDES)
libtransform_la_SOURCES= $(transform_sources)
libtransform_a_CXXFLAGS= $(AM_CXXFLAGS) $(INCLUDES)
libtransform_a_SOURCES=	 $(transform_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
