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

#ifndef _PageType_
#define _PageType_

// Page Types

enum PageType {
    PAGE_any,
	PAGE_header			= 1,
	PAGE_sections		= 2,
	//PAGE_section		= 3,		 unused
	PAGE_record_locator	= 4,		// was PAGE_section_index
	PAGE_btree			= 5,
	//PAGE_btree_leaf	= 6,		unused
	PAGE_data			= 7,
	PAGE_inventory		= 8,
	PAGE_data_overflow	= 9,
	PAGE_inversion		= 10,
	PAGE_free			= 11,
	PAGE_sequences		= 12,
	PAGE_max
	};

#endif

