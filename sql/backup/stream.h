#ifndef _BACKUP_STREAM_H_
#define _BACKUP_STREAM_H_

#include <backup/api_types.h>    // for Buffer definition
#include <backup/debug.h>        // for definition of DBUG_BACKUP

/**
  @file

 Generic Stream Interface for serializing basic types (integers, strings etc).
 */
 
namespace backup {

// TODO: remove the upper limit on chunk size

const size_t io_buffer_size= 1024*1024;   // buffer size for kernel/driver data transfers
const size_t max_chunk_size= 2*1024*1024; // upper limit on a single chunk size

}
 

namespace util
{

typedef unsigned char byte;

struct stream_result
{
  enum value { OK=0, NIL, ERROR };
};


/*
  Stream window: an abstract object which can be used to access a stream
  of bytes through a window in process address space. The methods give pointer
  to the beginning of the window and its current size. There is a request for
  enlarging the window. We can also move the window beginning to the right
  (decreasing window size). If some bytes move outside the window, they can't
  be accessed any more.

  ================================================> (stream of bytes)

             |--------------------|
             ^
             |
             window header


  1. Extend window:

             |--------------------............|
             ^
             |
             window header

  2. Move window header (shrinks window):

             .....|---------------|
                  ^
                  |
                  window header

   Interface
   ---------

   typename Result;

   byte*  head() const;       // pointer to start of the window
   byte*  end() const;        // pointer to the end of the window
   size_t length() const;     // current length of the window (in bytes)
   Result set_length(const size_t size); // extend the window to have (at least) given length
   Result move(const off_t offset);      // move window header
   //void  init();
   //void  done();

   operator int(const Result&); // interpret the result: is it error or other
                                   common situation (end of data).

   Note: window can be used for reading or writing.
*/


/**
   Stream classes.

   These classes provide interface for serializing basic data
   (numbers, strings) using host independent format.

   Interface is parametrised by a class implementing stream window.
   Instance of the window class is used to read/write stream data.
 */

template<class SWin>
class IStream
{
  SWin  *m_win;

 public:

  typedef typename SWin::Result Result;

  IStream(): m_win(NULL)
  {}
  
  // Note: we need to set m_win after instance has been created.
  void set_win(SWin *win)
  {
    m_win= win;
  }

  Result readbyte(byte &x);
  Result read2int(uint &x);
  Result read4int(ulong &x);
  Result readint(ulong &x);
  Result readint(uint &x)
  {
    ulong y= 0;
    Result res= readint(y);
    x= y;
    return res;
  }

  Result readstr(String &s);
};


template<class SWin>
class OStream
{
  SWin *m_win;

 public:

  typedef typename SWin::Result Result;

  OStream(): m_win(NULL)
  {}

  void set_win(SWin *win)
  {
    m_win= win;
  }

  Result writebyte(const byte x);
  Result write2int(const int x);
  Result write4int(const ulong x);
  Result writeint(const ulong x);
  Result writestr(const String &s);
  
  Result writestr(const char *s)
  { return writestr(String(s,table_alias_charset)); }

  Result writenil()
  { return writebyte(251); }
};

template<class SW>
inline
typename IStream<SW>::Result
IStream<SW>::readbyte(byte &x)
{
  Result res;
  
  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(1)) != Result(stream_result::OK))
    return res;

  x= *m_win->head();

  return m_win->move(1);
}

template<class SW>
inline
typename OStream<SW>::Result
OStream<SW>::writebyte(const byte x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(1)) != Result(stream_result::OK))
    return res;

  (*m_win->head())= x;

  return m_win->move(1);
}

template<class SW>
inline
typename IStream<SW>::Result
IStream<SW>::read2int(uint &x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(2)) != Result(stream_result::OK))
   return res;

  x= uint2korr(m_win->head());

  return m_win->move(2);
}

template<class SW>
inline
typename OStream<SW>::Result
OStream<SW>::write2int(const int x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(2)) != Result(stream_result::OK))
    return res;

  int2store(m_win->head(),x);

  return m_win->move(2);
}


template<class SW>
inline
typename IStream<SW>::Result
IStream<SW>::read4int(ulong &x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(4)) != Result(stream_result::OK))
   return res;

  x= uint4korr(m_win->head());

  return m_win->move(4);
}

template<class SW>
inline
typename OStream<SW>::Result
OStream<SW>::write4int(const ulong x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(4)) != Result(stream_result::OK))
    return res;

  int4store(m_win->head(),x);

  return m_win->move(4);
}

// write/read number using variable-length encoding

template<class SW>
inline
typename IStream<SW>::Result
IStream<SW>::readint(ulong &x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(1)) != Result(stream_result::OK))
    return res;

  x= *m_win->head();
  m_win->move(1);

  switch( x ) {
  case 251:
    return Result(stream_result::NIL);

  case 252:
  {
    uint y= 0;
    res= read2int(y);
    x= y;
    return res;
  }
  
  case 253:
    return read4int(x);

  default:
    return Result(stream_result::OK);
  }
}

template<class SW>
inline
typename OStream<SW>::Result
OStream<SW>::writeint(const ulong x)
{
  Result res;

  DBUG_ASSERT(m_win);

  if ((res= m_win->set_length(1)) != Result(stream_result::OK))
    return res;

  if (x < 251)
    return writebyte((byte)x);

  if (x < (1UL<<16))
  {
    res= writebyte(252);
    return res == Result(stream_result::OK) ? write2int(x) : res;
  }

  res= writebyte(253);
  return res == Result(stream_result::OK) ? write4int(x) : res;
}


// Write/read string using "length coded string" format

template<class SW>
inline
typename IStream<SW>::Result
IStream<SW>::readstr(String &s)
{
  Result  res;
  uint    len;

  DBUG_ASSERT(m_win);

  if ((res= readint(len)) != Result(stream_result::OK))
    return res;

  if ((res= m_win->set_length(len)) != Result(stream_result::OK))
    return res;

  s.free();
  s.copy((const char*)m_win->head(), len, &::my_charset_bin);

  return m_win->move(len);
}

template<class SW>
inline
typename OStream<SW>::Result
OStream<SW>::writestr(const String &s)
{
  Result  res;
  uint    len= s.length();

  if ((res= writeint(len)) != Result(stream_result::OK))
    return res;

  if ((res= m_win->set_length(len)) != Result(stream_result::OK))
    return res;

  memcpy(m_win->head(), s.ptr(), len);

  return m_win->move(len);
}

} // util namespace


/************************************************************

  Backup Stream Interface

  The stream is organized as a sequence of chunks each of which
  can have different length. When stream is read chunk boundaries
  are detected. If this happens, next_chunk() member must be called
  in order to access data in next chunk. When writing to a stream,
  data is appended to the current chunk. End_chunk() member closes
  the current chunk and starts a new one.

 ************************************************************/

namespace backup {

struct stream_result
{
  enum value {
    OK=    util::stream_result::OK,
    NIL=   util::stream_result::NIL,
    ERROR= util::stream_result::ERROR,
    EOC,    // end of chunk
    EOS     // end of stream
  };
};

/**
  Convert stream_result into a corresponding result_t value.
  
  End of stream or chunk is reported as DONE, errors as ERROR and
  other stream results (OK,NIL) as OK.
 */ 
inline
result_t report_stream_result(const stream_result::value &res)
{
  switch (res) {
  
  case stream_result::ERROR:
    return ERROR;
    
  case stream_result::EOC:
  case stream_result::EOS:
    return DONE;
    
  default:
    return OK;
  }
}

/**
  Implementation of stream window interface to be used by util::{I,O}Stream
  templates.

  It provides a window inside a static data buffer <code>m_buf</code> of
  fixed size <code>buf_size</code>. The window starts at <code>m_head</code> and ends
  at <code>m_end</code>. Window can be moved and resized using <code>move()</code>
  and <code>set_length()</code> methods. These methods take into account
  size of <code>m_buf</code> and report overflows accordingly.

  The window can be used by the util::{I,O}Stream templates to read/write
  basic data types in a uniform, host independent way.
 */

class Window
{
 public:

  typedef stream_result::value  Result;

  byte* head()
  { return m_head; }

  byte* end()
  { return m_end; }

  size_t length()
  { return m_end - m_head; }

  Result set_length(const size_t);  ///< resize window to the given size
  Result move(const off_t);         ///< move beginning of the window (shrinks size)

  virtual ~Window() {}

 protected:

  Window():
   last_byte(m_buf+buf_size), m_head(m_buf), m_end(m_buf), m_blocked(FALSE)
  {}

  static const size_t buf_size= max_chunk_size; ///< data buffer size
  byte  m_buf[buf_size];

  byte  *last_byte; ///< points at the byte after the last byte of m_buf
  byte  *m_head;    ///< points at first byte of the current window
  byte  *m_end;     ///< points at the byte after the last byte of the current window

  bool  m_blocked;  ///< If true, set_length() and move() are blocked (return ERROR).

  /// Create empty window at offset in m_buf
  void reset(off_t offset=0)
  { m_head= m_end= m_buf+offset; }

  /// Define the result which should be returned in case of buffer overflow.
  virtual Result out_of_bounds() const
  { return stream_result::ERROR; }

  friend class util::OStream<Window>;
  friend class util::IStream<Window>;
};

} // backup namespace


namespace backup {

/****************************************************

   Definitions of input and output backup streams

 ****************************************************/

class Stream
{
 public:

  bool open();
  void close();
  bool rewind();

  /// Check if stream is opened
  operator bool()
  { return m_fd>0; }

  ~Stream()
  { close(); }

 protected:

  Stream(const String &name, int flags):
    m_fd(-1), m_path(name), m_flags(flags) {}

  int     m_fd;
  String  m_path;
  int     m_flags;  ///< flags used when opening the file

};

/**
  Implements backup stream which writes data to a file.

  This class inherits from util::OStream which defines methods for serialization of
  basic datatypes (strings and integer). It also implements the concept of data chunks. Data
  is stored in chunks - writing to an IStream appends data to the current chunk. A chunk is
  closed and a new one is started with <code>end_chunk()</code> method.

  A client of this class can ask an instance for an output buffer with <code>get_window()</code>
  method. After filling the buffer its contents can be written to the stream. This is to avoid
  double buffering and unnecessary copying of data. However, once an output buffer is allocated,
  all output to the stream is blocked until the buffer is written with <code>write_window()</code>
  or dropped with <code>drop_window()</code>.
 */

class OStream:
  private Window,
  public Stream,
  public util::OStream< Window >
{
  typedef util::OStream< Window > Base3;

 public:

  typedef stream_result::value Result;  // disambiguate

  size_t bytes; ///< number of bytes written

  bool open()
  {
    close(FALSE);
    return Stream::open() && rewind();
  }

  void close(bool destroy=TRUE);

  bool rewind()
  {
    Stream::rewind();
    Window::reset(2);
    bytes= 0;
    return TRUE;
  }

  Result end_chunk();

  /**
    Ask stream for output buffer of given size.

    If buffer is allocated, stream is blocked for other operations until
    either write_window() or drop_window() is called.

    @note Writing to stream using output buffer doesn't create chunks
    boundaries. Explicit call to end_chunk() is needed.
   */
  byte*   get_window(const size_t);
  Result  write_window(const size_t);
  void    drop_window();

  stream_result::value operator <<(const String &str)
  {
    return writestr(str);
  }

  OStream(const String &name):
    Stream(name,O_WRONLY|O_CREAT|O_TRUNC), Base3()
  { set_win(this); }

};


/**
  Implements backup stream reading data from a file.

  This class inherits from util::IStream which defines methods for serialization of
  basic datatypes (strings and integer). It also handles chunk boundaries as created by
  the OStream class. When reading data at the end of a data chunk, <code>stream_result::EOC</code>
  is returned. To access data in next chunk, <code>next_chunk()</code> must be called.
 */

class IStream:
  private Window,
  public Stream,
  public util::IStream< Window >
{
  typedef util::IStream< Window > Base3;

 public:

  typedef stream_result::value Result;  // disambiguate

  size_t bytes; ///< number of bytes read

  bool open()
  {
    close(FALSE);
    return Stream::open() && rewind();
  }

  void close(bool destroy=TRUE)
  {
    Stream::close();
    if (destroy)
      delete this;
  }

  bool rewind();

  Result next_chunk();

  stream_result::value operator >>(String &str)
  {
    return readstr(str);
  }

  /**
    Return current chunk.

    Will return the same chunk until next_chunk() is called.
   */
  stream_result::value operator >>(Buffer &buf)
  {
    m_end= last_byte;

    if (last_byte == m_buf) // empty chunk means end of stream
      return stream_result::EOS;

    buf.data= m_buf;
    buf.size= last_byte - m_buf;

    return stream_result::OK;
  }

  IStream(const String &name):
    Stream(name,O_RDONLY), Base3(), bytes(0)
  { 
    set_win(this);
    last_byte= m_buf; 
  }

 private:

  /// Length of next chunk of data or 0 if there are no more.
  uint next_chunk_len;  // we use 2 bytes to store chunk len

  Result out_of_bounds() const
  { return next_chunk_len > 0 ? stream_result::EOC : stream_result::EOS; }

  friend class stream_instances;
};

#ifdef DBUG_BACKUP

// Function for debugging backup stream implementation.
void dump_stream(IStream &);

#endif

} // backup namespace

#endif /*BACKUP_STREAM_H_*/
