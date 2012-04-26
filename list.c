/*
 *  list.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/7/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "list.h"
#include "errchk.h"

#include <stdlib.h>
#include <assert.h>

int initlist(struct ListNode *list) {
  assert(list != NULL);
  list->prev = NULL;
  list->next = NULL;
  list->ptr = NULL;

TRY

CLEANUP
ERRHANDLER(0, -1)
END
}


int addlist(struct ListNode *list, void *ptr) {
  struct ListNode *node = NULL;
  assert(list != NULL);

TRY
  if ((node = (struct ListNode *)malloc(sizeof(struct ListNode))) == NULL) LOGFAIL(errno)

  node->prev = list;
  node->next = list->next;
  node->prev->next = node;

  if (node->next != NULL) {
    node->next->prev = node;
  }

  node->ptr = ptr;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (node != NULL) {
      free(node);
      node = NULL;
    }

    RETERR(-1)
  }
END
}


struct ListNode *removelist(struct ListNode *node, void (*releasefunc)(void *)) {
  struct ListNode *next;

  node->prev->next = node->next;

  if (node->next != NULL) {
    node->next->prev = node->prev;
  }

  next = node->next;

  releasefunc(node->ptr);
  free(node);

  return next;
}

void clearlist(struct ListNode *list, void (*releasefunc)(void *)) {
  struct ListNode *node, *next;

  assert(list != NULL);

  for (node = list->next; node != NULL; node = next) {
    next = node->next;
    releasefunc(node->ptr);
    free(node);
  }

  list->next = NULL;
}

struct ListNode *nextlist(struct ListNode *node) {
  assert(node != NULL);
  return node->next;
}

struct ListNode *prevlist(struct ListNode *node) {
  assert(node != NULL);
  return node->prev;
}

void *ptrlist(struct ListNode *node) {
  assert(node != NULL);
  return node->ptr;
}
