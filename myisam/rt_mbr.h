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

#ifndef _rt_mbr_h
#define _rt_mbr_h

int rtree_key_cmp(HA_KEYSEG *keyseg, uchar *a, uchar *b, uint key_length,
                  uint nextflag);
int rtree_combine_rect(HA_KEYSEG *keyseg,uchar *, uchar *, uchar*, 
                       uint key_length);
double rtree_rect_volume(HA_KEYSEG *keyseg, uchar*, uint key_length);
int rtree_d_mbr(HA_KEYSEG *keyseg, uchar *a, uint key_length, double *res);
double rtree_overlapping_area(HA_KEYSEG *keyseg, uchar *a, uchar *b, 
                              uint key_length);
double rtree_area_increase(HA_KEYSEG *keyseg, uchar *a, uchar *b, 
                           uint key_length, double *ab_area);
int rtree_page_mbr(MI_INFO *info, HA_KEYSEG *keyseg, uchar *page_buf, 
                   uchar* c, uint key_length);
#endif /* _rt_mbr_h */
