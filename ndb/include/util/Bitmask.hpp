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

#ifndef NDB_BITMASK_H
#define NDB_BITMASK_H

#include <ndb_global.h>

#ifndef NDB_ASSERT
#define NDB_ASSERT(x, s) \
  do { if (!(x)) { printf("%s\n", s); abort(); } } while (0)
#endif

/**
 * Bitmask implementation.  Size is given explicitly
 * (as first argument).  All methods are static.
 */
class BitmaskImpl {
public:
  STATIC_CONST( NotFound = (unsigned)-1 );

  /**
   * get - Check if bit n is set.
   */
  static bool get(unsigned size, const Uint32 data[], unsigned n);

  /**
   * set - Set bit n to given value (true/false).
   */
  static void set(unsigned size, Uint32 data[], unsigned n, bool value);

  /**
   * set - Set bit n.
   */
  static void set(unsigned size, Uint32 data[], unsigned n);

  /**
   * set - Set all bits.
   */
  static void set(unsigned size, Uint32 data[]);

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src/<em>
   */
  static void assign(unsigned size, Uint32 dst[], const Uint32 src[]);

  /**
   * clear - Clear bit n.
   */
  static void clear(unsigned size, Uint32 data[], unsigned n);

  /**
   * clear - Clear all bits.
   */
  static void clear(unsigned size, Uint32 data[]);

  /**
   * isclear -  Check if all bits are clear.  This is faster
   * than checking count() == 0.
   */
  static bool isclear(unsigned size, const Uint32 data[]);

  /**
   * count - Count number of set bits.
   */
  static unsigned count(unsigned size, const Uint32 data[]);

  /**
   * find - Find first set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find(unsigned size, const Uint32 data[], unsigned n);

  /**
   * equal - Bitwise equal.
   */
  static bool equal(unsigned size, const Uint32 data[], const Uint32 data2[]);

  /**
   * bitOR - Bitwise (x | y) into first operand.
   */
  static void bitOR(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitAND - Bitwise (x & y) into first operand.
   */
  static void bitAND(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitANDC - Bitwise (x & ~y) into first operand.
   */
  static void bitANDC(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitXOR - Bitwise (x ^ y) into first operand.
   */
  static void bitXOR(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * contains - Check if all bits set in data2 are set in data
   */
  static bool contains(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * overlaps - Check if any bit set in data is set in data2
   */
  static bool overlaps(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * getField - Get bitfield at given position and length (max 32 bits)
   */
  static Uint32 getField(unsigned size, const Uint32 data[],
      unsigned pos, unsigned len);

  /**
   * setField - Set bitfield at given position and length (max 32 bits)
   */
  static void setField(unsigned size, Uint32 data[],
      unsigned pos, unsigned len, Uint32 val);

  /**
   * getText - Return as hex-digits (only for debug routines).
   */
  static char* getText(unsigned size, const Uint32 data[], char* buf);
};

inline bool
BitmaskImpl::get(unsigned size, const Uint32 data[], unsigned n)
{
  NDB_ASSERT(n < (size << 5), "bit get out of range");
  return (data[n >> 5] & (1 << (n & 31))) != 0;
}

inline void
BitmaskImpl::set(unsigned size, Uint32 data[], unsigned n, bool value)
{
  value ? set(size, data, n) : clear(size, data, n);
}

inline void
BitmaskImpl::set(unsigned size, Uint32 data[], unsigned n)
{
  NDB_ASSERT(n < (size << 5), "bit set out of range");
  data[n >> 5] |= (1 << (n & 31));
}

inline void
BitmaskImpl::set(unsigned size, Uint32 data[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] = ~0;
  }
}

inline void 
BitmaskImpl::assign(unsigned size, Uint32 dst[], const Uint32 src[])
{
  for (unsigned i = 0; i < size; i++) {
    dst[i] = src[i];
  }
}

inline void
BitmaskImpl::clear(unsigned size, Uint32 data[], unsigned n)
{
  NDB_ASSERT(n < (size << 5), "bit clear out of range");
  data[n >> 5] &= ~(1 << (n & 31));
}

inline void
BitmaskImpl::clear(unsigned size, Uint32 data[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] = 0;
  }
}

inline bool
BitmaskImpl::isclear(unsigned size, const Uint32 data[])
{
  for (unsigned i = 0; i < size; i++) {
    if (data[i] != 0)
      return false;
  }
  return true;
}

inline unsigned
BitmaskImpl::count(unsigned size, const Uint32 data[])
{
  unsigned cnt = 0;
  for (unsigned i = 0; i < size; i++) {
    Uint32 x = data[i];
    while (x) {
      x &= (x - 1);
      cnt++;
    }
  }
  return cnt;
}

inline unsigned
BitmaskImpl::find(unsigned size, const Uint32 data[], unsigned n)
{
  while (n < (size << 5)) {             // XXX make this smarter
    if (get(size, data, n)) {
      return n;
    }
    n++;
  }
  return NotFound;
}

inline bool
BitmaskImpl::equal(unsigned size, const Uint32 data[], const Uint32 data2[])
{
  for (unsigned i = 0; i < size; i++) {
    if (data[i] != data2[i])
      return false;
  }
  return true;
}

inline void
BitmaskImpl::bitOR(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] |= data2[i];
  }
}

inline void
BitmaskImpl::bitAND(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] &= data2[i];
  }
}

inline void
BitmaskImpl::bitANDC(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] &= ~data2[i];
  }
}

inline void
BitmaskImpl::bitXOR(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned i = 0; i < size; i++) {
    data[i] ^= data2[i];
  }
}

inline bool
BitmaskImpl::contains(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned int i = 0; i < size; i++)
    if ((data[i] & data2[i]) != data2[i])
      return false;
  return true;
}

inline bool
BitmaskImpl::overlaps(unsigned size, Uint32 data[], const Uint32 data2[])
{
  for (unsigned int i = 0; i < size; i++)
    if ((data[i] & data2[i]) != 0)
      return true;
  return false;
}

inline Uint32
BitmaskImpl::getField(unsigned size, const Uint32 data[],
    unsigned pos, unsigned len)
{
  Uint32 val = 0;
  for (unsigned i = 0; i < len; i++)
    val |= get(size, data, pos + i) << i;
  return val;
}

inline void
BitmaskImpl::setField(unsigned size, Uint32 data[],
    unsigned pos, unsigned len, Uint32 val)
{
  for (unsigned i = 0; i < len; i++)
    set(size, data, pos + i, val & (1 << i));
}

inline char *
BitmaskImpl::getText(unsigned size, const Uint32 data[], char* buf)
{
  char * org = buf;
  const char* const hex = "0123456789abcdef";
  for (int i = (size-1); i >= 0; i--) {
    Uint32 x = data[i];
    for (unsigned j = 0; j < 8; j++) {
      buf[7-j] = hex[x & 0xf];
      x >>= 4;
    }
    buf += 8;
  }
  *buf = 0;
  return org;
}

/**
 * Bitmasks.  The size is number of 32-bit words (Uint32).
 * Unused bits in the last word must be zero.
 *
 * XXX replace size by length in bits
 */
template <unsigned size>
struct BitmaskPOD {
public:
  /**
   * POD data representation
   */
  struct Data {
    Uint32 data[size];
#if 0
    Data & operator=(const BitmaskPOD<size> & src) {
      src.copyto(size, data);
      return *this;
    }
#endif
  };
private:
  
  Data rep;
public:
  STATIC_CONST( Size = size );
  STATIC_CONST( NotFound = BitmaskImpl::NotFound );
  STATIC_CONST( TextLength = size * 8 );

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src/<em>
   */
  void assign(const typename BitmaskPOD<size>::Data & src);

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src/<em>
   */
  static void assign(Uint32 dst[], const Uint32 src[]);
  static void assign(Uint32 dst[], const BitmaskPOD<size> & src);
  void assign(const BitmaskPOD<size> & src);

  /**
   * copy this to <em>dst</em>
   */
  void copyto(unsigned sz, Uint32 dst[]) const;
  
  /**
   * assign <em>this</em> according to <em>src/em>
   */
  void assign(unsigned sz, const Uint32 src[]);

  /**
   * get - Check if bit n is set.
   */
  static bool get(const Uint32 data[], unsigned n);
  bool get(unsigned n) const;

  /**
   * set - Set bit n to given value (true/false).
   */
  static void set(Uint32 data[], unsigned n, bool value);
  void set(unsigned n, bool value);

  /**
   * set - Set bit n.
   */
  static void set(Uint32 data[], unsigned n);
  void set(unsigned n);

  /**
   * set - set all bits.
   */
  static void set(Uint32 data[]);
  void set();

  /**
   * clear - Clear bit n.
   */
  static void clear(Uint32 data[], unsigned n);
  void clear(unsigned n);

  /**
   * clear - Clear all bits.
   */
  static void clear(Uint32 data[]);
  void clear();

  /**
   * isclear -  Check if all bits are clear.  This is faster
   * than checking count() == 0.
   */
  static bool isclear(const Uint32 data[]);
  bool isclear() const;

  /**
   * count - Count number of set bits.
   */
  static unsigned count(const Uint32 data[]);
  unsigned count() const;

  /**
   * find - Find first set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find(const Uint32 data[], unsigned n);
  unsigned find(unsigned n) const;

  /**
   * equal - Bitwise equal.
   */
  static bool equal(const Uint32 data[], const Uint32 data2[]);
  bool equal(const BitmaskPOD<size>& mask2) const;

  /**
   * bitOR - Bitwise (x | y) into first operand.
   */
  static void bitOR(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size>& bitOR(const BitmaskPOD<size>& mask2);

  /**
   * bitAND - Bitwise (x & y) into first operand.
   */
  static void bitAND(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size>& bitAND(const BitmaskPOD<size>& mask2);

  /**
   * bitANDC - Bitwise (x & ~y) into first operand.
   */
  static void bitANDC(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size>& bitANDC(const BitmaskPOD<size>& mask2);

  /**
   * bitXOR - Bitwise (x ^ y) into first operand.
   */
  static void bitXOR(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size>& bitXOR(const BitmaskPOD<size>& mask2);

  /**
   * contains - Check if all bits set in data2 (that) are also set in data (this)
   */
  static bool contains(Uint32 data[], const Uint32 data2[]);
  bool contains(BitmaskPOD<size> that);

  /**
   * overlaps - Check if any bit set in this BitmaskPOD (data) is also set in that (data2)
   */
  static bool overlaps(Uint32 data[], const Uint32 data2[]);
  bool overlaps(BitmaskPOD<size> that);

  /**
   * getText - Return as hex-digits (only for debug routines).
   */
  static char* getText(const Uint32 data[], char* buf);
  char* getText(char* buf) const;
};

template <unsigned size>
inline void
BitmaskPOD<size>::assign(Uint32 dst[], const Uint32 src[])
{
  BitmaskImpl::assign(size, dst, src);
}

template <unsigned size>
inline void
BitmaskPOD<size>::assign(Uint32 dst[], const BitmaskPOD<size> & src)
{
  BitmaskImpl::assign(size, dst, src.rep.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::assign(const typename BitmaskPOD<size>::Data & src)
{
  assign(rep.data, src.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::assign(const BitmaskPOD<size> & src)
{
  assign(rep.data, src.rep.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::copyto(unsigned sz, Uint32 dst[]) const
{
  BitmaskImpl::assign(sz, dst, rep.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::assign(unsigned sz, const Uint32 src[])
{
  BitmaskImpl::assign(sz, rep.data, src);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::get(const Uint32 data[], unsigned n)
{
  return BitmaskImpl::get(size, data, n);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::get(unsigned n) const
{
  return get(rep.data, n);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set(Uint32 data[], unsigned n, bool value)
{
  BitmaskImpl::set(size, data, n, value);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set(unsigned n, bool value)
{
  set(rep.data, n, value);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set(Uint32 data[], unsigned n)
{
  BitmaskImpl::set(size, data, n);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set(unsigned n)
{
  set(rep.data, n);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set(Uint32 data[])
{
  BitmaskImpl::set(size, data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::set()
{
  set(rep.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::clear(Uint32 data[], unsigned n)
{
  BitmaskImpl::clear(size, data, n);
}

template <unsigned size>
inline void
BitmaskPOD<size>::clear(unsigned n)
{
  clear(rep.data, n);
}

template <unsigned size>
inline void
BitmaskPOD<size>::clear(Uint32 data[])
{
  BitmaskImpl::clear(size, data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::clear()
{
  clear(rep.data);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::isclear(const Uint32 data[])
{
  return BitmaskImpl::isclear(size, data);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::isclear() const
{
  return isclear(rep.data);
}

template <unsigned size>
unsigned
BitmaskPOD<size>::count(const Uint32 data[])
{
  return BitmaskImpl::count(size, data);
}

template <unsigned size>
inline unsigned
BitmaskPOD<size>::count() const
{
  return count(rep.data);
}

template <unsigned size>
unsigned
BitmaskPOD<size>::find(const Uint32 data[], unsigned n)
{
  return BitmaskImpl::find(size, data, n);
}

template <unsigned size>
inline unsigned
BitmaskPOD<size>::find(unsigned n) const
{
  return find(rep.data, n);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::equal(const Uint32 data[], const Uint32 data2[])
{
  return BitmaskImpl::equal(size, data, data2);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::equal(const BitmaskPOD<size>& mask2) const
{
  return equal(rep.data, mask2.rep.data);
}

template <unsigned size>
inline void
BitmaskPOD<size>::bitOR(Uint32 data[], const Uint32 data2[])
{
  BitmaskImpl::bitOR(size,data, data2);
}

template <unsigned size>
inline BitmaskPOD<size>&
BitmaskPOD<size>::bitOR(const BitmaskPOD<size>& mask2)
{
  bitOR(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void
BitmaskPOD<size>::bitAND(Uint32 data[], const Uint32 data2[])
{
  BitmaskImpl::bitAND(size,data, data2);
}

template <unsigned size>
inline BitmaskPOD<size>&
BitmaskPOD<size>::bitAND(const BitmaskPOD<size>& mask2)
{
  bitAND(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void
BitmaskPOD<size>::bitANDC(Uint32 data[], const Uint32 data2[])
{
  BitmaskImpl::bitANDC(size,data, data2);
}

template <unsigned size>
inline BitmaskPOD<size>&
BitmaskPOD<size>::bitANDC(const BitmaskPOD<size>& mask2)
{
  bitANDC(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void
BitmaskPOD<size>::bitXOR(Uint32 data[], const Uint32 data2[])
{
  BitmaskImpl::bitXOR(size,data, data2);
}

template <unsigned size>
inline BitmaskPOD<size>&
BitmaskPOD<size>::bitXOR(const BitmaskPOD<size>& mask2)
{
  bitXOR(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
char *
BitmaskPOD<size>::getText(const Uint32 data[], char* buf)
{
  return BitmaskImpl::getText(size, data, buf);
}

template <unsigned size>
inline char *
BitmaskPOD<size>::getText(char* buf) const
{
  return getText(rep.data, buf);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::contains(Uint32 data[], const Uint32 data2[])
{
  return BitmaskImpl::contains(size, data, data2);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::contains(BitmaskPOD<size> that)
{
  return contains(this->rep.data, that.rep.data);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::overlaps(Uint32 data[], const Uint32 data2[])
{
  return BitmaskImpl::overlaps(size, data, data2);
}

template <unsigned size>
inline bool
BitmaskPOD<size>::overlaps(BitmaskPOD<size> that)
{
  return overlaps(this->rep.data, that.rep.data);
}

template <unsigned size>
class Bitmask : public BitmaskPOD<size> {
public:
  Bitmask() { this->clear();}
};

#endif
