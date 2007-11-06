#include "../mysql_priv.h"

#include "stream.h"

/*
  TODO

  - blocking of OStream output when data window is allocated.
  - use my_read instead of read - need to know how to detect EOF.
  - remove fixed chunk size limit (backup::Window::buf_size)
  - better file buffering (in case of small data chunks)
 */


// Instantiate templates used in backup stream classes
template class util::IStream< backup::Window >;
template class util::OStream< backup::Window >;

namespace backup {

/************** Window *************************/

Window::Result Window::set_length(const size_t len)
{
  DBUG_ASSERT(!m_blocked);

  m_end= m_head+len;

  if (m_end <= last_byte)
    return stream_result::OK;

  m_end= last_byte;
  return out_of_bounds();
}

Window::Result Window::move(const off_t offset)
{
  DBUG_ASSERT(!m_blocked);

  m_head+= offset;

  if (m_head > m_end)
    m_end= m_head;

  if (m_head <= last_byte)
    return stream_result::OK;

  m_head= m_end= last_byte;
  return out_of_bounds();
}

/************** Stream *************************/

bool Stream::open()
{
  close();
  m_fd= my_open(m_path.c_ptr(),m_flags,MYF(0));
  return m_fd >= 0;
}

void Stream::close()
{
  if (m_fd >= 0)
  {
    my_close(m_fd,MYF(0));
    m_fd= -1;
  }
}

bool Stream::rewind()
{
  return m_fd >= 0 && my_seek(m_fd,0,SEEK_SET,MYF(0)) == 0;
}


/************** OStream *************************/

/*
  Implementation of data chunks.

  Data is written to the file in form of data chunks. Each chunk is prefixed with its size stored
  in 2 bytes (should it be increased to 4?).


  OStream instance uses an output buffer of fixed size inherited from Window class. The size of
  a chunk is limited by the size of this buffer as a whole chunk is stored inside the buffer
  before writing to the file.

  writing to the file happens when the current chunk is closed with <code>end_chunk()</code>
  method. At the time of writing the output, buffer contents is as follows:

  ====================== <- m_buf
  2 bytes for chunk size
  ====================== <- m_buf+2 (chunk data starts here)

    data written to
    the chunk

  ---------------------- <- m_head

   current output window

  ====================== <- m_end  (this is end of chunk data)

 */

byte* OStream::get_window(const size_t len)
{
  if (m_blocked || m_end+len > last_byte)
    return NULL;

  m_head= m_end;
  m_end+= len;
  m_blocked= TRUE;

  return m_head;
}

void OStream::drop_window()
{
  if (m_blocked)
   m_end= m_head;

  m_blocked= FALSE;
}

OStream::Result
OStream::write_window(const size_t len)
{
  if (m_blocked)
  {
    DBUG_ASSERT(m_head+len<=m_end);
    m_head+=len;
    m_end= m_head;
  }

  m_blocked= FALSE;

  return stream_result::OK;
}


void OStream::close(bool destroy)
{
  if (m_fd<0)
    return;

  end_chunk();

  // write 0 at the end
  last_byte=m_buf+2;
  Window::reset();
  write2int(0);

  my_write(m_fd,m_buf,2,MYF(0));

  Stream::close();

  if (destroy)
    delete this;
}

stream_result::value OStream::end_chunk()
{
  if (m_blocked)
    drop_window();

  DBUG_ASSERT(m_end >= m_buf+2);

  size_t len= m_end - m_buf - 2; // length of the chunk

  if (len==0)
  {
    Window::reset(2);
    return stream_result::OK;
  }

  // store length of chunk in front of the buffer
  Window::reset();
  write2int(len);

  bytes+= len;

  len+= 2;  // now len is the number of bytes we want to write

  uint res= my_write(m_fd,m_buf,len,MY_NABP);

  Window::reset(2);

  if (res)
    return stream_result::ERROR;

  return stream_result::OK;
}

/************** IStream *************************/

/*
  Handling of stream data chunks.

  Chunks are read into the input buffer inherited from <code>Window</code> class. It is assumed
  that a whole chunk will always fit into the buffer (otherwise error is reported).

  When reading a chunk of data, the size of the next chunk is also read-in in the same file access
  and stored in the <code>next_chunk_len</code> member.

  The input buffer has the following layout:

  =================== <- m_buf (start of input buffer)

   chunk data

  ------------------- <- m_head
   current input
      window
  ------------------- <- m_end

   chunk data

  =================== <- last_byte  (end of chunk data)
   size of next chunk
  =================== <- last_byte+2

  The first chunk of data is read into the input buffer when stream is opened. Next chunks are
  read inside <code>next_chunk()</code> method.

 */

// PRE: there is at least one chunk in the stream.

bool IStream::rewind()
{
  Stream::rewind();
  Window::reset();
  bytes= 0;

  if (my_read(m_fd, m_buf, 2, MYF(0)) < 2)
    return FALSE;

  last_byte= m_head+2;

  read2int(next_chunk_len);

  Window::reset();  // ignore the 2 bytes containing chunk length
  last_byte= m_buf;

  return next_chunk() == stream_result::OK;
}

stream_result::value IStream::next_chunk()
{
  bytes+= (last_byte-m_buf);    // update statistics

  last_byte= m_buf;

  if (next_chunk_len == 0)
  {
    Window::reset();
    return stream_result::EOS;
  }

  size_t len= next_chunk_len+2;

  long int howmuch= 0;  // POSIX ssize_t not defined on win platform :|

  while (len > 0 && (howmuch= ::read(m_fd,m_buf,len)) > 0)
    len-= howmuch;

  if (howmuch<0) // error reading file
  {
    next_chunk_len= 0;
    Window::reset();
    return stream_result::ERROR;
  }

  if (len == 0)
  {
    // read length of next chunk (at the end of the buffer)
    last_byte+= next_chunk_len+2;
    Window::reset(next_chunk_len);
    read2int(next_chunk_len);
    last_byte-=2;
  }
  else
  {
    last_byte+= next_chunk_len+2-len;
    next_chunk_len= 0;
  }

  Window::reset();

  return howmuch==0 ? stream_result::EOS : stream_result::OK;
}

#ifdef DBUG_BACKUP

// Show data chunks in a backup stream;

void dump_stream(IStream &s)
{
  stream_result::value  res;
  byte b;

  DBUG_PRINT("stream",("=========="));

  do {

    uint chunk_size;

    for( chunk_size=0; (res= s.readbyte(b)) == stream_result::OK ; ++chunk_size );

    DBUG_PRINT("stream",(" chunk size= %u",chunk_size));

    if( res == stream_result::EOC )
    {
      DBUG_PRINT("stream",("----------"));
      res= s.next_chunk();
    }

  } while ( res == stream_result::OK );

  if (res == stream_result::EOS)
   DBUG_PRINT("stream",("=========="));
  else
   DBUG_PRINT("stream",("== ERROR: %d",(int)res));
}

#endif

} // backup namespace
