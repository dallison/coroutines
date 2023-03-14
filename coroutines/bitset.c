//
//  BitSet.c
//  c_compiler
//
//  Created by David Allison on 12/14/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "bitset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void BitSetInit(BitSet* set) {
  set->value = NULL;
  set->capacity = 0;
}

BitSet* NewBitSet() {
  BitSet* set = malloc(sizeof(BitSet));
  BitSetInit(set);
  return set;
}

void BitSetDestruct(BitSet* set) {
  free(set->value);
  set->value = NULL;
  set->capacity = 0;
}

void BitSetDelete(BitSet* set) {
  BitSetDestruct(set);
  free(set);
}

void BitSetClear(BitSet* set) {
  if (set->value == NULL) {
    return;
  }
  memset(set->value, 0, set->capacity * sizeof(uint32_t));
}

static void CalculateIndexes(size_t index, size_t* word, size_t* bit) {
  *word = index / 32;
  *bit = index % 32;
}

// Make room for an index into the set.
static void MakeRoomFor(BitSet* set, size_t index) {
  size_t words_required = (index + 32) / 32;
  if (set->value == NULL) {
    set->value = calloc(words_required, sizeof(uint32_t));
    set->capacity = words_required;
  } else if (words_required > set->capacity) {
    set->value = realloc(set->value, words_required * sizeof(uint32_t));
    // Clear new memory.
    memset(&set->value[set->capacity], 0,
           (words_required - set->capacity) * sizeof(uint32_t));
    set->capacity = words_required;
  }
}

// Make room for a number of words.
static void MakeRoom(BitSet* set, size_t words) {
  if (set->value == NULL) {
    set->value = calloc(words, sizeof(uint32_t));
    set->capacity = words;
  } else if (words > set->capacity) {
    set->value = realloc(set->value, words * sizeof(uint32_t));
    set->capacity = words;
  }
}

void BitSetInsert(BitSet* set, size_t index) {
  MakeRoomFor(set, index);
  size_t word, bit;
  CalculateIndexes(index, &word, &bit);
  set->value[word] |= 1 << bit;
}

bool BitSetContains(BitSet* set, size_t index) {
  size_t word, bit;
  CalculateIndexes(index, &word, &bit);
  if (word >= set->capacity) {
    return false;
  }
  return (set->value[word] & (1 << bit)) != 0;
}

void BitSetRemove(BitSet* set, size_t index) {
  MakeRoomFor(set, index);
  size_t word, bit;
  CalculateIndexes(index, &word, &bit);
  set->value[word] &= ~(1 << bit);
}

void BitSetIntersection(BitSet* set1, BitSet* set2, BitSet* result) {
  size_t min = set1->capacity;
  if (min > set2->capacity) {
    min = set2->capacity;
  }
  // If the min size is zero then the result is an empty set.
  if (min == 0) {
    return;
  }
  MakeRoom(result, min);
  memcpy(result->value, set1->value, min * sizeof(uint32_t));
  for (size_t i = 0; i < min && i < set2->capacity; i++) {
    result->value[i] &= set2->value[i];
  }
}

void BitSetUnion(BitSet* set1, BitSet* set2, BitSet* result) {
  size_t max = set1->capacity;
  if (max < set2->capacity) {
    max = set2->capacity;
  }
  MakeRoom(result, max);
  memcpy(result->value, set1->value, set1->capacity * sizeof(uint32_t));
  for (size_t i = 0; i < set2->capacity; i++) {
    result->value[i] |= set2->value[i];
  }
}

void BitSetUnionInPlace(BitSet* dest, BitSet* src) {
  size_t max = dest->capacity;
  if (max < src->capacity) {
    max = src->capacity;
  }
  MakeRoom(dest, max);
  for (size_t i = 0; i < src->capacity; i++) {
    dest->value[i] |= src->value[i];
  }
}

void BitSetCopy(BitSet* to, BitSet* from) {
  MakeRoom(to, from->capacity);
  memcpy(to->value, from->value, from->capacity * sizeof(uint32_t));
}

bool BitSetEqual(BitSet* set1, BitSet* set2) {
  BitSet* larger = set2;
  size_t min = set1->capacity;
  if (min > set2->capacity) {
    min = set2->capacity;
    larger = set1;
  }

  // First compare area where the capacities match.  One may have a larger
  // capacity but all the extra bits might be zero.
  int v = memcmp(set1->value, set2->value, min * sizeof(uint32_t));
  if (v != 0) {
    return false;
  }
  // Now we look at the extra bits in the larger set to see if they
  // are all zero.
  for (size_t i = min; i < larger->capacity; i++) {
    if (larger->value[i] != 0) {
      return false;
    }
  }
  return true;
}

void BitSetExpand(BitSet* set, Vector* vec) {
  size_t index = 0;
  for (size_t word = 0; word < set->capacity; word++) {
    for (size_t bit = 0; bit < 32; bit++) {
      if ((set->value[word] & (1 << bit)) != 0) {
        VectorAppend(vec, (void*)index);
      }
      index++;
    }
  }
}

size_t BitSetCount(BitSet* set) {
  size_t count = 0;
  for (size_t word = 0; word < set->capacity; word++) {
    for (size_t bit = 0; bit < 32; bit++) {
      if ((set->value[word] & (1 << bit)) != 0) {
        count++;
      }
    }
  }
  return count;
}

void BitSetPrint(BitSet* set, FILE* fp) {
  fprintf(fp, "{");
  const char* sep = "";
  size_t index = 0;
  for (size_t word = 0; word < set->capacity; word++) {
    for (size_t bit = 0; bit < 32; bit++) {
      if ((set->value[word] & (1 << bit)) != 0) {
        fprintf(fp, "%s%zd", sep, index);
        sep = ", ";
      }
      index++;
    }
  }
  fprintf(fp, "}");
}

void BitSetIteratorStart(BitSetIterator* it, BitSet* set) {
  it->set = set;
  it->word_offset = 0;
  it->bit_offset = 0;
  size_t index = 0;
  // Find first bit with value 1.
  for (size_t word = 0; word < set->capacity; word++) {
    for (size_t bit = 0; bit < 32; bit++) {
      if ((set->value[word] & (1 << bit)) != 0) {
        it->word_offset = word;
        it->bit_offset = bit;
        return;
      }
      index++;
    }
  }
}

bool BitSetIteratorDone(BitSetIterator* it);

void BitSetIteratorNext(BitSetIterator* it) {
  it->bit_offset++;
  while (it->word_offset < it->set->capacity) {
    while (it->bit_offset < 32 &&
           (it->set->value[it->word_offset] & (1 << it->bit_offset)) == 0) {
      it->bit_offset++;
    }
    if (it->bit_offset < 32) {
      return;
    }
    it->bit_offset = 0;
    it->word_offset++;
  }
}

size_t BitSetIteratorValue(BitSetIterator* it);
