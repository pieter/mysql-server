/* Copyright (C) 2000 MySQL AB & Ramil Kalimullin & MySQL Finland AB 
   & TCX DataKonsult AB

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

#ifndef _rt_index_h
#define _rt_index_h

#define rt_PAGE_FIRST_KEY(page, nod_flag) (page + 2 + nod_flag)
#define rt_PAGE_NEXT_KEY(key, key_length, nod_flag) (key + key_length + \
              (nod_flag ? nod_flag : info->s->base.rec_reflength))
#define rt_PAGE_END(page) (page + mi_getint(page))

#define rt_PAGE_MIN_SIZE(block_length) ((uint)(block_length) / 2)

int rtree_insert(MI_INFO *info, uint keynr, uchar *key, uint key_length);
int rtree_delete(MI_INFO *info, uint keynr, uchar *key, uint key_length);

int rtree_find_first(MI_INFO *info, uint keynr, uchar *key, uint key_length, 
                    uint search_flag);
int rtree_find_next(MI_INFO *info, uint keynr, uint search_flag);

int rtree_get_first(MI_INFO *info, uint keynr, uint key_length);
int rtree_get_next(MI_INFO *info, uint keynr, uint key_length);

ha_rows rtree_estimate(MI_INFO *info, uint keynr, uchar *key, 
                       uint key_length, uint flag);

int rtree_split_page(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page, uchar *key, 
                    uint key_length, my_off_t *new_page_offs);

#endif /* _rt_index_h */
