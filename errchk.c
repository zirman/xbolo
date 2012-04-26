/*
 *  errchk.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "errchk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct TErrNode {
  struct TErrNode *next;
  pthread_t thread;
  size_t used;
  size_t size;
  struct LineInfo {
    char file[64];
    char function[64];
    size_t line;
  } *stack;
} ;

static struct TErrNode *top = NULL;
static pthread_mutex_t mutex;

static struct TErrNode *getnode();
static void errchkinit(void) __attribute__ ((constructor));

void errchkinit(void) {
  pthread_mutex_init(&mutex, NULL);
}

struct TErrNode *getnode() {
  pthread_t thread;
  struct TErrNode *node;

  thread = pthread_self();

  for (node = top; node != NULL; node = node->next) {
    if (node->thread == thread) {
      return node;
    }
  }

  assert((node = (struct TErrNode *)malloc(sizeof(struct TErrNode))) != NULL);
  node->thread =  thread;
  node->next = top;
  node->used = 0;
  node->size = 1;
  assert((node->stack =
    (struct LineInfo *)malloc(node->size*sizeof(struct LineInfo))) != NULL);

  return node;
}

void errchkcleanup() {
  pthread_t thread;
  struct TErrNode **prev, *node;

  thread = pthread_self();

  pthread_mutex_lock(&mutex);

  for (prev = &top, node = top; node != NULL; prev = &node->next, node = node->next) {
    if (pthread_equal(node->thread, thread)) {
      *prev = node->next;
      free(node->stack);
      free(node);
    }
  }

  pthread_mutex_unlock(&mutex);
}

void pushlineinfo(const char *file, const char *function, size_t line) {
  struct TErrNode *node;

  pthread_mutex_lock(&mutex);

  node = getnode();

  if (node->used + 1 > node->size) {
    node->size *= 2;
    assert((node->stack = realloc(node->stack, node->size*sizeof(struct LineInfo))) != NULL);
  }

  strncpy(node->stack[node->used].file, file, sizeof(node->stack[node->used].file) - 1);
  strncpy(node->stack[node->used].function, function, sizeof(node->stack[node->used].function) - 1);
  node->stack[node->used].line = line;
  node->used++;

  pthread_mutex_unlock(&mutex);
}

void printlineinfo() {
  struct TErrNode *node;
  size_t i;

  pthread_mutex_lock(&mutex);

  node = getnode();
  assert(fprintf(stderr, "Error Trace:\n") >= 0);

  for (i = 0; i < node->used; i++) {
    assert(fprintf(stderr, "file:%s:%s:%ld\n", node->stack[i].file,
                   node->stack[i].function, node->stack[i].line) >= 0);
  }

  pthread_mutex_unlock(&mutex);
}
