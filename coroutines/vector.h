//
//  vector.h
//  c_compiler
//
//  Created by David Allison on 10/26/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef vector_h
#define vector_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Vector of 64-bit values.  This is a dynamic array.  The array will
// be expanded as space is exhausted.  The Vector does not own anything
// stored in it and is not responsible for freeing up memory occupied by
// the values.
//
typedef struct Vector {
  union {
    void** p;       // Pointers.
    int64_t* w;     // 64-bit values.
  } value;
  size_t length;    // Number of pointers in memory.
  size_t capacity;  // Number of pointers we have space for.
} Vector;

// Initializes an empty vector.
void VectorInit(Vector* vec);

// Allocates a new vector using malloc and initializes it.
Vector* NewVector(void);

// Function pointer type to destruct contents of vector.
typedef void (*VectorElementDestructor)(void*);

// Reserve space for 'n' elements.  If n is less than the current
// capacity this will have no effect.  Otherwise the capacity is
// increased to n.
void VectorReserve(Vector* vec, size_t n);

// Destroys a vector by freeing up the pointer storage, not the memory
// used by the things being pointed to.
void VectorDestruct(Vector* vec);
void VectorDelete(Vector* vec);

// Destroys the vector and the contents by calling the destructor function
// for each element.  The destructor is called for each element but it should
// not free the element.  Id free_element is true, his function will free the
// element, in which case the elements must be allocated using malloc.
void VectorDestructWithContents(Vector* vec,
                                VectorElementDestructor destructor,
                                bool free_element);
void VectorDeleteWithContents(Vector* vec, VectorElementDestructor destructor,
                              bool free_element);

// Clears the vector but doesn't touch the elements - just sets the length to
// zero and leaves the capacity as is.
void VectorClear(Vector* vec);

// Clears the vector and also destruct and frees the elements if free_element
// is true.  The elements must be allocated using malloc if free_element is
// true.
void VectorClearWithContents(Vector* vec, VectorElementDestructor destructor,
                             bool free_element);

// Appends a pointer to the vector, reallocating the space
// as necessary.
void VectorAppend(Vector* vec, void* value);

// Sets an element in the vector to the value given.
void VectorSet(Vector* vec, size_t index, void* value);

// Gets a value from the vector at the given index.
void* VectorGet(Vector* vec, size_t index);

// Gets the first pointer in the vector.
void* VectorFirst(Vector* vec);

// Gets the last pointer in the vector.
void* VectorLast(Vector* vec);

void VectorCopy(Vector* dest, Vector* src);
void VectorAppendVector(Vector* dest, Vector* src);

void VectorPush(Vector* v, void* value);
void VectorPop(Vector* v);

bool VectorEqual(Vector* a, Vector* b);

void VectorInsertBefore(Vector* vec, size_t index, void* value);
void VectorInsertAfter(Vector* vec, size_t index, void* value);
void VectorDeleteElement(Vector* vec, size_t index);

void VectorSortPointers(Vector* vec, int (*compare)(const void*, const void*));
void VectorSortInts(Vector* vec, int (*compare)(const void*, const void*));

#endif /* vector_h */
