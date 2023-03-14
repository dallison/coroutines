//
//  list.h
//  c_compiler
//
//  Created by David Allison on 12/20/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#ifndef list_h
#define list_h

#include <stdio.h>

// This is an invasive list.  Each element in the list must have a ListElement
// struct at offset 0.

// A list element header.  This should be placed at the very beginning
// of any type to be inserted into a list
typedef struct ListElement {
  struct ListElement* prev;
  struct ListElement* next;
} ListElement;

// Initializes a list element.  Call this to initialize the header before
// inserting into the list.
void ListElementInit(ListElement* element);

typedef struct List {
  ListElement* first;
  ListElement* last;
  size_t length;  // Number of elements in the list.
} List;

void ListInit(List* list);
List* NewList(void);
void ListDestruct(List* list);
void ListDelete(List* list);

// Appends element e at the end of the list.
void ListAppend(List* list, ListElement* e);

// Inserts element e just before element pos in the list.
void ListInsertBefore(List* list, ListElement* e, ListElement* pos);

// Inserts element e just after element pos in the list.
void ListInsertAfter(List* list, ListElement* e, ListElement* pos);

// Deletes element e from the list.  The element is still intact after
// deletion.
void ListDeleteElement(List* list, ListElement* e);

// Traverses the list calling the function 'func' for every element.
void ListTraverse(List* list, void (*func)(ListElement*, void*), void* data);

// Finds an element in the list given a comparison function.  The function
// should return 0 if the value matches.  The first argument passed to the
// compare function is the value to find; the second is a pointer to a
// ListElement to compare against.
ListElement* ListFind(List* list, const void* value,
                      int (*compare)(const void*, const void*));

void ListCopy(List* to, List* from,
              ListElement* (*copy_func)(ListElement* from));

#endif /* list_h */
