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

#ifndef NDB_SQL_UTIL_HPP
#define NDB_SQL_UTIL_HPP

#include <string.h>
#include <ndb_types.h>

class NdbSqlUtil {
public:
  /**
   * Compare strings, optionally with padded semantics.  Returns
   * negative (less), zero (equal), or positive (greater).
   */
  static int char_compare(const char* s1, unsigned n1,
                          const char* s2, unsigned n2, bool padded);

  /**
   * Like operator, optionally with padded semantics.  Returns true or
   * false.
   */
  static bool char_like(const char* s1, unsigned n1,
                        const char* s2, unsigned n2, bool padded);

  /**
   * Compare kernel attribute values.  Returns -1, 0, +1 for less,
   * equal, greater, respectively.  Parameters are pointers to values,
   * full attribute size in words, and size of available data in words.
   * There are 2 special return values to check first.  All values fit
   * into a signed char.
   */
  typedef int Cmp(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size);

  enum CmpResult {
    CmpLess = -1,
    CmpEqual = 0,
    CmpGreater = 1,
    CmpUnknown = 126,   // insufficient partial data
    CmpError = 127      // bad data format or unimplemented comparison
  };

  /**
   * Kernel data types.  Must match m_typeList in NdbSqlUtil.cpp.
   */
  struct Type {
    enum Enum {
      Undefined = 0,    // Undefined 
      Tinyint,          // 8 bit
      Tinyunsigned,     // 8 bit
      Smallint,         // 16 bit
      Smallunsigned,    // 16 bit
      Mediumint,        // 24 bit
      Mediumunsigned,   // 24 bit
      Int,              // 32 bit
      Unsigned,         // 32 bit
      Bigint,           // 64 bit
      Bigunsigned,      // 64 Bit
      Float,            // 32-bit float
      Double,           // 64-bit float
      Decimal,          // Precision, Scale
      Char,             // Len
      Varchar,          // Max len
      Binary,           // Len
      Varbinary,        // Max len
      Datetime,         // Precision down to 1 sec  (size 8 bytes)
      Timespec          // Precision down to 1 nsec (size 12 bytes)
    };
    Enum m_typeId;
    Cmp* m_cmp;         // set to NULL if cmp not implemented
  };

  /**
   * Get type by id.  Can return the Undefined type.
   */
  static const Type& type(Uint32 typeId);

  /**
   * Inline comparison method.  Most or all real methods Type::m_cmp are
   * implemented via this (trusting dead code elimination).
   */
  static int cmp(Uint32 typeId, const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size);

private:
  /**
   * List of all types.  Must match Type::Enum.
   */
  static const Type m_typeList[];
  /**
   * Comparison methods.
   */
  static Cmp cmpTinyint;
  static Cmp cmpTinyunsigned;
  static Cmp cmpSmallint;
  static Cmp cmpSmallunsigned;
  static Cmp cmpMediumint;
  static Cmp cmpMediumunsigned;
  static Cmp cmpInt;
  static Cmp cmpUnsigned;
  static Cmp cmpBigint;
  static Cmp cmpBigunsigned;
  static Cmp cmpFloat;
  static Cmp cmpDouble;
  static Cmp cmpDecimal;
  static Cmp cmpChar;
  static Cmp cmpVarchar;
  static Cmp cmpBinary;
  static Cmp cmpVarbinary;
  static Cmp cmpDatetime;
  static Cmp cmpTimespec;
};

inline int
NdbSqlUtil::cmp(Uint32 typeId, const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  if (size > full)
    return CmpError;
  switch ((Type::Enum)typeId) {
  case Type::Undefined:
    break;
  case Type::Tinyint:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Int8 v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Tinyunsigned:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Uint8 v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Smallint:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Int16 v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Smallunsigned:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Uint16 v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Mediumint:         // XXX fix these
    break;
  case Type::Mediumunsigned:
    break;
  case Type::Int:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Int32 v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Unsigned:
    {
      if (size >= 1) {
        union { Uint32 p[1]; Uint32 v; } u1, u2;
        u1.v = p1[0];
        u2.v = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Bigint:
    {
      if (size >= 2) {
        union { Uint32 p[2]; Int64 v; } u1, u2;
        u1.p[0] = p1[0];
        u1.p[1] = p1[1];
        u2.p[0] = p2[0];
        u2.p[1] = p2[1];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Bigunsigned:
    {
      if (size >= 2) {
        union { Uint32 p[2]; Uint64 v; } u1, u2;
        u1.p[0] = p1[0];
        u1.p[1] = p1[1];
        u2.p[0] = p2[0];
        u2.p[1] = p2[1];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Float:
    {
      if (size >= 1) {
        union { Uint32 p[1]; float v; } u1, u2;
        u1.p[0] = p1[0];
        u2.p[0] = p2[0];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Double:
    {
      if (size >= 2) {
        union { Uint32 p[2]; double v; } u1, u2;
        u1.p[0] = p1[0];
        u1.p[1] = p1[1];
        u2.p[0] = p2[0];
        u2.p[1] = p2[1];
        if (u1.v < u2.v)
          return -1;
        if (u1.v > u2.v)
          return +1;
        return 0;
      }
      return CmpUnknown;
    }
    break;
  case Type::Decimal:
    break;
  case Type::Char:
    {
      /*
       * Char is blank-padded to length and null-padded to word size.
       * There is no terminator so we must compare the full values.
       */
      union { const Uint32* p; const char* v; } u1, u2;
      u1.p = p1;
      u2.p = p2;
      int k = memcmp(u1.v, u2.v, size << 2);
      return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
    }
    break;
  case Type::Varchar:
    {
      /*
       * Varchar is not allowed to contain a null byte and the stored
       * value is null-padded.  Therefore comparison does not need to
       * use the length.
       */
      if (size >= 1) {
        union { const Uint32* p; const char* v; } u1, u2;
        u1.p = p1;
        u2.p = p2;
        // length in first 2 bytes
        int k = strncmp(u1.v + 2, u2.v + 2, (size << 2) - 2);
        return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
      }
      return CmpUnknown;
    }
    break;
  case Type::Binary:            // XXX fix these
    break;
  case Type::Varbinary:
    break;
  case Type::Datetime:
    {
      /*
       * Datetime is CC YY MM DD hh mm ss \0
       */
      if (size >= 1) {
        union { const Uint32* p; const char* v; } u1, u2;
        u1.p = p1;
        u2.p = p2;
        // skip format check
        int k = strncmp(u1.v, u2.v, 4);
        if (k != 0)
          return k;
        if (size >= 2) {
          return strncmp(u1.v + 4, u2.v + 4, 4);
        }
      }
      return CmpUnknown;
    }
    break;
  case Type::Timespec:          // XXX fix this
    break;
  }
  return CmpError;
}

#endif
