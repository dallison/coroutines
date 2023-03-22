//
//  buffer.c
//  c_compiler
//
//  Created by David Allison on 11/28/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "buffer.h"
#include <stdlib.h>
#include <string.h>

void BufferInit(Buffer* buf) {
  buf->value = NULL;
  buf->length = 0;
  buf->capacity = 0;
}

Buffer* NewBuffer() {
  Buffer* buf = malloc(sizeof(Buffer));
  BufferInit(buf);
  return buf;
}

void BufferDestruct(Buffer* buf) {
  free(buf->value);
  buf->length = buf->capacity = 0;
}

void BufferDelete(Buffer* buf) {
  BufferDestruct(buf);
  free(buf);
}

void BufferClear(Buffer* buf) { buf->length = 0; }

static void ExpandMemory(Buffer* buf, size_t new_length) {
  buf->capacity = new_length * 2;
  if (buf->value == NULL) {
    buf->value = calloc(buf->capacity, 1);
  } else {
    buf->value = realloc(buf->value, buf->capacity);
    memset(&buf->value[buf->length], 0, buf->capacity - buf->length);
  }
}

void BufferAppend(Buffer* buf, char* value, size_t length) {
  size_t new_length = buf->length + length;

  // Make room for new contents by doubling the necessary memory.
  if (new_length > buf->capacity) {
    ExpandMemory(buf, new_length);
  }

  // Append value to end of memory.
  memcpy(&buf->value[buf->length], value, length);
  buf->length += length;
}

void BufferAppendByte(Buffer* buf, char byte) {
  size_t new_length = buf->length + 1;

  // Make room for new contents by doubling the necessary memory.
  if (new_length > buf->capacity) {
    ExpandMemory(buf, new_length);
  }

  // Append value to end of memory.
  buf->value[buf->length] = byte;
  buf->length++;
}

void BufferAppendHalfLE(Buffer* buf, uint16_t v) {
  BufferAppend(buf, (char*)&v, 2);
}

void BufferAppendWordLE(Buffer* buf, uint32_t v) {
  BufferAppend(buf, (char*)&v, 4);
}

void BufferAppendLongLE(Buffer* buf, uint64_t v) {
  BufferAppend(buf, (char*)&v, 8);
}

void BufferAddSpace(Buffer* buf, size_t length) {
  size_t new_length = buf->length + length;
  // Make room for new contents by doubling the necessary memory,
  if (new_length > buf->capacity) {
    ExpandMemory(buf, new_length);
  }
  buf->length = new_length;
}

void BufferFill(Buffer* buf, size_t length, char value) {
  size_t new_length = buf->length + length;
  // Make room for new contents by doubling the necessary memory,
  if (new_length > buf->capacity) {
    ExpandMemory(buf, new_length);
  }
  memset(&buf->value[buf->length], value, length);
  buf->length = new_length;
}

void BufferAlignLength(Buffer* buf, int alignment) {
  size_t new_length = (buf->length + (alignment - 1)) & ~(alignment - 1);
  BufferAddSpace(buf, new_length - buf->length);
}

int BufferCompare(Buffer* b1, Buffer* b2) {
  size_t min = b1->length < b2->length ? b1->length : b2->length;
  int v = memcmp(b1->value, b2->value, min);
  if (v != 0) {
    return v;
  }
  return (int)(b1->length - b2->length);
}
