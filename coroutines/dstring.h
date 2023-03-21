//
//  dstring.h
//  c_compiler
//
//  Created by David Allison on 10/26/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef dstring_h
#define dstring_h

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

struct Vector;

// This is a struct the implements a general purpose variable
// length string.  The string is mutable and can be appended to
// or changed.  The memory is either in a small array inside the
// String struct or is allocated using malloc.  The memory
// will expand as needed as the string changes.
//
// The value of the string may not contain a NUL byte.  This makes
// it a direct mapping onto the C string functions.  If you need to put
// a zero byte in an expandable buffer, use a Buffer instead.
//
// To access the string, the following fields are available:
// length: the length of the string not including the zero byte
// value: the address of the first character in the string.  All characters
//        are contiguous.
//
// An empty string may be creaated by:
// String s = {0};
//
// To create one with an initial value on the stack.
// String s;
// StringInit(&s, "hello");
//
// When the string needs to persist beyond the function, create it in
// static memory or:
// String* s = NewEmptyString();
// or:
// String* s = NewString("foobar");
//
// A string must be destroyed when you are done with it, otherwise you
// get a memory leak.  To destroy a string on the stack or static memory:
//
// StringDestruct(&s);
//
// For one on the heap:
// StringDelete(s);
//
// Strings are always mutable, meaning that you can modify them after
// they are created.  You may also create an immutable string that can't
// be modified.  NOTE: This does not copy the value so make sure it's lifetime
// is longer than the string itself (a string literal, for example). This
// exists to avoid a malloc and memcpy for strings that never change.
//
// String s;
// StringInitImmutable("value");

#define STRING_BUFFER_SIZE 16
#define STRING_IMMUTABLE ((size_t)-1)

typedef struct {
  // Most strings are short.  This buffer will avoid heap
  // allocations for most cases.
  char buffer[STRING_BUFFER_SIZE];
  char* value;      // Current memory for string.
  size_t length;    // Length of string excluding \0.
  size_t capacity;  // Total space available.
} String;

// Initialization and allocation.

// Initializes a String to the value given.  The 'init'
// value can be NULL or a pointer to a zero-terminated
// array of chars.  The array including the zero byte at the end will be
// copied into String object.
void StringInit(String* str, const char* init);
// Allocates a new string from the heap using malloc and initializes it.
String* NewString(const char* init);
String* NewEmptyString(void);
String* NewStringWithLength(const char* init, size_t length);

// Initializes the string from a pointer to a character and the length.
void StringInitFromSegment(String* str, const char* init, size_t length);

// Initialize an immutable string from a character pointer.
void StringInitImmutable(String* str, const char* init);

// Destroys a string.
void StringDestruct(String* str);

// Deletes a string allocated using malloc.
void StringDelete(String* str);

// Get a character at the given index in the string.
char StringCharAt(String* str, size_t index);

// Comparison.
bool StringEqual(String* str1, const char* str2);
int StringCompare(String* str1, const char* str2);
bool StringEqualString(String* str1, String* str2);
int StringCompareString(String* str1, String* str2);
bool StringContainsChar(String* str, char ch);
bool StringContainsString(String* str, const char* s);

bool StringEqualCaseBlind(String* str1, const char* str2);
int StringCompareCaseBlind(String* str1, const char* str2);
int StringCompareStringCaseBlind(String* str1, String* str2);

// Set and append.
void StringSet(String* str, const char* value);
void StringSetString(String* str, String* value);
void StringAppend(String* str, const char* value);
void StringAppendString(String* str, String* value);
void StringAppendChar(String* str, char ch);
void StringAppendSegment(String* str, const char* value, size_t length);

// Printf-style setting of String contents.
void StringPrintf(String* str, const char* format, ...);
void StringVPrintf(String* str, const char* format, va_list ap);

void StringClear(String* str);

void StringEscape(String* in, String* out);
void StringTrimStart(String* s);
void StringTrimEnd(String* s);
void StringTrim(String* s);

// Returns index into s or -1.
size_t StringIndexOf(String* s, const char* substring);
size_t StringLastIndexOf(String* s, const char* substring);
void StringSubstring(String* s, size_t start, size_t length, String* out);
bool StringStartsWith(String* s, const char* prefix);
bool StringEndsWith(String* s, const char* suffix);

void StringSplit(String* s, char sep, struct Vector* v);
void StringReplace(String* str, size_t pos, size_t len, const char* p, size_t plen);
void StringReplaceString(String* str, size_t pos, size_t len, String* p);
void StringErase(String* str, size_t pos, size_t len);

#endif /* string_h */
