#include "../mysql_priv.h"

#include <backup_stream.h>
#include "stream.h"

const unsigned char backup_magic_bytes[8]=
{
  0xE0, // ###.....
  0xF8, // #####...
  0x7F, // .#######
  0x7E, // .######.
  0x7E, // .######.
  0x5F, // .#.#####
  0x0F, // ....####
  0x03  // ......##
};

namespace backup {

/**
  Low level write for backup stream library.

  Pointer to this function is stored in @c backup_stream::stream structure
  and then used by other stream library function for physical writing of
  data.
*/
extern "C" int stream_write(void *instance, bstream_blob *buf, bstream_blob)
{
  int fd;
  int res;

  DBUG_ENTER("backup::IStream::write");

  DBUG_ASSERT(instance);
  DBUG_ASSERT(buf);

  OStream *s= (OStream*)instance;

  fd= s->m_fd;

  DBUG_ASSERT(fd >= 0);

  if (!buf->begin || buf->begin == buf->end)
    DBUG_RETURN(BSTREAM_OK);

  DBUG_ASSERT(buf->end);

  size_t howmuch = buf->end - buf->begin;

  res= my_write(fd, buf->begin, howmuch,
                MY_NABP /* error if not all bytes written */ );

  if (res)
    DBUG_RETURN(BSTREAM_ERROR);

  s->bytes += howmuch;

  buf->begin= buf->end;
    DBUG_RETURN(BSTREAM_OK);
}

/**
  Low level read for backup stream library.

  Pointer to this function is stored in @c backup_stream::stream structure
  and then used by other stream library function for physical reading of
  data.
*/
extern "C" int stream_read(void *instance, bstream_blob *buf, bstream_blob)
{
  int fd;
  size_t howmuch;

  DBUG_ENTER("backup::IStream::read");

  DBUG_ASSERT(instance);
  DBUG_ASSERT(buf);

  IStream *s= (IStream*)instance;

  fd= s->m_fd;

  DBUG_ASSERT(fd >= 0);

  if (!buf->begin || buf->begin == buf->end)
    DBUG_RETURN(BSTREAM_OK);

  DBUG_ASSERT(buf->end);

  howmuch= buf->end - buf->begin;

  howmuch= my_read(fd, buf->begin, howmuch, MYF(0));

  /*
   How to detect EOF when reading bytes with my_read().

   We assume that my_read(fd, buf, count, MYF(0)) behaves as POSIX read:

   - if it returns -1 then error has been detected.
   - if it returns N>0 then N bytes have been read.
   - if it returns 0 then there are no more bytes in the stream (EOS reached).
  */

  if (howmuch == (size_t) -1)
    DBUG_RETURN(BSTREAM_ERROR);

  if (howmuch == 0)
    DBUG_RETURN(BSTREAM_EOS);

  s->bytes += howmuch;
  buf->begin += howmuch;
  DBUG_RETURN(BSTREAM_OK);
}


Stream::Stream(const ::String &name, int flags):
  m_fd(-1), m_path(name), m_flags(flags)
{
  bzero(&stream, sizeof(stream));
  bzero(&buf, sizeof(buf));
  bzero(&mem, sizeof(mem));
  bzero(&data_buf, sizeof(data_buf));
  block_size= 0;
  state= CLOSED;
}

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


OStream::OStream(const ::String &name):
  Stream(name,O_WRONLY|O_CREAT|O_EXCL|O_TRUNC), bytes(0)
{
  stream.write= stream_write;
  m_block_size=0; // use default block size provided by the backup stram library
}

/**
  Write the magic bytes and format version number at the beginning of a stream.

  Stream should be positioned at its beginning.

  @return Number of bytes written or -1 if error.
*/
int OStream::write_magic_and_version()
{
  byte buf[10];

  DBUG_ASSERT(m_fd >= 0);

  memmove(buf,backup_magic_bytes,8);
  // format version = 1
  buf[8]= 0x01;
  buf[9]= 0x00;

  int ret= my_write(m_fd, buf, 10,
                    MY_NABP /* error if not all bytes written */ );
  if (ret)
    return -1; // error when writing magic bytes
  else
    return 10;
}

/**
  Open and initialize backup stream for writing.

  @retval TRUE  operation succeeded
  @retval FALSE operation failed

  @todo Report errors.
*/
bool OStream::open()
{
  close(FALSE);

  bool ret= Stream::open();

  if (!ret)
    return FALSE;

  // write magic bytes and format version
  int len= write_magic_and_version();

  if (len <= 0)
  {
    // TODO: report errors
    return FALSE;
  }

  bytes= 0;
  ret= BSTREAM_OK == bstream_open_wr(this,m_block_size,len);
  // TODO: report errors
  return ret;
}

/**
  Close backup stream

  If @c destroy is TRUE, the stream object is deleted.
*/
void OStream::close(bool destroy)
{
  if (m_fd<0)
    return;

  bstream_close(this);
  Stream::close();

  if (destroy)
    delete this;
}

/**
  Rewind output stream so that it is positioned at its beginning and
  ready for writing new image.

  @retval TRUE  operation succeeded
  @retval FALSE operation failed
*/
bool OStream::rewind()
{
  bstream_close(this);

  bool ret= Stream::rewind();

  if (!ret)
    return FALSE;

  int len= write_magic_and_version();

  if (len <= 0)
    return FALSE;

  ret= BSTREAM_OK == bstream_open_wr(this,m_block_size,len);

  return ret;
}


IStream::IStream(const ::String &name):
  Stream(name,O_RDONLY), bytes(0)
{
  stream.read= stream_read;
}

/**
  Check that input stream starts with correct magic bytes and
  version number.

  Stream should be positioned at its beginning.

  @return Number of bytes read or -1 if error.
*/
int IStream::check_magic_and_version()
{
  byte buf[10];

  DBUG_ASSERT(m_fd >= 0);

  int ret= my_read(m_fd, buf, 10,
                   MY_NABP /* error if not all bytes read */ );
  if (ret)
    return -1; // couldn't read magic bytes

  if (memcmp(buf,backup_magic_bytes,8))
    return -1; // wrong magic bytes

  unsigned int ver = buf[8] + (buf[9]<<8);

  if (ver != 1)
    return -1; // unsupported format version

  return 10;
}

/**
  Open backup stream for reading.

  @retval TRUE  operation succeeded
  @retval FALSE operation failed

  @todo Report errors.
*/
bool IStream::open()
{
  close(FALSE);

  bool ret= Stream::open();

  if (!ret)
    return FALSE;

  int len= check_magic_and_version();

  if (len <= 0)
  {
    // TODO: report errors
    return FALSE;
  }

  bytes= 0;

  ret= BSTREAM_OK == bstream_open_rd(this,len);
  // TODO: report errors
  return ret;
}

/**
  Close backup stream

  If @c destroy is TRUE, the stream object is deleted.
*/
void IStream::close(bool destroy)
{
  if (m_fd<0)
    return;

  bstream_close(this);
  Stream::close();

  if (destroy)
    delete this;
}

/**
  Rewind input stream so that it can be read again.

  @retval TRUE  operation succeeded
  @retval FALSE operation failed
*/
bool IStream::rewind()
{
  bstream_close(this);

  bool ret= Stream::rewind();

  if (!ret)
    return FALSE;

  int len= check_magic_and_version();

  if (len < 0)
    return FALSE;

  ret= BSTREAM_OK == bstream_open_rd(this,len);

  return ret;
}

/// Move to next chunk in the stream.
int IStream::next_chunk()
{
  return bstream_next_chunk(this);
}

} // backup namespace
