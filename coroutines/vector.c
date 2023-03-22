//
//  vector.c
//  c_compiler
//
//  Created by David Allison on 10/26/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// A lot of vectors only have one member so let's keep the
// initial capacity small so that we don't waste memory.
#define INIT_CAPACITY 1

void VectorInit(Vector* vec) {
  vec->value.p = NULL;
  vec->length = 0;
  vec->capacity = 0;
}

Vector* NewVector() {
  Vector* vec = malloc(sizeof(Vector));
  VectorInit(vec);
  return vec;
}

void VectorDestruct(Vector* vec) {
  free(vec->value.p);
  vec->length = vec->capacity = 0;
  vec->value.p = NULL;
}

void VectorDelete(Vector* vec) {
  VectorDestruct(vec);
  free(vec);
}

void VectorDestructWithContents(Vector* vec, VectorElementDestructor destructor,
                                bool free_element) {
  for (size_t i = 0; i < vec->length; i++) {
    if (vec->value.p[i] == NULL) {
      continue;
    }
    if (destructor != NULL) {
      (*destructor)(vec->value.p[i]);
    }
    if (free_element) {
      free(vec->value.p[i]);
    }
  }
  VectorDestruct(vec);
}

void VectorDeleteWithContents(Vector* vec, VectorElementDestructor destructor,
                              bool free_element) {
  VectorDestructWithContents(vec, destructor, free_element);
  free(vec);
}

void VectorClear(Vector* vec) { vec->length = 0; }

void VectorClearWithContents(Vector* vec, VectorElementDestructor destructor,
                             bool free_element) {
  for (size_t i = 0; i < vec->length; i++) {
    if (vec->value.p[i] == NULL) {
      continue;
    }
    if (destructor != NULL) {
      (*destructor)(vec->value.p[i]);
    }
    if (free_element) {
      free(vec->value.p[i]);
    }
  }
  vec->length = 0;
}

static void MakeSpace(Vector* vec) {
  // If the vector is initially empty, allocate it with default capacity.
  if (vec->value.p == NULL) {
    vec->capacity = INIT_CAPACITY;
    vec->value.p = malloc(sizeof(int64_t) * vec->capacity);
    memset(vec->value.p, 0, sizeof(int64_t) * vec->capacity);
  }

  // Make room for new contents.
  if (vec->length + 1 > vec->capacity) {
    size_t old_capacity = vec->capacity;
    vec->capacity *= 2;
    vec->value.p = realloc(vec->value.p, sizeof(int64_t) * vec->capacity);
    memset(vec->value.p + old_capacity, 0,
           (vec->capacity - old_capacity) * sizeof(int64_t));
  }
}

void VectorReserve(Vector* vec, size_t n) {
  if (vec->value.p == NULL) {
    vec->capacity = n;
    ;
    vec->value.p = malloc(sizeof(int64_t) * vec->capacity);
    memset(vec->value.p, 0, sizeof(int64_t) * vec->capacity);
  }

  if (n <= vec->capacity) {
    return;
  }
  size_t old_capacity = vec->capacity;
  vec->capacity = n;
  vec->value.p = realloc(vec->value.p, sizeof(int64_t) * vec->capacity);
  memset(vec->value.p + old_capacity, 0,
         (vec->capacity - old_capacity) * sizeof(int64_t));
}

void VectorAppend(Vector* vec, void* value) {
  MakeSpace(vec);

  // Append value to end of memory.
  vec->value.p[vec->length] = value;
  vec->length++;
}

void VectorSet(Vector* vec, size_t index, void* value) {
  vec->value.p[index] = value;
}

void* VectorGet(Vector* vec, size_t index) { return vec->value.p[index]; }

void* VectorFirst(Vector* vec) {
  return vec->length == 0 ? NULL : vec->value.p[0];
}

void* VectorLast(Vector* vec) {
  return vec->length == 0 ? NULL : vec->value.p[vec->length - 1];
}

void VectorCopy(Vector* dest, Vector* src) {
  VectorClear(dest);
  for (size_t i = 0; i < src->length; i++) {
    VectorAppend(dest, src->value.p[i]);
  }
}

void VectorAppendVector(Vector* dest, Vector* src) {
  for (size_t i = 0; i < src->length; i++) {
    VectorAppend(dest, src->value.p[i]);
  }
}

void VectorPush(Vector* v, void* value) { VectorAppend(v, value); }

void VectorPop(Vector* v) {
  if (v->length > 0) {
    v->length--;
  }
}

bool VectorEqual(Vector* a, Vector* b) {
  if (a->length != b->length) {
    return false;
  }
  for (size_t i = 0; i < a->length; i++) {
    if (a->value.p[i] != b->value.p[i]) {
      return false;
    }
  }
  return true;
}

void VectorInsertBefore(Vector* vec, size_t index, void* value) {
  assert(index < vec->length);
  MakeSpace(vec);
  size_t elements_to_move = vec->length - index;
  memmove(vec->value.p + index + 1, vec->value.p + index,
          sizeof(int64_t) * elements_to_move);
  vec->value.p[index] = value;
  vec->length++;
}

void VectorInsertAfter(Vector* vec, size_t index, void* value) {
  assert(index < vec->length);
  if (index == vec->length - 1) {
    VectorAppend(vec, value);
    return;
  }

  MakeSpace(vec);
  size_t elements_to_move = vec->length - index - 1;

  memmove(vec->value.p + index + 2, vec->value.p + index + 1,
          sizeof(int64_t) * elements_to_move);
  vec->value.p[index + 1] = value;
  vec->length++;
}

void VectorDeleteElement(Vector* vec, size_t index) {
  assert(index < vec->length);
  size_t elements_to_move = vec->length - index - 1;
  memmove(vec->value.p + index, vec->value.p + index + 1,
          sizeof(int64_t) * elements_to_move);
  vec->length--;
}

void VectorSortPointers(Vector* vec, int (*compare)(const void*, const void*)) {
  qsort(vec->value.p, vec->length, sizeof(void*), compare);
}

void VectorSortInts(Vector* vec, int (*compare)(const void*, const void*)) {
  qsort(vec->value.w, vec->length, sizeof(int64_t), compare);
}
