//
//  list.c
//  c_compiler
//
//  Created by David Allison on 12/20/17.
//  Copyright © 2017 David Allison. All rights reserved.
//

#include "list.h"
#include <assert.h>
#include <stdlib.h>

void ListElementInit(ListElement* element) {
  element->prev = NULL;
  element->next = NULL;
}

void ListInit(List* list) {
  list->first = NULL;
  list->last = NULL;
  list->length = 0;
}

List* NewList() {
  List* list = malloc(sizeof(List));
  ListInit(list);
  return list;
}

void ListDestruct(List* list) {
  ListElement* e = list->first;
  while (e != NULL) {
    ListElement* next = e->next;
    free(e);
    e = next;
  }
  list->first = list->last = NULL;
}

void ListDelete(List* list) {
  ListDestruct(list);
  free(list);
}

void ListAppend(List* list, ListElement* e) {
  assert(e->prev == NULL);
  assert(e->next == NULL);
  if (list->last == NULL) {
    list->first = list->last = e;
  } else {
    e->prev = list->last;
    list->last->next = e;
    list->last = e;
  }
  list->length++;
}

void ListInsertBefore(List* list, ListElement* e, ListElement* pos) {
  e->next = pos;
  if (pos->prev == NULL) {
    list->first = e;
  } else {
    pos->prev->next = e;
  }
  e->prev = pos->prev;
  pos->prev = e;
  list->length++;
}

void ListInsertAfter(List* list, ListElement* e, ListElement* pos) {
  if (pos == NULL) {
    ListAppend(list, e);
    return;
  }
  e->prev = pos;
  if (pos->next == NULL) {
    list->last = e;
  } else {
    pos->next->prev = e;
  }
  e->next = pos->next;
  pos->next = e;
  list->length++;
}

void ListDeleteElement(List* list, ListElement* e) {
  if (e->prev == NULL) {
    list->first = e->next;
  } else {
    e->prev->next = e->next;
  }
  if (e->next == NULL) {
    list->last = e->prev;
  } else {
    e->next->prev = e->prev;
  }
  list->length--;
}

void ListTraverse(List* list, void (*func)(ListElement*, void*), void* data) {
  ListElement* e = list->first;
  while (e != NULL) {
    ListElement* next = e->next;
    func(e, data);
    e = next;
  }
}

ListElement* ListFind(List* list, const void* value,
                      int (*compare)(const void*, const void*)) {
  ListElement* e = list->first;
  while (e != NULL) {
    ListElement* next = e->next;
    if (compare(value, e) == 0) {
      return e;
    }
    e = next;
  }
  return NULL;
}

void ListCopy(List* to, List* from,
              ListElement* (*copy_func)(ListElement* from)) {
  ListElement* e = from->first;
  while (e != NULL) {
    ListElement* next = e->next;
    ListElement* new = copy_func(e);
    ListAppend(to, new);
    e = next;
  }
}
