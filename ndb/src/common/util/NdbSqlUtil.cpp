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

#include <NdbSqlUtil.hpp>

int
NdbSqlUtil::char_compare(const char* s1, unsigned n1,
                         const char* s2, unsigned n2, bool padded)
{
  int c1 = 0;
  int c2 = 0;
  unsigned i = 0;
  while (i < n1 || i < n2) {
    c1 = i < n1 ? s1[i] : padded ? 0x20 : 0;
    c2 = i < n2 ? s2[i] : padded ? 0x20 : 0;
    if (c1 != c2)
      break;
    i++;
  }
  return c1 - c2;
}

bool
NdbSqlUtil::char_like(const char* s1, unsigned n1,
                      const char* s2, unsigned n2, bool padded)
{
  int c1 = 0;
  int c2 = 0;
  unsigned i1 = 0;
  unsigned i2 = 0;
  while (i1 < n1 || i2 < n2) {
    c1 = i1 < n1 ? s1[i1] : padded ? 0x20 : 0;
    c2 = i2 < n2 ? s2[i2] : padded ? 0x20 : 0;
    if (c2 == '%') {
      while (i2 + 1 < n2 && s2[i2 + 1] == '%') {
        i2++;
      }
      unsigned m = 0;
      while (m <= n1 - i1) {
        if (char_like(s1 + i1 + m, n1 -i1 - m,
                      s2 + i2 + 1, n2 - i2 - 1, padded))
        return true;
        m++;
      }
      return false;
    }
    if (c2 == '_') {
      if (c1 == 0)
        return false;
    } else {
      if (c1 != c2)
        return false;
    }
    i1++;
    i2++;
  }
  return i1 == n2 && i2 == n2;
}

/*
 * Data types.  The entries must be in the numerical order.
 */

const NdbSqlUtil::Type
NdbSqlUtil::m_typeList[] = {
  {
    Type::Undefined,
    NULL
  },
  {
    Type::Tinyint,
    cmpTinyint
  },
  {
    Type::Tinyunsigned,
    cmpTinyunsigned
  },
  {
    Type::Smallint,
    cmpSmallint
  },
  {
    Type::Smallunsigned,
    cmpSmallunsigned
  },
  {
    Type::Mediumint,
    cmpMediumint
  },
  {
    Type::Mediumunsigned,
    cmpMediumunsigned
  },
  {
    Type::Int,
    cmpInt
  },
  {
    Type::Unsigned,
    cmpUnsigned
  },
  {
    Type::Bigint,
    cmpBigint
  },
  {
    Type::Bigunsigned,
    cmpBigunsigned
  },
  {
    Type::Float,
    cmpFloat
  },
  {
    Type::Double,
    cmpDouble
  },
  {
    Type::Decimal,
    NULL  // cmpDecimal
  },
  {
    Type::Char,
    cmpChar
  },
  {
    Type::Varchar,
    cmpVarchar
  },
  {
    Type::Binary,
    cmpBinary
  },
  {
    Type::Varbinary,
    cmpVarbinary
  },
  {
    Type::Datetime,
    cmpDatetime
  },
  {
    Type::Date,
    cmpDate
  },
  {
    Type::Blob,
    NULL  // cmpBlob
  },
  {
    Type::Text,
    NULL  // cmpText
  },
  {
    Type::Bit,
    NULL  // cmpBit
  },
  {
    Type::Longvarchar,
    cmpLongvarchar
  },
  {
    Type::Longvarbinary,
    cmpLongvarbinary
  },
  {
    Type::Time,
    cmpTime
  }
};

const NdbSqlUtil::Type&
NdbSqlUtil::getType(Uint32 typeId)
{
  if (typeId < sizeof(m_typeList) / sizeof(m_typeList[0]) &&
      m_typeList[typeId].m_typeId != Type::Undefined) {
    return m_typeList[typeId];
  }
  return m_typeList[Type::Undefined];
}

const NdbSqlUtil::Type&
NdbSqlUtil::getTypeBinary(Uint32 typeId)
{
  switch (typeId) {
  case Type::Char:
  case Type::Varchar:
  case Type::Binary:
  case Type::Varbinary:
  case Type::Longvarchar:
  case Type::Longvarbinary:
    typeId = Type::Binary;
    break;
  case Type::Text:
    typeId = Type::Blob;
    break;
  default:
    break;
  }
  return getType(typeId);
}

/*
 * Comparison functions.
 */

int
NdbSqlUtil::cmpTinyint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Int8)) {
    Int8 v1, v2;
    memcpy(&v1, p1, sizeof(Int8));
    memcpy(&v2, p2, sizeof(Int8));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpTinyunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Uint8)) {
    Uint8 v1, v2;
    memcpy(&v1, p1, sizeof(Uint8));
    memcpy(&v2, p2, sizeof(Uint8));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpSmallint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Int16)) {
    Int16 v1, v2;
    memcpy(&v1, p1, sizeof(Int16));
    memcpy(&v2, p2, sizeof(Int16));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpSmallunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Uint16)) {
    Uint16 v1, v2;
    memcpy(&v1, p1, sizeof(Uint16));
    memcpy(&v2, p2, sizeof(Uint16));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpMediumint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= 3) {
    Int32 v1, v2;
    v1 = sint3korr((const uchar*)p1);
    v2 = sint3korr((const uchar*)p2);
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpMediumunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= 3) {
    Uint32 v1, v2;
    v1 = uint3korr((const uchar*)p1);
    v2 = uint3korr((const uchar*)p2);
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpInt(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Int32)) {
    Int32 v1, v2;
    memcpy(&v1, p1, sizeof(Int32));
    memcpy(&v2, p2, sizeof(Int32));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpUnsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Uint32)) {
    Uint32 v1, v2;
    memcpy(&v1, p1, sizeof(Uint32));
    memcpy(&v2, p2, sizeof(Uint32));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpBigint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Int64)) {
    Int64 v1, v2;
    memcpy(&v1, p1, sizeof(Int64));
    memcpy(&v2, p2, sizeof(Int64));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpBigunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Uint64)) {
    Uint64 v1, v2;
    memcpy(&v1, p1, sizeof(Uint64));
    memcpy(&v2, p2, sizeof(Uint64));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpFloat(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(float)) {
    float v1, v2;
    memcpy(&v1, p1, sizeof(float));
    memcpy(&v2, p2, sizeof(float));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpDouble(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(double)) {
    double v1, v2;
    memcpy(&v1, p1, sizeof(double));
    memcpy(&v2, p2, sizeof(double));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

// not used by MySQL or NDB
int
NdbSqlUtil::cmpDecimal(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  assert(false);
  return 0;
}

int
NdbSqlUtil::cmpChar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  // collation does not work on prefix for some charsets
  assert(full);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  // not const in MySQL
  CHARSET_INFO* cs = (CHARSET_INFO*)(info);
  // compare with space padding
  int k = (*cs->coll->strnncollsp)(cs, v1, n1, v2, n2, false);
  return k < 0 ? -1 : k > 0 ? +1 : 0;
}

int
NdbSqlUtil::cmpVarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  const unsigned lb = 1;
  // collation does not work on prefix for some charsets
  assert(full && n1 >= lb && n2 >= lb);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  unsigned m1 = *v1;
  unsigned m2 = *v2;
  if (m1 <= n1 - lb && m2 <= n2 - lb) {
    CHARSET_INFO* cs = (CHARSET_INFO*)(info);
    // compare with space padding
    int k = (*cs->coll->strnncollsp)(cs, v1 + lb, m1, v2 + lb, m2, false);
    return k < 0 ? -1 : k > 0 ? +1 : 0;
  }
  // treat bad data as NULL
  if (m1 > n1 - lb && m2 <= n2 - lb)
    return -1;
  if (m1 <= n1 - lb && m2 > n2 - lb)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpBinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  // compare as binary strings
  unsigned n = (n1 <= n2 ? n1 : n2);
  int k = memcmp(v1, v2, n);
  if (k == 0) {
    k = (full ? n1 : n) - n2;
  }
  return k < 0 ? -1 : k > 0 ? +1 : full ? 0 : CmpUnknown;
}

int
NdbSqlUtil::cmpVarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  const unsigned lb = 1;
  if (n2 >= lb) {
    assert(n1 >= lb);
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    unsigned m1 = *v1;
    unsigned m2 = *v2;
    if (m1 <= n1 - lb && m2 <= n2 - lb) {
      // compare as binary strings
      unsigned m = (m1 <= m2 ? m1 : m2);
      int k = memcmp(v1 + lb, v2 + lb, m);
      if (k == 0) {
        k = (full ? m1 : m) - m2;
      }
      return k < 0 ? -1 : k > 0 ? +1 : full ? 0 : CmpUnknown;
    }
    // treat bad data as NULL
    if (m1 > n1 - lb && m2 <= n2 - lb)
      return -1;
    if (m1 <= n1 - lb && m2 > n2 - lb)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpDatetime(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= sizeof(Int64)) {
    Int64 v1, v2;
    memcpy(&v1, p1, sizeof(Int64));
    memcpy(&v2, p2, sizeof(Int64));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

int
NdbSqlUtil::cmpDate(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
#ifdef ndb_date_is_4_byte_native_int
  if (n2 >= sizeof(Int32)) {
    Int32 v1, v2;
    memcpy(&v1, p1, sizeof(Int32));
    memcpy(&v2, p2, sizeof(Int32));
    if (v1 < v2)
      return -1;
    if (v1 > v2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
#else
  char t1[4], t2[4];
  if (n1 == 3 && n2 == 3)
  {
    memcpy(t1, p1, 3);
    memcpy(t2, p2, 3);
    p1 = t1;
    p2 = t2;
    n1 = n2 = 4;
  }
  if (n2 >= 4) {        // may access 4-th byte
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    // from Field_newdate::val_int
    Uint64 j1 = uint3korr(v1);
    Uint64 j2 = uint3korr(v2);
    j1 = (j1 % 32L)+(j1 / 32L % 16L)*100L + (j1/(16L*32L))*10000L;
    j2 = (j2 % 32L)+(j2 / 32L % 16L)*100L + (j2/(16L*32L))*10000L;
    if (j1 < j2)
      return -1;
    if (j1 > j2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
#endif
}

// not supported
int
NdbSqlUtil::cmpBlob(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  assert(false);
  return 0;
}

// not supported
int
NdbSqlUtil::cmpText(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  assert(false);
  return 0;
}

int
NdbSqlUtil::cmpTime(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  if (n2 >= 4) {        // may access 4-th byte
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    // from Field_time::val_int
    Int32 j1 = sint3korr(v1);
    Int32 j2 = sint3korr(v2);
    if (j1 < j2)
      return -1;
    if (j1 > j2)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

// not yet
int
NdbSqlUtil::cmpBit(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  assert(false);
  return 0;
}

int
NdbSqlUtil::cmpLongvarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  const unsigned lb = 2;
  // collation does not work on prefix for some charsets
  assert(full && n1 >= lb && n2 >= lb);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  unsigned m1 = uint2korr(v1);
  unsigned m2 = uint2korr(v2);
  if (m1 <= n1 - lb && m2 <= n2 - lb) {
    CHARSET_INFO* cs = (CHARSET_INFO*)(info);
    // compare with space padding
    int k = (*cs->coll->strnncollsp)(cs, v1 + lb, m1, v2 + lb, m2, false);
    return k < 0 ? -1 : k > 0 ? +1 : 0;
  }
  // treat bad data as NULL
  if (m1 > n1 - lb && m2 <= n2 - lb)
    return -1;
  if (m1 <= n1 - lb && m2 > n2 - lb)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpLongvarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full)
{
  const unsigned lb = 2;
  if (n2 >= lb) {
    assert(n1 >= lb);
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    unsigned m1 = uint2korr(v1);
    unsigned m2 = uint2korr(v2);
    if (m1 <= n1 - lb && m2 <= n2 - lb) {
      // compare as binary strings
      unsigned m = (m1 <= m2 ? m1 : m2);
      int k = memcmp(v1 + lb, v2 + lb, m);
      if (k == 0) {
        k = (full ? m1 : m) - m2;
      }
      return k < 0 ? -1 : k > 0 ? +1 : full ? 0 : CmpUnknown;
    }
    // treat bad data as NULL
    if (m1 > n1 - lb && m2 <= n2 - lb)
      return -1;
    if (m1 <= n1 - lb && m2 > n2 - lb)
      return +1;
    return 0;
  }
  assert(! full);
  return CmpUnknown;
}

// check charset

bool
NdbSqlUtil::usable_in_pk(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  switch (type.m_typeId) {
  case Type::Char:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      return
        cs != 0 &&
        cs->cset != 0 &&
        cs->coll != 0 &&
        cs->coll->strnxfrm != 0 &&
        cs->strxfrm_multiply <= MAX_XFRM_MULTIPLY;
    }
    break;
  case Type::Undefined:
  case Type::Blob:
  case Type::Text:
    break;
  default:
    return true;
  }
  return false;
}

bool
NdbSqlUtil::usable_in_hash_index(Uint32 typeId, const void* info)
{
  return usable_in_pk(typeId, info);
}

bool
NdbSqlUtil::usable_in_ordered_index(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  if (type.m_cmp == NULL)
    return false;
  switch (type.m_typeId) {
  case Type::Char:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      return
        cs != 0 &&
        cs->cset != 0 &&
        cs->coll != 0 &&
        cs->coll->strnxfrm != 0 &&
        cs->coll->strnncollsp != 0 &&
        cs->strxfrm_multiply <= MAX_XFRM_MULTIPLY;
    }
    break;
  case Type::Undefined:
  case Type::Blob:
  case Type::Text:
    break;
  default:
    return true;
  }
  return false;
}

// utilities

bool
NdbSqlUtil::get_var_length(Uint32 typeId, const void* p, unsigned attrlen, Uint32& lb, Uint32& len)
{
  const unsigned char* const src = (const unsigned char*)p;
  switch (typeId) {
  case NdbSqlUtil::Type::Varchar:
  case NdbSqlUtil::Type::Varbinary:
    lb = 1;
    if (attrlen >= lb) {
      len = src[0];
      if (attrlen >= lb + len)
        return true;
    }
    break;
  case NdbSqlUtil::Type::Longvarchar:
  case NdbSqlUtil::Type::Longvarbinary:
    lb = 2;
    if (attrlen >= lb) {
      len = src[0] + (src[1] << 8);
      if (attrlen >= lb + len)
        return true;
    }
    break;
  default:
    lb = 0;
    len = attrlen;
    return true;
    break;
  }
  return false;
}

// workaround

int
NdbSqlUtil::strnxfrm_bug7284(CHARSET_INFO* cs, unsigned char* dst, unsigned dstLen, const unsigned char*src, unsigned srcLen)
{
  unsigned char nsp[20]; // native space char
  unsigned char xsp[20]; // strxfrm-ed space char
#ifdef VM_TRACE
  memset(nsp, 0x1f, sizeof(nsp));
  memset(xsp, 0x1f, sizeof(xsp));
#endif
  // convert from unicode codepoint for space
  int n1 = (*cs->cset->wc_mb)(cs, (my_wc_t)0x20, nsp, nsp + sizeof(nsp));
  if (n1 <= 0)
    return -1;
  // strxfrm to binary
  int n2 = (*cs->coll->strnxfrm)(cs, xsp, sizeof(xsp), nsp, n1);
  if (n2 <= 0)
    return -1;
  // strxfrm argument string - returns no error indication
  int n3 = (*cs->coll->strnxfrm)(cs, dst, dstLen, src, srcLen);
  // pad with strxfrm-ed space chars
  int n4 = n3;
  while (n4 < (int)dstLen) {
    dst[n4] = xsp[(n4 - n3) % n2];
    n4++;
  }
  // no check for partial last
  return dstLen;
}
