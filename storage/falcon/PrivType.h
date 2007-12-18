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

#ifndef _PrivType_
#define _PrivType_

// Option bits from Connection.hasRole()

#define HAS_ROLE			1
#define ROLE_ACTIVE			2
#define DEFAULT_ROLE		4
#define GRANT_OPTION		8

#define PRIV_MASK(priv)		(1 << priv)
#define GRANT_SHIFT			10
#define GRANT_MASK(priv)	(1 << (priv + GRANT_SHIFT))

enum PrivType {
	PrivSelect = 1,
	PrivInsert,
	PrivUpdate,
	PrivDelete,
	PrivGrant,				// deprecated
	PrivAlter,
	PrivExecute,
	};

enum PrivObject {
	PrivTable,
	PrivView,
	PrivProcedure,
	PrivUser,
	PrivRole,
	PrivCoterie,
	};

#define ALL_PRIVILEGES		-1


#endif
