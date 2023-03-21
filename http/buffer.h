//
//  buffer.h
//  c_compiler
//
//  Created by David Allison on 11/28/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef buffer_h
#define buffer_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Buffer of characters.  This is a dynamic array of chars.  The array will
// be expanded as space is exhausted.
//
// This is different from a String in that it can be used to hold any characters
// without any requirement that they have a \0 at the end.  It also uses a
// different allocation strategy, doubling the memory when it needs to expand.
typedef struct {
  char* value;      // Memory holding buffer value.
  size_t length;    // Size of value in memory.
  size_t capacity;  // Size of value we have space for.
} Buffer;

// Initializes an empty Buffer.
void BufferInit(Buffer* buf);

// Allocates a new Buffer using malloc and initializes it.
Buffer* NewBuffer(void);

// Destroys a Buffer by freeing up the pointer storage, not the memory
// used by the things being pointed to.
void BufferDestruct(Buffer* buf);
void BufferDelete(Buffer* buf);

void BufferClear(Buffer* buf);

// Appends a character array to the Buffer, reallocating the space
// as necessary.
void BufferAppend(Buffer* buf, char* value, size_t length);

// Appends a single byte.
void BufferAppendByte(Buffer* buf, char byte);

// Little endian values.
// TODO: add big endian too.
void BufferAppendHalfLE(Buffer* buf, uint16_t v);
void BufferAppendWordLE(Buffer* buf, uint32_t v);
void BufferAppendLongLE(Buffer* buf, uint64_t v);

// Adds some space to the buffer.
void BufferAddSpace(Buffer* buf, size_t length);
void BufferFill(Buffer* buf, size_t length, char value);

// Align the length of the buffer to the alignment (must be a power of 2).
void BufferAlignLength(Buffer* buf, int alignment);

// Compare buffer contents as in memcmp.
int BufferCompare(Buffer* b1, Buffer* b2);

#endif /* buffer_h */
