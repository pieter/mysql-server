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

/*
  Implementation of a bitmap type.
  The idea with this is to be able to handle any constant number of bits but
  also be able to use 32 or 64 bits bitmaps very efficiently
*/

#include <my_bitmap.h>

template <uint default_width> class Bitmap
{
  MY_BITMAP map;
  uchar buffer[(default_width+7)/8];
public:
  Bitmap() { init(); }
  Bitmap(Bitmap& from) { *this=from; }
  Bitmap(uint prefix_to_set) { init(prefix_to_set); }
  void init() { bitmap_init(&map, buffer, default_width, 0); }
  void init(uint prefix_to_set) { init(); set_prefix(prefix_to_set); }
  uint length() const { return default_width; }
  Bitmap& operator=(const Bitmap& map2)
  {
    init();
    memcpy(buffer, map2.buffer, sizeof(buffer));
    return *this;
  }
  void set_bit(uint n) { bitmap_set_bit(&map, n); }
  void clear_bit(uint n) { bitmap_clear_bit(&map, n); }
  void set_prefix(uint n) { bitmap_set_prefix(&map, n); }
  void set_all() { bitmap_set_all(&map); }
  void clear_all() { bitmap_clear_all(&map); }
  void intersect(Bitmap& map2) { bitmap_intersect(&map, &map2.map); }
  void intersect(ulonglong map2buff)
  {
    MY_BITMAP map2;
    bitmap_init(&map2, (uchar *)&map2buff, sizeof(ulonglong)*8, 0);
    bitmap_intersect(&map, &map2);
  }
  void subtract(Bitmap& map2) { bitmap_subtract(&map, &map2.map); }
  void merge(Bitmap& map2) { bitmap_union(&map, &map2.map); }
  my_bool is_set(uint n) const { return bitmap_is_set(&map, n); }
  my_bool is_prefix(uint n) const { return bitmap_is_prefix(&map, n); }
  my_bool is_clear_all() const { return bitmap_is_clear_all(&map); }
  my_bool is_set_all() const { return bitmap_is_set_all(&map); }
  my_bool is_subset(const Bitmap& map2) const { return bitmap_is_subset(&map, &map2.map); }
  my_bool operator==(const Bitmap& map2) const { return bitmap_cmp(&map, &map2.map); }
  char *print(char *buf) const
  {
    char *s=buf; int i;
    for (i=sizeof(buffer)-1; i>=0 ; i--)
    {
      if ((*s=_dig_vec[buffer[i] >> 4]) != '0')
        break;
      if ((*s=_dig_vec[buffer[i] & 15]) != '0')
        break;
    }
    for (s++, i-- ; i>=0 ; i--)
    {
      *s++=_dig_vec[buffer[i] >> 4];
      *s++=_dig_vec[buffer[i] & 15];
    }
    *s=0;
    return buf;
  }
  ulonglong to_ulonglong() const
  {
    if (sizeof(buffer) >= 8)
      return uint8korr(buffer);
    DBUG_ASSERT(sizeof(buffer) >= 4);
    uint4korr(buffer);
  }
};

template <> class Bitmap<64>
{
  ulonglong map;
public:
  Bitmap<64>() { }
  Bitmap<64>(uint prefix_to_set) { set_prefix(prefix_to_set); }
  void init() { }
  void init(uint prefix_to_set) { set_prefix(prefix_to_set); }
  uint length() const { return 64; }
  void set_bit(uint n) { map|= ((ulonglong)1) << n; }
  void clear_bit(uint n) { map&= ~(((ulonglong)1) << n); }
  void set_prefix(uint n)
  {
    if (n >= length())
      set_all();
    else
      map= (((ulonglong)1) << n)-1;
  }
  void set_all() { map=~(ulonglong)0; }
  void clear_all() { map=(ulonglong)0; }
  void intersect(Bitmap<64>& map2) { map&= map2.map; }
  void intersect(ulonglong map2) { map&= map2; }
  void subtract(Bitmap<64>& map2) { map&= ~map2.map; }
  void merge(Bitmap<64>& map2) { map|= map2.map; }
  my_bool is_set(uint n) const { return test(map & (((ulonglong)1) << n)); }
  my_bool is_prefix(uint n) const { return map == (((ulonglong)1) << n)-1; }
  my_bool is_clear_all() const { return map == (ulonglong)0; }
  my_bool is_set_all() const { return map == ~(ulonglong)0; }
  my_bool is_subset(const Bitmap<64>& map2) const { return !(map & ~map2.map); }
  my_bool operator==(const Bitmap<64>& map2) const { return map == map2.map; }
  char *print(char *buf) const { longlong2str(map,buf,16); return buf; }
  ulonglong to_ulonglong() const { return map; }
};

