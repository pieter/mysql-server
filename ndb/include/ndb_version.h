/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_VERSION_H
#define NDB_VERSION_H

#include <stdio.h>
#include <string.h>

#include <version.h>

#define MAKE_VERSION(A,B,C) (((A) << 16) | ((B) << 8)  | ((C) << 0))

/**
 * version of this build
 */

#define NDB_VERSION_MAJOR 3
#define NDB_VERSION_MINOR 4
#define NDB_VERSION_BUILD 5
#define NDB_VERSION_STATUS "alpha"

#define NDB_VERSION_D MAKE_VERSION(NDB_VERSION_MAJOR, NDB_VERSION_MINOR, NDB_VERSION_BUILD)

#define NDB_VERSION_STRING (getVersionString(NDB_VERSION, NDB_VERSION_STATUS))

#define NDB_VERSION_TAG_STRING "$Name:  $"

#define NDB_VERSION ndbGetOwnVersion()

/**
 * Version id 
 *
 *  Used by transporter and when communicating with
 *     managment server
 */
//#define NDB_VERSION_ID 0

#endif
 
