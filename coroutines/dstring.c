//
//  dstring.c
//  c_compiler
//
//  Created by David Allison on 10/26/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "dstring.h"
#include "vector.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void LazyInit(String* str) {
  if (str == NULL) {
    return;
  }
  if (str->value == NULL) {
    str->value = str->buffer;
    str->length = 0;
    str->capacity = STRING_BUFFER_SIZE;
  }
}

void StringInitFromSegment(String* str, const char* init, size_t length) {
  size_t real_length = length + 1;  // With \0 at the end.
  if (real_length > STRING_BUFFER_SIZE) {
    // String too long for buffer, allocate using heap.
    str->value = malloc(real_length);
    str->capacity = real_length;
  } else {
    str->value = str->buffer;
    str->capacity = STRING_BUFFER_SIZE;
  }
  str->length = length;  // Length doesn't include zero at end.
  if (init != NULL) {
    memcpy(str->value, init, length);
    str->value[length] = '\0';  // Terminate with \0.
  } else {
    str->value[0] = '\0';
  }
}

void StringInit(String* str, const char* init) {
  size_t length = {init == NULL ? 0 : strlen(init)};
  StringInitFromSegment(str, init, length);
}

void StringInitImmutable(String* str, const char* init) {
  size_t length = init == NULL ? 0 : strlen(init);
  str->value = (char*)init;
  str->capacity = STRING_IMMUTABLE;
  str->length = length;
}

static void CheckMutable(String* s) {
  LazyInit(s);
  assert(s->capacity != STRING_IMMUTABLE);
}

void StringClear(String* str) {
  CheckMutable(str);
  if (str->value != str->buffer) {
    free(str->value);
    str->value = str->buffer;
    str->capacity = STRING_BUFFER_SIZE;
  }
  str->length = 0;
  str->value[0] = '\0';
}

String* NewString(const char* init) {
  String* s = malloc(sizeof(String));
  StringInit(s, init);
  return s;
}

String* NewStringWithLength(const char* init, size_t length) {
  String* s = calloc(sizeof(String), 1);
  StringAppendSegment(s, init, length);
  return s;
}

String* NewEmptyString(void) { return NewString(NULL); }

void StringDestruct(String* str) {
  if (str->capacity != STRING_IMMUTABLE && str->value != str->buffer) {
    free(str->value);
  }
  str->value = NULL;
  str->length = 0;
  str->capacity = 0;
}

void StringDelete(String* str) {
  CheckMutable(str);
  if (str->value != str->buffer) {
    free(str->value);
  }
  free(str);
}

char StringCharAt(String* str, size_t index) {
  LazyInit(str);
  return str->value[index];
}

// Comparion.
bool StringEqual(String* str1, const char* str2) {
  LazyInit(str1);
  return str1 != NULL && strcmp(str1->value, str2) == 0;
}

bool StringEqualCaseBlind(String* str1, const char* str2) {
  LazyInit(str1);
  return str1 != NULL && strcasecmp(str1->value, str2) == 0;
}

int StringCompare(String* str1, const char* str2) {
  LazyInit(str1);
  return strcmp(str1->value, str2);
}

int StringCompareCaseBlind(String* str1, const char* str2) {
  LazyInit(str1);
  return strcasecmp(str1->value, str2);
}

bool StringEqualString(String* str1, String* str2) {
  LazyInit(str1);
  LazyInit(str2);
  return strcmp(str1->value, str2->value) == 0;
}

int StringCompareString(String* str1, String* str2) {
  LazyInit(str1);
  LazyInit(str2);
  return strcmp(str1->value, str2->value);
}

int StringCompareStringCaseBlind(String* str1, String* str2) {
  LazyInit(str1);
  LazyInit(str2);
  return strcasecmp(str1->value, str2->value);
}

// Set and append.
void StringSet(String* str, const char* value) {
  CheckMutable(str);
  size_t length =
      value == NULL ? 1 : strlen(value) + 1;  // Length of value + 1.
  if (str->capacity < length) {
    if (str->value == str->buffer) {
      // Currently in buffer, need to use malloc
      str->value = malloc(length);
    } else {
      // No space for new value, reallocate memory.
      str->value = realloc(str->value, length);
    }
    str->capacity = length;
  }
  if (value != NULL) {
    strcpy(str->value, value);
  }
  str->length = length - 1;
}

void StringSetString(String* str, String* value) {
  LazyInit(value);
  StringSet(str, value->value);
}

void StringAppendSegment(String* str, const char* value, size_t length) {
  CheckMutable(str);
  // Full length of strings, including \0.
  size_t full_length = str->length + length + 1;
  if (str->capacity < full_length) {
    if (str->value == str->buffer) {
      // Moving from buffer, allocate memory and copy the buffer in to it.
      str->value = malloc(full_length);
      memcpy(str->value, str->buffer, STRING_BUFFER_SIZE);
    } else {
      // No space, reallocate memory.
      str->value = realloc(str->value, full_length);
    }
    str->capacity = full_length;
  }

  // Copy in new string at the end of the current one.
  memcpy(str->value + str->length, value, length);
  str->value[str->length + length] = '\0';
  str->length = full_length - 1;
}

void StringAppend(String* str, const char* value) {
  LazyInit(str);
  // Get length of value (without \0).
  size_t value_length = value == NULL ? 0 : strlen(value);
  StringAppendSegment(str, value, value_length);
}

void StringAppendString(String* str, String* value) {
  LazyInit(value);
  StringAppend(str, value->value);
}

void StringTrimEnd(String* s) {
  CheckMutable(s);
  while (s->length > 0) {
    if (isspace(s->value[s->length - 1])) {
      s->length--;
    } else {
      break;
    }
  }
  s->value[s->length] = '\0';
}

void StringTrimStart(String* s) {
  CheckMutable(s);
  size_t first_non_space = 0;
  while (first_non_space < s->length) {
    if (!isspace(s->value[first_non_space])) {
      break;
    }
    first_non_space++;
  }
  if (first_non_space == s->length) {
    StringClear(s);
    return;
  }
  StringReplace(s, 0, first_non_space, "", 0);
}

void StringTrim(String* s) {
  StringTrimStart(s);
  StringTrimEnd(s);
}

// This needs to be optimal since code that build up strings tends
// to call it a lot.
void StringAppendChar(String* str, char ch) {
  CheckMutable(str);
  // We are appending a single char but the memory needs a '\0'
  // at the end and this is not included in str->length, so
  // the actual length we need is str->length + 2.
  size_t new_length = str->length + 2;
  if (str->capacity < new_length) {
    // Out of space, reallocate memory.
    // If we are appending chars we don't want to keep reallocing
    // the memory for each additional char.  So double the amount of
    // memory needed each time we grow.
    new_length *= 2;

    if (str->value == str->buffer) {
      // Moving from buffer, allocate memory and copy the buffer in to it.
      str->value = malloc(new_length);
      memcpy(str->value, str->buffer, STRING_BUFFER_SIZE);
    } else {
      // No space in heap-allocated memory, reallocate memory.
      str->value = realloc(str->value, new_length);
    }
    str->capacity = new_length;
  }
  // Append the character and then '\0'.
  str->value[str->length++] = ch;
  str->value[str->length] = '\0';
}

void StringReplace(String* str, size_t pos, size_t len, const char* p,
                   size_t plen) {
  CheckMutable(str);
  ssize_t len_diff = plen - len;
  size_t new_length = str->length + len_diff + 1;
  if (str->capacity < new_length) {
    // Out of space, reallocate memory.
    // If we are appending chars we don't want to keep reallocing
    // the memory for each additional char.  So double the amount of
    // memory needed each time we grow.
    new_length *= 2;

    if (str->value == str->buffer) {
      // Moving from buffer, allocate memory and copy the buffer in to it.
      str->value = malloc(new_length);
      memcpy(str->value, str->buffer, STRING_BUFFER_SIZE);
    } else {
      // No space, reallocate memory.
      str->value = realloc(str->value, new_length);
    }
    str->capacity = new_length;
  }

  // Move tail up or down in memory
  size_t tail_length = str->length - (pos + len) + 1;
  if (tail_length > 0) {
    memmove(&str->value[pos + plen], &str->value[pos + len], tail_length);
  }
  // Copy in new string.
  if (plen > 0) {
    memcpy(&str->value[pos], p, plen);
  }
  str->length += len_diff;
}

void StringReplaceString(String* str, size_t pos, size_t len, String* p) {
  StringReplace(str, pos, len, p->value, p->length);
}

void StringErase(String* str, size_t pos, size_t len) {
  StringReplace(str, pos, len, NULL, 0);
}

size_t StringIndexOf(String* s, const char* substring) {
  LazyInit(s);
  char* pos = strstr(s->value, substring);
  if (pos == NULL) {
    return (size_t)-1;
  }
  return pos - s->value;
}

// Look for a substring, backwards in the file.
size_t StringLastIndexOf(String* s, const char* substring) {
  LazyInit(s);
  size_t len = strlen(substring);
  size_t index = s->length - len;
  while (index > 0) {
    if (strncmp(s->value + index, substring, len) == 0) {
      return index;
    }
    index--;
  }
  // Check for initial substring.
  return strncmp(s->value, substring, len) == 0 ? 0 : -1;
}

void StringSubstring(String* s, size_t start, size_t length, String* out) {
  LazyInit(s);
  if (start >= s->length) {
    return;
  }
  // Limit length to length of string from start position.
  if (start + length >= s->length) {
    length = s->length - start;
  }
  StringClear(out);
  StringAppendSegment(out, s->value + start, length);
}

bool StringStartsWith(String* s, const char* prefix) {
  LazyInit(s);
  return strstr(s->value, prefix) == s->value;
}

bool StringEndsWith(String* s, const char* suffix) {
  LazyInit(s);
  size_t len = strlen(suffix);
  if (len > s->length) {
    return false;
  }
  return strcmp(s->value + s->length - len, suffix) == 0;
}

void StringPrintf(String* str, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringVPrintf(str, format, ap);
  va_end(ap);
}

void StringVPrintf(String* str, const char* format, va_list ap) {
  LazyInit(str);
  char buf[1024];
  vsnprintf(buf, sizeof(buf), format, ap);
  StringAppend(str, buf);
}

bool StringContainsChar(String* str, char ch) {
  LazyInit(str);
  return strchr(str->value, ch) != NULL;
}

bool StringContainsString(String* str, const char* s) {
  LazyInit(str);
  return strstr(str->value, s) != NULL;
}

void StringEscape(String* in, String* out) {
  LazyInit(in);
  for (size_t i = 0; i < in->length; i++) {
    char ch = in->value[i];
    if (ch < ' ' || ch >= 127) {
      char escape_char = '\0';
      switch (ch) {
        case '\n':
          escape_char = 'n';
          break;
        case '\a':
          escape_char = 'a';
          break;
        case '\r':
          escape_char = 'r';
          break;
        case '\f':
          escape_char = 'f';
          break;
        case '\t':
          escape_char = 't';
          break;
        case '\b':
          escape_char = 'b';
          break;
        case '\v':
          escape_char = 'v';
          break;
      }
      if (escape_char != '\0') {
        StringPrintf(out, "\\%c", escape_char);
      } else {
        StringPrintf(out, "\\%x", ch);
      }
    } else {
      switch (ch) {
        case '\'':
        case '"':
          StringPrintf(out, "\\%c", ch);
          break;
        case '\\':
          StringPrintf(out, "\\\\");
          break;
        default:
          StringAppendChar(out, ch);
      }
    }
  }
}

void StringSplit(String* s, char sep, Vector* v) {
  LazyInit(s);
  size_t i = 0;
  while (i < s->length) {
    size_t start = i;

    while (i < s->length && s->value[i] != sep) {
      i++;
    }
    String* part = NewString(NULL);
    StringAppendSegment(part, &s->value[start], i - start);
    VectorAppend(v, part);
    i++;  // Skip separator.
  }
}
