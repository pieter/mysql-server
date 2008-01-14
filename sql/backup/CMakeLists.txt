# Copyright (C) 2006 MySQL AB
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

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/sql/backup
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(BACKUP_SOURCES stream.cc logger.cc kernel.cc
               catalog.cc meta_data.cc data_backup.cc
               be_default.cc buffer_iterator.cc
               be_snapshot.cc be_thread.cc backup_progress.cc
               backup_test.cc)

IF(NOT SOURCE_SUBLIBS)
  ADD_LIBRARY(backup ${BACKUP_SOURCES})
  ADD_LIBRARY(backupstream stream_v1.c stream_v1_transport.c)
ENDIF(NOT SOURCE_SUBLIBS)

TARGET_LINK_LIBRARIES(backup backupstream)
ADD_DEPENDENCIES(backup mysys)

