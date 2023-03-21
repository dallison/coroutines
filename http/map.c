//
//  map.c
//  c_compiler
//
//  Created by David Allison on 12/17/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "dstring.h"

#define INIT_CAPACITY 2

void MapInit(Map* map, MapKeyCompareFunc compare_func) {
  map->capacity = 0;
  map->length = 0;
  map->values = NULL;
  map->compare = compare_func;
}

static int CompareStrings(const void* a, const void* b) {
  const MapKeyValue* v1 = a;
  const MapKeyValue* v2 = b;
  return StringCompareString(v1->key.p, v2->key.p);
}

static int CompareStringsCaseBlind(const void* a, const void* b) {
  const MapKeyValue* v1 = a;
  const MapKeyValue* v2 = b;
  return StringCompareStringCaseBlind(v1->key.p, v2->key.p);
}

static int CompareCharPointers(const void* a, const void* b) {
  const MapKeyValue* v1 = a;
  const MapKeyValue* v2 = b;
  return strcmp(v1->key.p, v2->key.p);
}

static int CompareCharPointersCaseBlind(const void* a, const void* b) {
  const MapKeyValue* v1 = a;
  const MapKeyValue* v2 = b;
  return strcasecmp(v1->key.p, v2->key.p);
}

static int CompareMappedInt64s(const void*a, const void* b) {
  const MapKeyValue* s1 = a;
  const MapKeyValue* s2 = b;
  return (int)(s1->key.w - s2->key.w);
}

static int CompareMappedPointers(const void*a, const void* b) {
  const MapKeyValue* s1 = a;
  const MapKeyValue* s2 = b;
  return (int)(s1->key.p - s2->key.p);
}

void MapInitForStringKeys(Map* map) {
  MapInit(map,  CompareStrings);
}

void MapInitForCaseBlindStringKeys(Map* map) {
  MapInit(map,  CompareStringsCaseBlind);
}

void MapInitForPointerKeys(Map* map) {
  MapInit(map,  CompareMappedPointers);
}

void MapInitForCharPointerKeys(Map* map) {
  MapInit(map,  CompareCharPointers);
}

void MapInitForCaseBlindCharPointerKeys(Map* map) {
  MapInit(map,  CompareCharPointersCaseBlind);
}

void MapInitForInt64Keys(Map* map) {
  MapInit(map,  CompareMappedInt64s);
}

Map* NewMap(MapKeyCompareFunc compare_func) {
  Map* map = malloc(sizeof(Map));
  MapInit(map, compare_func);
  return map;
}

Map* NewMapForStringKeys(void) {
  return NewMap(CompareStrings);
}

Map* NewMapForCharPointerKeys(void) {
  return NewMap(CompareCharPointers);
}

Map* NewMapForInt64Keys(void) {
  return NewMap(CompareMappedInt64s);
}

Map* NewMapForPointerKeys(void) {
  return NewMap(CompareMappedPointers);
}

Map* NewMapForCaseBlindStringKeys(void) {
  return NewMap(CompareStringsCaseBlind);
}

Map* NewMapForCaseBlindCharPointerKeys(void) {
  return NewMap(CompareCharPointersCaseBlind);
}

void MapDestruct(Map* map) {
  free(map->values);
  map->capacity = 0;
  map->length = 0;
}

void MapDelete(Map* map) {
  MapDestruct(map);
  free(map);
}

void MapClone(Map* dest, Map* src) {
  dest->capacity = src->capacity;
  dest->length = src->length;
  if (src->values == NULL) {
    dest->values = NULL;
  } else {
    dest->values = malloc(dest->capacity * sizeof(MapKeyValue));
    memcpy(dest->values, src->values, dest->capacity * sizeof(MapKeyValue));
  }
  dest->compare = src->compare;
}

void MapDestructWithContents(Map* map,
                             void (*func)(MapKeyValue *kv)) {
  for (size_t i = 0; i < map->length; i++) {
    if (func != NULL) {
      func(&map->values[i]);
    }
  }
  MapDestruct(map);
}

void MapDeleteWithContents(Map* map,
                           void (*func)(MapKeyValue* kv)) {
  MapDestructWithContents(map, func);
  free(map);
}

void MapClear(Map* map) { map->length = 0; }

static void MakeSpace(Map* map) {
  // If the map is initially empty, allocate it with default capacity.
  if (map->values == NULL) {
    map->capacity = INIT_CAPACITY;
    map->values = malloc(sizeof(MapKeyValue) * map->capacity);
    memset(map->values, 0, sizeof(MapKeyValue) * map->capacity);
  }

  // Make room for new contents.
  if (map->length + 1 > map->capacity) {
    size_t old_capacity = map->capacity;
    map->capacity *= 2;
    map->values = realloc(map->values, sizeof(MapKeyValue) * map->capacity);
    memset(map->values + old_capacity, 0,
           (map->capacity - old_capacity) * sizeof(MapKeyValue));
  }
}

// Append at end of memory.
static void Append(Map* map, MapKeyValue* key_value) {
  MakeSpace(map);
  size_t length = map->length++;
  map->values[length] = *key_value;
}

// Insert the key/value pair before the index.
static void InsertBefore(Map* map, size_t index, MapKeyValue* key_value) {
  assert(index < map->length);
  MakeSpace(map);
  size_t elements_to_move = map->length - index;
  memmove(map->values + index + 1, map->values + index,
          sizeof(MapKeyValue) * elements_to_move);
  map->values[index] = *key_value;
  map->length++;
}

// Remove the key/value pair at the index.
static void Remove(Map* map, size_t index) {
  assert(index < map->length);
  size_t elements_to_move = map->length - index - 1;
  memmove(map->values + index, map->values + index + 1,
          sizeof(MapKeyValue) * elements_to_move);
  map->length--;
}

// Find the location to insert into the map using a binary search.  Returns
// NULL if the location is outside the map, otherwise returns the next
// highest key/value pair which can be used to insert before.  If *found
// is set to true then an exact match has been found and the value returned
// is the key/value pair found.
static MapKeyValue* FindLocation(Map* map, MapKeyValue* key_value,
                                 bool* found) {
  *found = false;
  size_t low = 0;
  size_t high = map->length;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    int compval = map->compare(key_value, &map->values[mid]);
    if (compval == 0) {
      // Exact match found.
      *found = true;
      return &map->values[mid];
    }
    if (compval < 0) {
      // In first half.
      high = mid;
    } else {
      // In second half.
      low = mid + 1;
    }
  }
  if (high == map->length) {
    return NULL;
  }
  return &map->values[high];
}

// Insert into the map using a linear search for the location at
// which the insertion is done.  This is more efficient than binary
// insertions if the number of elements is small (<= 4).
static void* LinearInsert(Map* map, MapKeyValue* key_value) {
  for (size_t i = 0; i < map->length; i++) {
    int compval = map->compare(key_value, &map->values[i]);
    if (compval == 0) {
      // Matches exising value, replace value.
      void* old_value = map->values[i].value.p;
      map->values[i].value = key_value->value;
      return old_value;
    }
    if (compval < 0) {
      // value < vec[i]
      InsertBefore(map, i, key_value);
      return NULL;
    }
  }
  Append(map, key_value);
  return NULL;
}

// Insert into the map using a binary insertion.  This will be
// O(log2) because the keys are always sorted based on the
// comparison function.
static void* BinaryInsert(Map* map, MapKeyValue* key_value) {
  bool found;
  MapKeyValue* p = FindLocation(map, key_value, &found);
  if (p == NULL) {
    Append(map, key_value);
    return NULL;
  }
  if (found) {
    // Matches exising value, replace value.
    void* old_value = p->value.p;
    p->value = key_value->value;
    return old_value;
  }
  // Insert before 'p'.
  InsertBefore(map, p - map->values, key_value);
  return NULL;
}

// Insert the value in the map, keeping the values array sorted by key.
// Returns NULL if the value is newly inserted or the value replaced
// if the key is already present.
void* MapInsert(Map* map, MapKeyValue kv) {
  if (map->length < 5) {
    return LinearInsert(map, &kv);
  }
  return BinaryInsert(map, &kv);
}

void* MapFind(Map* map, MapKeyType key) {
  MapKeyValue key_value;
  key_value.key = key;
  MapKeyValue* result = bsearch(&key_value, map->values, map->length,
                                sizeof(MapKeyValue), map->compare);
  if (result == NULL) {
    return NULL;
  }
  return result->value.p;
}

MapValueType* MapSearch(Map* map, MapKeyType key) {
  MapKeyValue key_value;
  key_value.key = key;
  MapKeyValue* result = bsearch(&key_value, map->values, map->length,
                                sizeof(MapKeyValue), map->compare);
  if (result == NULL) {
    return NULL;
  }
  return &result->value;
}

void* MapFindPointerKey(Map* map, void* key) {
  MapKeyType k;
  k.p = key;
  return MapFind(map, k);
}

void* MapFindInt64Key(Map* map, int64_t key) {
  MapKeyType k;
  k.w = key;
  return MapFind(map, k);
}

void* MapRemove(Map* map, MapKeyType key) {
  MapKeyValue key_value;
  key_value.key = key;
  MapKeyValue* result = bsearch(&key_value, map->values, map->length,
                                sizeof(MapKeyValue), map->compare);
  if (result == NULL) {
    return NULL;
  }

  // Remove the key-value pair at the index found.
  void* value = result->value.p;
  Remove(map, result - map->values);
  return value;
}

void MapCopy(Map* dest, Map* src) {
  for (size_t i = 0; i < src->length; i++) {
    MapKeyValue* kv = &src->values[i];
    MapInsert(dest, *kv);
  }
}

void MapPrint(Map* map, void (*printer)(const MapKeyValue* kv)) {
  printf("{");
  const char* sep = "";
  for (size_t i = 0; i < map->length; i++) {
    printf("%s", sep);
    printer(&map->values[i]);
    sep = ", ";
  }
  printf("}");
}

void MapTraverse(Map* map,
                 void (*func)(MapKeyValue* kv, void* data),
                 void* data) {
  for (size_t i = 0; i < map->length; i++) {
    func(&map->values[i], data);
  }
}
