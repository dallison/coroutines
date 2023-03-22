//
//  map.h
//  c_compiler
//
//  Created by David Allison on 12/17/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef map_h
#define map_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Simple binary map between two values.  The map is held as a contiguous array
// of MapKeyValue struct, each of which has a key and a value.  The map also
// has a comparison function that can compare two MapKeyValue pairs for ordering
// (like the bsearch and qsort functions use).  The map's memory is always held
// in sorted order (sorted by key) and there are no holes in it.  The 'length'
// field says how many key/value pairs are present in the map.  The 'capacity'
// field is the number of key/value pairs for which we have space in the array.
// The array is expanded as needed, but never contracts.

// Key type.
typedef union {
  void* p;
  int64_t w;
} MapKeyType;

// Value type;
typedef union {
  void* p;
  int64_t w;
} MapValueType;

// The map's memory is an array of these structs.
typedef struct {
  MapKeyType key;
  MapValueType value;
} MapKeyValue;

// Comparison function, taking MapKeyValue pointers.  Returns 0 if the keys
// are identical, <0 if the first arg is less than the second and >0 if the
// other way around.  You can use, for example, strcmp to compare strings.
typedef int (*MapKeyCompareFunc)(const void*, const void*);

typedef struct {
  MapKeyValue* values;        // Contiguous, sorted key/value pairs.
  size_t length;              // Number of keys in map.
  size_t capacity;            // Total space in the values array.
  MapKeyCompareFunc compare;  // Comparison function.
} Map;

// Initializes the map with a function to perform comparisons.  The function
// is the same as those used for the qsort and bsearch C library functions.
void MapInit(Map* map, MapKeyCompareFunc compare_func);
Map* NewMap(MapKeyCompareFunc compare_func);

// Initializers for common map types.
void MapInitForStringKeys(Map* map);
void MapInitForCharPointerKeys(Map* map);
void MapInitForInt64Keys(Map* map);
void MapInitForPointerKeys(Map* map);
void MapInitForCaseBlindStringKeys(Map* map);
void MapInitForCaseBlindCharPointerKeys(Map* map);

Map* NewMapForStringKeys(void);
Map* NewMapForCharPointerKeys(void);
Map* NewMapForInt64Keys(void);
Map* NewMapForPointerKeys(void);
Map* NewMapForCaseBlindStringKeys(void);
Map* NewMapForCaseBlindCharPointerKeys(void);

void MapDestruct(Map* map);
void MapDelete(Map* map);
void MapClear(Map* map);
void MapCopy(Map* dest, Map* src);

void MapDestructWithContents(Map* map,
                             void (*func)(MapKeyValue* kv));
void MapDeleteWithContents(Map* map,
                             void (*func)(MapKeyValue* kv));

// Removes the key from the map, returning the value being removed if the
// removal was successful. If the removal was unsuccessful (the key was not
// present) NULL is returned.  You can use the value returned to free up any
// memory used by the value if necessary.
void* MapRemove(Map* map, MapKeyType key);

// Inserts the key and value into the map.  Returns the old value if the
// insertion replaced an old value, NULL otherwise.  You can use this return
// value to free any memory used by the old value if necessary.
void* MapInsert(Map* map, MapKeyValue kv);

// Quickly shallow clone a map.  Dest must not be initialized.
void MapClone(Map* dest, Map* src);

// Finds a value given an key.  Returns NULL if it is not found.
void* MapFind(Map* map, MapKeyType key);
void* MapFindPointerKey(Map* map, void* key);
void* MapFindInt64Key(Map* map, int64_t key);

// Search the map and return NULL or pointer to MapKeyValue found.
MapValueType* MapSearch(Map* map, MapKeyType key);

void MapPrint(Map* map, void (*printer)(const MapKeyValue* kv));
void MapTraverse(Map* map,
                 void (*func)(MapKeyValue* kv, void* data),
                 void* data);

#endif /* map_h */
