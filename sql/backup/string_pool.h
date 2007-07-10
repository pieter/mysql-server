#ifndef _STRING_POOL_H
#define _STRING_POOL_H

#include "map.h"
#include "stream.h"

/**
  @file

  Implementation of a string pool which is a collection of
  String objects indexed by 8 bit keys.
 */

/*
  TODO

  - change memory allocation scheme
  - use mysql library functions
  - simplify by not using Map template
 */

namespace util {

/// A domain of String values which can by used by Map template.

struct StringDom
{
  typedef String Element;
  static Element &null;

  static Element *create(const Element &s);

  static int cmp(const Element &x, const Element &y)
  { return stringcmp(&x,&y); }

  static unsigned int hash(const Element&)
  { return 0; }
};

typedef Map<StringDom>  StringPool;

} // util namespace


namespace backup {

/**
  Adds serialization to util::StringPool class
 */
class StringPool: public util::StringPool
{
 public:
  result_t save(OStream&);
  result_t read(IStream&);
};

} // backup namespace

#endif
