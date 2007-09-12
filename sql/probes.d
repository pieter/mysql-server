/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

provider mysql {

probe external_lock(unsigned long, int);
probe insert_row_start(unsigned long);
probe insert_row_end(unsigned long);
probe filesort_start(unsigned long);
probe filesort_end(unsigned long);
probe delete_start(unsigned long);
probe delete_end(unsigned long);
probe insert_start(unsigned long);
probe insert_end(unsigned long);
probe select_start(unsigned long);
probe select_end(unsigned long);
probe update_start(unsigned long);
probe update_end(unsigned long);
};
