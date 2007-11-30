#ifndef BACKUP_STREAM_H_
#define BACKUP_STREAM_H_

// magic bytes defined in stream.cc
extern const unsigned char backup_magic_bytes[8];

extern "C" {

// We use version 1 of the stream format.

#include "stream_v1.h"

}

#endif /*BACKUP_STREAM_H_*/
