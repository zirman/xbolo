/*
 *  list.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/7/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */
 
#ifndef __LIST__
#define __LIST__

struct ListNode {
  struct ListNode *prev, *next;
  void *ptr;
} ;

int initlist(struct ListNode *list);
int addlist(struct ListNode *list, void *ptr);
struct ListNode *removelist(struct ListNode *node, void (*releasefunc)(void *));
void clearlist(struct ListNode *list, void (*releasefunc)(void *));
struct ListNode *nextlist(struct ListNode *node);
struct ListNode *prevlist(struct ListNode *node);
void *ptrlist(struct ListNode *node);

#endif  /* LIST */
