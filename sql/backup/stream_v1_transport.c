#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "stream_v1.h"
#include "stream_v1_services.h"

/**
  @file

  @brief
  Implementation of the low-level I/O functions from backup stream library.

  These functions form the transport layer of the 1st version of backup stream
  format. They split stream into a sequence of data chunks.
*/

/**
  @brief Default size of a stream block.

  When opening stream for writing a different size can be specified.
*/
#define DEFAULT_BLOCK_SIZE  (32*1024)

/**
  @brief Input buffer size.

  When reading stream, we need sometimes to look ahead into it (for example
  to see a fragment header byte). This constant determines how much data will
  be loaded into internal input buffer in such cases.

  @note

  - The underlying stream will most probably have its own input buffer and
  thus increasing this constant will introduce more double buffering.

  - The size of input buffer can be different from the stream block size.
*/
#define INPUT_BUF_SIZE      512

/**
  @brief
  Minimal amount of data considered for writing without internal buffering.

  The write code tries to avoid double buffering and write data directly to
  the underlying stream if possible. However, for small amounts of data it is
  perhaps better to save them in an internal output buffer and write the whole
  buffer later. This constant defines the threshold below which data will be
  buffered internally.
*/
#define MIN_WRITE_SIZE      1024

/* Local type definitions. */

#define TRUE    1
#define FALSE   0
#define ASSERT(X) assert(X)

typedef unsigned char bool;
typedef bstream_byte byte;
typedef bstream_blob blob;


/*
 Carrier format
 --------------

 [backup stream] = [ block ! ... ! block ]

 [block] = [ fragment ! ... ! fragment ]

 where [fragment] is one of

 [EOC marker] = [ 0x80 ]
 [EOS marker] = [ 0xC0 ]
 [fragment extending to the end of block, last in a chunk] = [ 0x00 ! payload ]
 [fragment extending to the end of block, more follow] = [ 0x40 ! payload ]
 [limited size fragment] = [ header:1 ! payload ]

 The header of [limited size fragment] consists of two bit fragment type info
 followed by 6 bit, non-zero value encoding length of the fragment:

 [fragment header] = [ type:.2 ! value:.6 ]

 There are four types of fragments determined by the first 2 bits of the header:

 00 - small fragment which is the last fragment of a chunk,
 01 - small fragment with more fragments following it,
 10 - big fragment (more fragments follow),
 11 - huge fragment (more fragments follow).

 Encoding of the size of the fragment depends on its type:

 - for small fragments: size= value
 - for big fragments:   size= value << 6
 - for huge fragments:  size= value << 12

 For small fragments, second bit of the header determines if it is the last
 fragment of a chunk or there are more fragments to come. Chunk can't end with
 a big or huge fragment and thus for these fragments we always expect more
 fragments to come. [EOC marker] ends a chunk, even if the last fragment said
 that more fragments will follow.

 The biggest fragment size is 64*2^12 ~= 250 Kb. The format of fragment header
 puts constraints on possible fragment sizes. If a chunk of data has size not
 possible to encode by a single fragment header, it is split into several
 fragments of correct sizes.
*/


/*************************************************************************
 *
 *   ENCODING OF CHUNK FRAGMENTS
 *
 *************************************************************************/

#define FR_EOC    0x80
#define FR_EOS    0xC0
#define FR_MORE   0x00
#define FR_LAST   0x40

#define FR_TYPE_MASK  0xC0
#define FR_LEN_MASK   (~FR_TYPE_MASK)

/** biggest size of small fragment */
#define FR_SMALL_MAX  ((size_t)FR_LEN_MASK)
#define FR_BIG        0x80        /**< type bits for big fragment */
#define FR_HUGE       0xC0        /**< type bits for huge fragment */
#define FR_BIG_SHIFT  6           /**< value shift for big fragment */
#define FR_HUGE_SHIFT 12          /**< value shift for huge fragment */
/** header for the biggest possible chunk */
#define FR_HUGE_MAX_HDR (FR_HUGE|FR_LEN_MASK)
/** size of the biggest possible chunk */
#define FR_HUGE_MAX_LEN ((size_t)FR_LEN_MASK << FR_HUGE_SHIFT)

/**
  Determine biggest prefix of a given blob of data which can be stored as
  a single fragment.

  @return FR_EOC if there is no data in the blob. Otherwise header of the
  prefix fragment and @c *data is updated to describe the part of the blob
  which is not included in the prefix.
*/
static
byte biggest_fragment_prefix(blob *data)
{
  size_t len= data->end - data->begin;
  byte *prefix_end= data->begin;
  byte hdr;

  ASSERT(data->end >= data->begin);

  if (len == 0)
    return FR_EOC;

  if (len > FR_HUGE_MAX_LEN)
  {
    data->begin += FR_HUGE_MAX_LEN;
    return FR_HUGE_MAX_HDR;
  }

  hdr= len & FR_LEN_MASK;
  prefix_end= data->begin + hdr;

  len >>= 6;
  if (len)
  {
    hdr = FR_BIG|(len & FR_LEN_MASK);
    prefix_end= data->begin + ((len & FR_LEN_MASK) << FR_BIG_SHIFT);
  }

  len >>= 6;
  if (len)
  {
    hdr = FR_HUGE|(len & FR_LEN_MASK);
    prefix_end= data->begin + ((len & FR_LEN_MASK) << FR_HUGE_SHIFT);
  }

  data->begin = prefix_end;
  return hdr;
}

/**
 Read fragment header.

 Fragment header is in the byte pointed by @c *header. After reading the
 header, @c *header is set to point at the first byte after the
 end of the fragment unless the fragment extends to the end of the current
 block, in which case @c *header is unchanged.

 @retval BSTREAM_EOC   header contains EOC marker
 @retval BSTREAM_EOS   header contains EOS marker
 @retval FR_MORE  normal fragment which is not the last fragment of a chunk
 @retval FR_LAST  this is last fragment of a chunk
*/
static
int read_fragment_header(byte **header)
{
  byte hdr= *((*header)++);
  size_t len= hdr & FR_LEN_MASK; /* 6 bit value stored in the header */
  bool last;

  if (hdr == FR_EOS)
    return BSTREAM_EOS;

  if (hdr == FR_EOC)
    return BSTREAM_EOC;

  last= hdr & FR_LAST ? FR_LAST : FR_MORE; /* is it last fragment of a chunk? */

  /*
    If len == 0 then we have a fragment which extends to the end of a block,
    thus we restore *header to signal that fact to the caller.
   */
  if (len == 0)
  {
    (*header)--;
    return last;
  }

  /*
    If the highest bit is set, we are looking at a big or huge fragment. Such
    fragment is never the last fragment of a chunk.
   */
  if (hdr & 0x80)
  {
    last= FR_MORE;

    if ((hdr & FR_TYPE_MASK) == FR_BIG)
      len <<= FR_BIG_SHIFT;

    if ((hdr & FR_TYPE_MASK) == FR_HUGE)
      len <<= FR_HUGE_SHIFT;
  }

  /* move *header pointer to the first byte after the fragment */
  *header += len;

  return last;
}


/*************************************************************************
 *
 *   OPERATIONS ON ABSTRACT STREAM
 *
 *************************************************************************/

/*
 These macros use function pointers stored in a backup_stream structure
 (parameter S) to write/read bytes to/from underlying stream.
*/

#define as_write(S,Data,Env) \
  ((S)->write ? (S)->write((S),(Data),(Env)) : BSTREAM_ERROR)

#define as_read(S,Buf,Env) \
  ((S)->read ?(S)->read((S),(Buf),(Env)) : BSTREAM_ERROR)

#define as_forward(S,Off) \
  ((S)->forward ? (S)->forward((S),(Off)) : (*(Off)=0, BSTREAM_ERROR))

/** Write all bytes in a given blob */
int as_write_all(struct st_abstract_stream *s, bstream_blob b, bstream_blob env)
{
  int ret= BSTREAM_OK;

  while (ret == BSTREAM_OK && b.begin < b.end)
   ret= as_write(s,&b,env);

  return ret;
};

/** Fill blob with bytes from stream */
int as_read_all(struct st_abstract_stream *s, bstream_blob b, bstream_blob env)
{
  int ret= BSTREAM_OK;

  while (ret == BSTREAM_OK && b.begin < b.end)
   ret= as_read(s,&b,env);

  return ret;
};


/*************************************************************************
 OUTPUT BUFFER

 It stores data to be written to the next block in the output stream. Data
 in block is divided into fragments. If buffer is non-empty, the header of
 last fragment stored in it is pointed by header. pos marks the end of data
 in the buffer, end indicates how much bytes is left until the end of output
 block

    other fragments   current fragment   free space
 [===================|XX:=============:..............]
 ^                    ^               ^              ^
 begin                header          pos            end

 Note: pos == header means that the current fragment is empty and when appending
 data to it, one byte should be left for the fragment header.

 Invariant:

    (end - begin) is the number of bytes left to the end of block

 *************************************************************************/

/**
  Prepare stream`s output buffer for writing a new block
*/
static
void reset_output_buffer(backup_stream *s)
{
  s->buf.pos= s->buf.header= s->buf.begin= s->mem.begin;
  s->buf.end= s->buf.begin + s->block_size;
}

/**
  Write stream output buffer's contents to the output stream.

  After this call the buffer becomes empty (pos == header == begin). The end
  pointer indicates end of output block as always (the invariant).

  @returns error code if there was an error while writing to the output stream.
*/
static
int write_buffer(backup_stream *s)
{
  int ret= BSTREAM_OK;

  if (s->buf.pos > s->buf.begin)
    ret= as_write_all(&s->stream,*(bstream_blob*)&s->buf,s->mem);

  /*
    Now buffer should be empty. If whole block has been written, reset buffer
    so that it is prepared for next block. Otherwise leave as much space as
    remains to the end of the block.
   */
  if (s->buf.pos == s->buf.end)
    reset_output_buffer(s);
  else
    s->buf.begin= s->buf.header= s->buf.pos; /* now invariant should hold */

  return ret;
}

/**
  Append given blob of data to the current fragment in stream`s output buffer.

  @note if the buffer or the current fragment are empty then one byte space
  is left for fragment`s header.
*/
static
int append_to_buffer(backup_stream *s, blob *b)
{
  if (b->begin >= b->end) // no data to append
    return BSTREAM_OK;

  if(s->buf.pos == s->buf.header) // current fragment empty
    s->buf.pos++;

  while (s->buf.pos < s->buf.end && b->begin < b->end)
   *(s->buf.pos++)= *(b->begin++);

  return BSTREAM_OK;
}

/**
  Prepare the current fragment in stream`s output buffer to be written
  to the output stream.

  This means correctly setting the header of the fragment. If it is not possible
  to describe fragment`s length in a single byte header, the data is split into
  more fragments. When splitting is done, the last of the resulting fragments
  becomes the current fragment in the buffer (with header correctly set)
  while all other data from the buffer is send to the output stream.

  @return Error code if there was an error while writing output stream.
*/
static
int close_current_fragment(backup_stream *s)
{
  struct st_bstream_buffer *buf= &s->buf;
  int ret= BSTREAM_OK;

  /* blob describing data in the current fragment */
  blob current_fragment= {buf->header+1, buf->pos};

  /* nothing to do if current fragment is empty */
  if (buf->pos == buf->header)
    return BSTREAM_OK;

  /*
    The following loop will split the current_fragment data into as many
    sub-fragments as necessary, chopping off a biggest possible prefix in each
    iteration.

    However, if the data extends to the end of the block, we don't have to
    do any splitting since we can create fragment which spans to the end of the
    block.
   */
  while (current_fragment.begin < current_fragment.end &&
         current_fragment.end < buf->end)
  {
    /*
      Determine longest prefix of the current fragments which can be described
      by a fragemnt header.
     */
    *(buf->header)= biggest_fragment_prefix(&current_fragment);

    /*
      If there are any bytes not included in the prefix, we write prefix
      to the output stream and deal with the remainder in the next iteration
      of the loop.
     */
    if (current_fragment.begin < current_fragment.end)
    {
      buf->pos= current_fragment.begin;

      ret= write_buffer(s); /* this empties the buffer */
      if (ret != BSTREAM_OK)
        return ret;

      buf->pos= current_fragment.end;

      /*
        Now buffer contains only the remaining bytes. We shift whole bufer
        one byte to the left to make space for the header of next fragment.
       */
      buf->begin= --(buf->header);
      buf->end--; /* now invariant should hold */
    }
  }

  /*
    If there any data left here it means that it extends to the end of the
    block. We set the fragment header accordingly.
   */
  if (current_fragment.begin < current_fragment.end)
  {
    ASSERT(current_fragment.end == buf->end);
    *(buf->header)= FR_MORE;
  }

  return BSTREAM_OK;
}

/*************************************************************************
 INPUT BUFFER

 Stores bytes read from input stream. These bytes are stored in the region
 starting from begin and ending at pos (pos points one byte after the end of
 data). The bytes starting from begin belong to the current fragment. The next
 fragment starts at byte pointed by header (this byte contains fragment`s
 header). When all data from the current fragment have been read, begin==header.
 To start reading next fragment, it must be entered so that header is moved to
 point at the header of the fragment following it (this is done in the
 load_next_fragment() function).

 The header of next fragment can be inside the input buffer:

                               rest of input block
              next fragment   (still in the stream)
 [==========|XX:============].......................|
  ^          ^              ^                       ^
  begin      header         pos                     end

 or still in the stream:

 [=============]...........|........................|
  ^            ^            ^                       ^
  begin        pos          header                  end

 In each case header points at the position where the header should be if
 the data were present in the buffer.

 Invariant:

    (end - pos) is the number of bytes left in the input block
    header <= end and header points at the first byte of next fragment

 *************************************************************************/

/**
  Prepare stream`s input buffer for reading next block from input stream.
*/
static
void reset_input_buffer(backup_stream *s)
{
  s->buf.header -= (s->buf.pos - s->mem.begin);
  s->buf.pos= s->buf.begin= s->mem.begin;
  s->buf.end= s->buf.begin + s->block_size;
}

/**
  Read few more bytes into the stream`s input buffer if it is empty.

  Normally INPUT_BUF_SIZE bytes is read, unless current input block doesn't
  contain that many bytes in which case the rest of the input block is read.

  @retval BSTREAM_OK   input buffer was filled with data
  @retval BSTREAM_EOS  end of stream was hit, all remaining bytes from the stream
                  are in the buffer
  @retval BSTREAM_ERROR  error when reading from the stream
*/
static
int load_buffer(backup_stream *s)
{
  int ret= BSTREAM_OK;
  byte  *saved_begin;
  size_t howmuch= s->buf.end - s->buf.pos;

  /* do nothing if there already is some data in the buffer */
  if (s->buf.pos > s->buf.begin)
    return BSTREAM_OK;

  /*
    Call reset_input_buffer() to move buffer head to the beginning of
    available memory
   */
  reset_input_buffer(s);

  /*
    If howmuch > 0 the current input block has not been completely read yet.
    In that case we restore input buffer's invariant by setting end pointer
    accordingly.
   */
  if (howmuch > 0)
    s->buf.end= s->buf.pos + howmuch;
  else
    howmuch= s->buf.end - s->buf.pos;

  /* don't read more than INPUT_BUF_SIZE */
  if (howmuch > INPUT_BUF_SIZE)
    howmuch= INPUT_BUF_SIZE;

  /* read into the buffer howmuch bytes from the input stream */
  s->buf.pos += howmuch;
  saved_begin= s->buf.begin;

  ret= as_read(&s->stream,(bstream_blob*)&s->buf,s->mem);

  s->buf.pos= s->buf.begin;
  s->buf.begin= saved_begin;

  return ret;
}

/**
  Setup input buffer to read next fragment.

  This moves buf.header so that it points at the header of the fragment
  following the fragment which is entered now. buf.begin will point at the
  first byte of the entered fragment.

  @pre All bytes from previous fragment have been consumed (buf.begin ==
  buf.header) and header of next fragment is loaded into the buffer.

  @retval BSTREAM_OK   next fragment has been entered
  @retval BSTREAM_EOC  the entered fragment is the last fragment of a chunk
  @retval BSTREAM_EOS  there are no more fragments in the stream
*/
static
int load_next_fragment(backup_stream *s)
{
  byte *saved_header= s->buf.header;

  ASSERT(s->buf.pos > s->buf.header);
  ASSERT(s->buf.begin == s->buf.header);

  s->reading_last_fragment= 0;

  int ret= read_fragment_header(&s->buf.header);

  /*
    If buf.header was not moved, it means that the fragment extends to
    the end of the input block and thus we set buf.header= buf.end.
   */
  if (s->buf.header == saved_header)
    s->buf.header= s->buf.end;

  /* move buf.begin to point at the first byte of the fragment */
  s->buf.begin++;

  /*
    It can happen that fragment header was the last byte in
    the block. In that case we reload input buffer and start over.

    TODO: remove recursion
   */
  if (s->buf.begin == s->buf.end)
  {
    ret= load_buffer(s);
    return ret == BSTREAM_OK ? load_next_fragment(s) : ret;
  }

  switch (ret) {

  case BSTREAM_EOS: s->state= EOS; return BSTREAM_EOS;

  case BSTREAM_EOC: s->reading_last_fragment= 1; return BSTREAM_EOC;

  case FR_LAST: s->reading_last_fragment= 1;

  default: return BSTREAM_OK;
  }
}


/*********************************************************************
 *
 *   INITIALIZATION
 *
 *********************************************************************/

/**
  Open backup stream for writing.

  @pre The abstract stream methods in @c s should be setup and ready for
  writing.

  @note Output buffer is allocated.
*/
int bstream_open_wr(backup_stream *s, unsigned long int block_size)
{
  s->state= ERROR;
  s->block_size= block_size > 0 ? block_size : DEFAULT_BLOCK_SIZE;

  s->mem.begin= bstream_alloc(s->block_size);

  if (!s->mem.begin)
    return BSTREAM_ERROR;

  s->mem.end= s->mem.begin + s->block_size;

  reset_output_buffer(s);

  s->data_buf.begin= s->data_buf.end= NULL;
  s->state= WRITING;

  return BSTREAM_OK;
}

/**
  Open backup stream for reading.

  @pre The abstract stream methods in @c s should be setup and ready for
  reading.

  @note Input buffer is allocated.
*/
int bstream_open_rd(backup_stream *s, unsigned long int block_size)
{
  s->state= ERROR;
  s->block_size= block_size;

  s->mem.begin= bstream_alloc(INPUT_BUF_SIZE);

  if (!s->mem.begin)
    return BSTREAM_ERROR;

  s->mem.end= s->mem.begin + INPUT_BUF_SIZE;

  /* initialize input buffer */
  reset_input_buffer(s);
  s->buf.header= s->buf.begin;

  s->data_buf.begin= s->data_buf.end= NULL;
  s->state= READING;

  return BSTREAM_OK;
}

int bstream_end_chunk(backup_stream *s);
int bstream_flush(backup_stream *s);

/**
  Close backup stream.

  @note This function can be used both for streams opened for reading or
  for writing.
*/
int bstream_close(backup_stream *s)
{
  int ret= BSTREAM_OK;

  if (s->state == CLOSED)
    return BSTREAM_OK;

  if (s->state == WRITING)
  {
    bstream_end_chunk(s);
    bstream_flush(s);
    /* write EOS marker */
    reset_output_buffer(s);
    *(s->buf.pos++)= FR_EOS;
    ret= write_buffer(s);
  }

  if (s->mem.begin)
  {
    bstream_free(s->mem.begin);
    s->mem.begin= NULL;
  }

  if (s->data_buf.begin)
  {
    bstream_free(s->data_buf.begin);
    s->data_buf.begin= NULL;
  }

  if (s->state == ERROR)
    return BSTREAM_ERROR;

  s->state= CLOSED;

  return ret;
}


/*********************************************************************
 *
 *   WRITING
 *
 *********************************************************************/

/**
  Write part of buffer to backup stream.

  Write the part of @c buf described by @c data blob. Not all bytes form
  @c data need to be written at once - the blob is modified to describe these
  bytes which have not been written.

  @verbatim

  buf  [---------------=================--------]

  data blob before writing:

                      [=================]

  data blob after writing:

                       **********[======]
                       ^          ^
                       |          bytes yet to be written
                       bytes which have been written
  @endverbatim

  @retval BSTREAM_OK at least one byte was written (or copied to the output buffer)
  @retval BSTREAM_ERROR  error was detected
*/
int bstream_write_part(backup_stream *s, bstream_blob *data, bstream_blob buf)
{
  int ret;
  blob fragment;
  byte *saved_end;

  if (s->state != WRITING)
    return BSTREAM_ERROR;

  if (data->begin >= data->end)
    return BSTREAM_OK;

  if (s->buf.pos == s->buf.end)
  {
    ret= bstream_flush(s);
    if (ret != BSTREAM_OK)
      return ret;
  }

  ASSERT(s->buf.pos < s->buf.end);

  /* if current fragment is empty, make space for the header */
  if (s->buf.pos == s->buf.header)
    s->buf.pos++;

  /*
    Setup fragment to describe all the data available in the current fragment
    of the stream`s output buffer together with the data to be written

       output buffer
               current fragment
   [ ===== | 0x00 ===============]
            ^
            header                       data
                                 [====================]

                  [-----------------------------------]
                              fragment
   */
  fragment.begin= s->buf.header+1;
  fragment.end=   s->buf.pos + (data->end - data->begin);

  /*
    If there is enough data to fill a complete block, we will write it and
    mark that the last fragment extends to its end.

    We first write the contents of output buffer and then as much of data
    as needed to fill the block.
   */
  if (fragment.end > s->buf.end)
  {
    /*
      Possible optimization: try to write block in one go by copying part
      of data to output buffer or prepending output buffer to data if there is
      space for that. Do that only if it doesn't mean a lot of data copying.
     */

    *s->buf.header= FR_MORE;

    ret= write_buffer(s);
    if (ret != BSTREAM_OK)
      return ret;

    /* write bytes from data blob to fill the block */
    saved_end= data->end;
    data->end= data->begin + (s->buf.end - s->buf.pos);

    ret= as_write_all(&s->stream,*data,buf);
    if (ret != BSTREAM_OK)
      return ret;

    data->begin= data->end;
    data->end= saved_end;

    reset_output_buffer(s); /* prepare for next block */
    return BSTREAM_OK;
  }

  /*
   To avoid copying bytes to the internal output buffer we try to cut a prefix
   of the data to be written which forms a valid fragment and write this
   fragment to output stream.
  */
  *(s->buf.header)= biggest_fragment_prefix(&fragment);

  /*
    We use this method only if it will actually write enough of the bytes
    to be written - if it is only few bytes we save them into the output
    buffer anyway.
   */
  if (fragment.end > s->buf.pos + MIN_WRITE_SIZE)
  {
    /* write contents of the output buffer */
    ret= write_buffer(s);
    if (ret != BSTREAM_OK)
      return ret;

    /* write remainder of the fragment from data blob */
    saved_end= data->end;
    data->end= data->begin + (fragment.end - s->buf.pos);

    ret= as_write_all(&s->stream,*data,*data);

    data->begin= data->end;
    data->end= saved_end;

    /* move buffer beginning to keep the invariant */

    s->buf.begin = fragment.end;
    s->buf.pos= s->buf.header= s->buf.begin;

    return ret;
  }

  /*
    If nothing else worked, we just append the data to the output buffer
    and return.
   */
  append_to_buffer(s,data);
  return BSTREAM_OK;
}

/**
  Write bytes to backup stream.

  @param s      backup stream to write to
  @param b      blob with bytes to be written

  The blob is modified to indicate which data was not written.

  @retval BSTREAM_OK at least one byte was written (or copied to the output buffer)
  @retval BSTREAM_ERROR  error was detected
*/
int bstream_write(backup_stream *s, bstream_blob *b)
{
  blob buf= *b;

  return bstream_write_part(s,b,buf);
}

/**
  Write complete blob to backup stream.

  This function iterates until all bytes of the blob are written or
  error is detected.

  @retval BSTREAM_OK all bytes from the blob have been written (or copied to the
                output buffer)
  @retval BSTREAM_ERROR  error was detected
*/
int bstream_write_blob(backup_stream *s, bstream_blob buf)
{
  blob envelope= buf;
  int ret= BSTREAM_OK;

  while (ret == BSTREAM_OK && buf.begin < buf.end)
    ret= bstream_write_part(s,&buf,envelope);

  return ret;
}

/**
  Create new chunk in the stream.

  The new chunk will contain all data which has been send to the stream
  after the last chunk was closed. If no data was sent nothing will happen
  (it is not possible to have an empty chunk).

  @retval BSTREAM_OK chunk has been created
  @retval BSTREAM_ERROR  error was detected
*/
int bstream_end_chunk(backup_stream *s)
{
  int ret= BSTREAM_OK;

  if (s->state != WRITING)
    return BSTREAM_ERROR;


  ret= close_current_fragment(s);

  /*
    If buffer is empty, store EOC marker in it otherwise mark the last
    fragment of the chunk.
   */
  if (s->buf.pos == s->buf.begin)
  {
    *(s->buf.header++)= FR_EOC;
    s->buf.pos= s->buf.header;
  }
  else
    *s->buf.header |= FR_LAST;

  /*
    Start new fragment. Note that if the current fragment is empty, these
    operations will have no effect.
  */
  s->buf.header= s->buf.pos;

  if (s->buf.pos == s->buf.end-1)
  {
    *(s->buf.pos++)= FR_MORE;
    ret= bstream_flush(s);
  }

  return ret;
}

/**
 Flush backup stream`s output buffer to the output stream.
*/
int bstream_flush(backup_stream *s)
{
  struct st_bstream_buffer *buf= &s->buf;
  int ret;

  if (s->state != WRITING)
    return BSTREAM_ERROR;

  /* if buffer is empty, do nothing */
  if (buf->pos == buf->begin)
    return BSTREAM_OK;

  /*
    If current fragment is empty, just write the other fragments stored
    in the buffer
   */
  if (buf->pos == buf->header)
  {
    ret= write_buffer(s);
    return ret;
  }

  /* Otherwise close the current fragment to set its header */
  ret= close_current_fragment(s);
  if (ret != BSTREAM_OK)
    return ret;

  /*
    If there is only one byte left to the end of block, we fill this byte with
    a header of a fragment which will effectively be empty.
   */
  if (buf->pos == buf->end-1)
    *(buf->pos++)= FR_MORE;

  if (buf->pos > buf->begin)
    ret= write_buffer(s);

  /* start new fragment */

  buf->header= buf->pos;

  return ret;
}


/*********************************************************************
 *
 *   READING
 *
 *********************************************************************/

/**
  Read data from the stream to the indicated part of a buffer.

  Data are stored in the part of @c buf described by @c data. At most as many
  bytes will be read as will fit into the @c data blob, however less bytes can
  be read. After the call, @c data is modified to describe the area which was
  not filled with data.

  @verbatim

  buf  [---------------=================--------]

  data blob before reading:

                      [=================]

  data blob after reading:

                       **********[======]
                       ^          ^
                       |          space remaining free
                       bytes which have been read
  @endverbatim

  @retval BSTREAM_EOC  current data chunk ended while reading data - the following
                  calls to @c bstream_read() will not read any data until we move to
                  the next chunk with @c bstream_next_chunk()
  @retval BSTREAM_EOS  end of stream was detected while reading data
  @retval BSTREAM_ERROR error was detected while reading data
  @retval BSTREAM_OK   successful read - at least one byte was read
*/
int bstream_read_part(backup_stream *s, bstream_blob *data, bstream_blob buf)
{
  int ret= BSTREAM_OK;
  size_t howmuch;
  blob saved;

  if (s->state != READING)
    return s->state == EOS ? BSTREAM_EOS : BSTREAM_ERROR;

  /* fill input buffer if it is empty */
  if (s->buf.pos == s->buf.begin)
  {
    ret= load_buffer(s);
    if (ret != BSTREAM_OK)
      return ret;
  }

  ASSERT(s->buf.pos > s->buf.begin);

  /*
    If we finished reading a fragment, we should load next one
    or signal EOC if it was the last fragment of a chunk.
   */
  if (s->buf.header == s->buf.begin)
  {
    if (s->reading_last_fragment)
      return BSTREAM_EOC;

    ret= load_next_fragment(s);
    if (ret != BSTREAM_OK)
      return ret;
  }

  /*
    Determine length of the fragment remainder stored in the input
    buffer.
  */
  if (s->buf.header <= s->buf.pos)
    howmuch= s->buf.header - s->buf.begin;
  else
    howmuch= s->buf.pos - s->buf.begin;

  /*
    If there is some data in the buffer we copy it to the data blob,
    otherwise we fill data from the stream.
   */
  if (howmuch > 0)
  {
    if (howmuch > (size_t)(data->end - data->begin))
      howmuch= data->end - data->begin;

    memmove(data->begin, s->buf.begin, howmuch);
    data->begin  += howmuch;
    s->buf.begin += howmuch;

  }
  else
  {
    /* read directly into data area */
    ASSERT(s->buf.header > s->buf.pos);

    howmuch= data->end - data->begin;
    if (howmuch > (size_t)(s->buf.header - s->buf.pos))
      howmuch= s->buf.header - s->buf.pos;

    saved= *data;
    data->end= data->begin + howmuch;

    as_read(&s->stream,data,buf);

    s->buf.begin += data->begin - saved.begin;
    s->buf.pos= s->buf.begin;

    data->begin= data->end;
    data->end= saved.end;
  }

  return s->buf.begin == s->buf.header && s->reading_last_fragment ?
         BSTREAM_EOC: BSTREAM_OK;
}


/**
  Read bytes from backup stream.

  @param s      backup stream to read from
  @param b      blob where the data should be placed

  The blob is modified to describe the area which was not filled with the data.

  @retval BSTREAM_EOC  current data chunk ended while reading data - the
            following calls to @c bstream_read() will not read any data until
            we move to the next chunk with @c bstream_next_chunk()
  @retval BSTREAM_EOS   end of stream was detected while reading data
  @retval BSTREAM_ERROR error was detected while reading data
  @retval BSTREAM_OK    successful read - at least one byte was read
*/
int bstream_read(backup_stream *s, bstream_blob *b)
{
  return bstream_read_part(s,b,*b);
}


/**
  Read complete blob to backup stream.

  This function iterates until whole blob is filled with bytes from the
  stream.
*/
int bstream_read_blob(backup_stream *s, bstream_blob buf)
{
  blob envelope= buf;
  int ret= BSTREAM_OK;

  while (ret == BSTREAM_OK && buf.begin < buf.end)
    ret= bstream_read_part(s,&buf,envelope);

  return buf.begin == buf.end ? ret : BSTREAM_ERROR;
}

/**
 Position backup stream at the first byte of next chunk.

 If there are no more chunks in the stream, stream position is
 moved to its end (so that following reads will not read any bytes
 and report EOS).

 @retval BSTREAM_OK  stream is positioned at the beginning of next chunk
 @retval BSTREAM_EOS there are no more chunks in the stream
 @retval BSTREAM_ERROR an error was detected - stream position is undefined
*/
int bstream_next_chunk(backup_stream *s)
{
  int ret;
  unsigned long int howmuch;

  if (s->state != READING)
    return s->state == EOS ? BSTREAM_EOS : BSTREAM_ERROR;

  /* if we are not at the beginning of next fragment, move there */

  if (s->buf.begin < s->buf.header)
  {
    /*
      The header of next fragment can be in the buffer, or still in
      the stream. In former case we just move beginning of the buffer to the
      header, otherwise we move the stream forward and empty the buffer so that
      header will be loaded into it When the buffer is filled below.
     */
    if (s->buf.header < s->buf.pos)
      s->buf.begin= s->buf.header;
    else
    {
      howmuch= s->buf.header - s->buf.pos;
      as_forward(&s->stream, &howmuch);
      s->buf.begin= s->buf.pos= s->buf.header;
    }
  }

  /*
    If buffer is empty, we load few bytes into it to have access to
    the fragment header.
   */
  if (s->buf.pos == s->buf.begin)
    load_buffer(s);

  ASSERT(s->buf.begin == s->buf.header);
  ASSERT(s->buf.pos > s->buf.header);

  ret= load_next_fragment(s);

  /* if we hit EOC marker here, we treat it as empty chunk */
  if (ret == BSTREAM_EOC)
    ret= BSTREAM_OK;

  return ret;
}

/**
  Skip given amount of bytes in the stream.

  @todo do something also when the abstract stream doesn't define forward
  function (e.g., read and discard bytes).
*/
int bstream_skip(backup_stream *s, unsigned long int howmuch)
{
  return as_forward(&s->stream, &howmuch);
}
