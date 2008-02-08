#ifndef _BACKUP_STREAM_H_
#define _BACKUP_STREAM_H_

#include <backup_stream.h>

#include <backup/api_types.h>    // for Buffer definition
#include "debug.h"        // for definition of DBUG_BACKUP

/**
  @file

 Interface layer between backup kernel and the backup stream library defining
 format of the data written/read.

*/

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
    OK= BSTREAM_OK,
    EOC= BSTREAM_EOC,
    EOS= BSTREAM_EOS,
    ERROR= BSTREAM_ERROR
  };
};


extern "C" int stream_write(void *instance, bstream_blob *buf, bstream_blob);
extern "C" int stream_read(void *instance, bstream_blob *buf, bstream_blob);

/****************************************************

   Definitions of input and output backup streams

 ****************************************************/

/**
  Base for @c OStream and @c IStream.

  It stores file descriptor and provides basic methods for operating on
  it. It also inherits from (and correctly fills) the backup_stream structure
  so that an instance of @c Stream class can be passed to backup stream library
  functions.
*/
class Stream: public backup_stream
{
 public:

  bool open();
  void close();
  bool rewind();

  /// Check if stream is opened
  bool is_open() const
  { return m_fd>0; }

  ~Stream()
  { close(); }

 protected:

  Stream(const ::String&, int);

  int     m_fd;
  String  m_path;
  int     m_flags;  ///< flags used when opening the file
  size_t  m_block_size;

  friend int stream_write(void*,bstream_blob*,bstream_blob);
  friend int stream_read(void*,bstream_blob*,bstream_blob);
};

/// Used to write to backup stream.
class OStream:
  public Stream
{
 public:

  size_t bytes; ///< number of bytes written

  OStream(const ::String&);

  bool open();
  void close(bool destroy=TRUE);
  bool rewind();

 private:

  int write_magic_and_version();
};

/// Used to read from backup stream.
class IStream:
  public Stream
{
 public:

  size_t bytes; ///< number of bytes read

  IStream(const ::String &name);

  bool open();
  void close(bool destroy=TRUE);
  bool rewind();

  int next_chunk();

 private:

  int check_magic_and_version();
};

} // backup namespace

#endif /*BACKUP_STREAM_H_*/
