#include "../mysql_priv.h"

#include "string_pool.h"

extern const String my_null_string;

namespace util {

String *StringDom::create(const Element &s)
{
  String *ns= new (::current_thd->mem_root) String();
  ns->copy(s);
  return ns;
}

String &StringDom::null= const_cast< String& >(::my_null_string);

// Map template instance used as StringPool implementation.
template class Map<StringDom>;

} // util namespace


namespace backup {

// Serialization of StringPool

/*
  Pool

  0: "foo"
  1: -
  2: -
  3: "bar"
  4: "baz"
  ...

  is saved as

  | foo | NIL : 2 | bar | baz | ...

  where the (NIL:2) entry describes a "hole" in the pool of width 2.

  Empty pool is saved as | NIL : 0 |.
*/

result_t StringPool::save(OStream &str)
{
  DBUG_ENTER("StringPool::save()");

  if (count() == 0) // empty pool
  {
    DBUG_PRINT("string_pool",("saving empty pool"));
    if (stream_result::ERROR == str.writenil()
        || stream_result::ERROR == str.writeint(0))
     DBUG_RETURN(ERROR);
  }
  else
  {
    String buf;
    uint  skip=0, i=0;

    DBUG_PRINT("string_pool",("saving pool of size %lu",(unsigned long)count()));

    for (i=0 ; TRUE ; ++i)
    {
      if (i >= count() || entries[i].el != NULL) // non-empty or last entry
      {
        /*
          If skip > 0 this entry closes a hole of skip entries.
          Write hole width to finish its description.
         */
        if (skip > 0)
        {
          DBUG_PRINT("string_pool",("noting hole of width %d",skip));
          if (stream_result::ERROR == str.writeint(skip))
          {
            DBUG_PRINT("string_pool",("error when writing hole width"));
            DBUG_RETURN(ERROR);
          }
        }

        if (i >= count())
          break;

        skip= 0; // the hole has ended now

        DBUG_PRINT("string_pool",("writing entry %s at position %d",
                                  (entries[i].el)->c_ptr(),i));
        // save the entry
        if (stream_result::ERROR == str.writestr(*entries[i].el))
        {
          DBUG_PRINT("string_pool",("error when writing string"));
          DBUG_RETURN(ERROR);
        }

        continue;
      }

      // we have an empty entry, if skip == 0 it starts a new hole which
      // is marked by NIL value

      if( skip == 0 )
        if (stream_result::ERROR == str.writenil())
          DBUG_RETURN(ERROR);

      skip++;
    }

  } // if (count()==0) else ...

  DBUG_PRINT("string_pool",("saved"));

  DBUG_RETURN(str.end_chunk() == stream_result::ERROR ? ERROR : OK);
}

/**
  @retval OK
  @retval DONE  end of stream/chunk hit
  @retval ERROR
 */
result_t StringPool::read(IStream &str)
{
  DBUG_ENTER("StringPool::read");

  stream_result::value res;
  String buf;
  uint  skip= 0, i=0;

  /*
    Read first entry. If it is NIL then we either have an empty pool
    or a pool which starts with a hole. This is decided by the number following
    NIL value.
   */

  res= str.readstr(buf);

  if (res != stream_result::OK && res != stream_result::NIL)
  {
    // In that case there is an error or we hit end of data
    if (res != stream_result::ERROR)
      DBUG_PRINT("string_pool",("End of data hit"));
    DBUG_RETURN(report_stream_result(res));
  }

  clear();

  if (res == stream_result::NIL)
  {
    res= str.readint(skip);

    if (res != stream_result::OK)
      DBUG_RETURN(ERROR);

    if (skip == 0) // this is empty pool - nothing more to read
    {
      DBUG_PRINT("string_pool",("Found empty pool"));
      goto finish;
    }

    DBUG_PRINT("sting_pool",("Starts with hole of width %d",skip));

    /*
      this is non-empty pool starting with a hole - read the following
      string
     */

    res= str.readstr(buf);

    if (res == stream_result::ERROR)
    {
      DBUG_PRINT("string",("Error reading string"));
      DBUG_RETURN(ERROR);
    }
  }

  /*
    If we are here, we have a non-empty pool and are going to populate it
    with strings read from the stream. The first entry is already read.
   */

  while (res == stream_result::OK)
  {
    /*
      If skip > 0 then we have a hole of that size preceding the next
      string entry.
     */
    if (skip > 0)
      i+= skip;

    skip= 0;

    if (i >= size) // Not enough space to store the string
    {
      DBUG_PRINT("string_pool",("Not enough space to store string at pos %d"
                                " (size=%lu)",i,(unsigned long)size));
      DBUG_RETURN(ERROR);
    }

    DBUG_PRINT("string_pool",("Adding string %s at position %d",buf.c_ptr(),i));
    // store the string and increase position
    set(i++,buf);

    // read next entry from stream
    res= str.readstr(buf);

    if (res == stream_result::NIL) // this is hole description
    {
      // read size of the hole
      res= str.readint(skip);
      DBUG_PRINT("string_pool",("Found hole of width %d",skip));

      if (res != stream_result::OK)
      {
        DBUG_PRINT("string_pool",("Found hole - error reading its width"));
        DBUG_RETURN(ERROR);
      }

      // read the following string
      res= str.readstr(buf);
    }
  }

  // Now we should be at the end of chunk
  if (res != stream_result::EOC && res != stream_result::EOS)
  {
    DBUG_PRINT("string_pool",("Error when reading strings (res=%d)",(int)res));
    DBUG_RETURN(ERROR);
  }

  DBUG_PRINT("string_pool",("Finished reading strings"));

 finish:

  res= str.next_chunk();

  DBUG_RETURN(res == stream_result::ERROR ? ERROR: OK);
}

} // backup namespace
