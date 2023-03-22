//
//  BitSet.h
//  c_compiler
//
//  Created by David Allison on 12/14/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef BitSet_h
#define BitSet_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "vector.h"

typedef struct {
  uint32_t* value;
  size_t capacity;  // Capacity in words, not bytes.
} BitSet;

void BitSetInit(BitSet* set);
BitSet* NewBitSet(void);
void BitSetDestruct(BitSet* set);
void BitSetDelete(BitSet* set);
void BitSetCopy(BitSet* to, BitSet* from);
size_t BitSetCount(BitSet* set);

void BitSetClear(BitSet* set);

void BitSetInsert(BitSet* set, size_t index);
bool BitSetContains(BitSet* set, size_t index);
void BitSetRemove(BitSet* set, size_t index);

void BitSetIntersection(BitSet* set1, BitSet* set2, BitSet* result);
void BitSetUnion(BitSet* set1, BitSet* set2, BitSet* result);
void BitSetUnionInPlace(BitSet* dest, BitSet* src);

bool BitSetEqual(BitSet* set1, BitSet* set2);

// Expand the values in a bitset to a vector of ints.
void BitSetExpand(BitSet* set, Vector* vec);

void BitSetPrint(BitSet* set, FILE* fp);

// Iterator.
typedef struct {
  BitSet* set;
  size_t word_offset;
  size_t bit_offset;
} BitSetIterator;

void BitSetIteratorStart(BitSetIterator* it, BitSet* set);

inline bool BitSetIteratorDone(BitSetIterator* it) {
  if (it->set->value == NULL) {
    return true;
  }
  if (it->word_offset >= it->set->capacity) {
    return true;
  }
  if (it->word_offset < (it->set->capacity - 1)) {
    return false;
  }
  int32_t mask = (1 << it->bit_offset) - 1;
  return (it->set->value[it->word_offset] & ~mask) == 0;
}

void BitSetIteratorNext(BitSetIterator* it);
inline size_t BitSetIteratorValue(BitSetIterator* it) {
  return it->word_offset * 32 + it->bit_offset;
}

// To use an iterator.
// BitSetIterator it;
// BitSetIteratorStart(&it, &set);
// while (!BigSetIteratorDone(&it)) {
//   size_t value = BitSetIteratorValue(&it);
//   BitSetIteratorNext(&it);
// }
#endif /* BitSet_h */
