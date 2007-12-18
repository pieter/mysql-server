/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _HdrState_
#define _HdrState_

enum HdrState {
	HdrOpen,			// Database has not been closed
	HdrClosed,			// Database was normally closed
	HdrPartial,			// File is only a partial copy
	};

enum FileType {
	HdrDatabaseFile		= 0,
	HdrRepositoryFile	= 1,
	HdrTableSpace		= 2,
	};


#endif

