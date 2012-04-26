/*
 *  rect.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/11/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __RECT__
#define __RECT__

typedef struct Pointi {
  int x;
  int y;
} Pointi;

typedef struct Rangei {
  int origin;
  int size;
} Rangei;

typedef struct Sizei {
  int width;
  int height;
} Sizei;

typedef struct Recti {
  Pointi origin;
  Sizei size;
} Recti;

Pointi makepoint(int x, int y);
int isequalpoint(Pointi p1, Pointi p2);

Rangei makerange(int origin, unsigned size);
int intersectsrange(Rangei r1, Rangei r2);
int containsrange(Rangei r1, Rangei r2);
int inrange(Rangei r, int x);

Sizei makesize(unsigned width, unsigned height);
int equalsizes(Sizei s1, Sizei s2);

Recti makerect(int x, int y, unsigned w, unsigned h);
unsigned heightrect(Recti r);
unsigned widthrect(Recti r);
int maxxrect(Recti r);
int maxyrect(Recti r);
int midxrect(Recti r);
int midyrect(Recti r);
int minxrect(Recti r);
int minyrect(Recti r);
Recti offsetrect(Recti r, int dx, int dy);
int ispointinrect(Recti r, Pointi p);
Recti unionrect(Recti r1, Recti r2);
int containsrect(Recti r1, Recti r2);
int isequalrect(Recti r1, Recti r2);
int isemptyrect(Recti r);
Recti insetrect(Recti r, int dx, int dy);
Recti intersectionrect(Recti r1, Recti r2);
int intersectsrect(Recti r1, Recti r2);
void splitrect(Recti r, int x, int y, Recti *rects);
void subtractrect(Recti r1, Recti r2, Recti *rects);

//Recti dividerect(Recti inRecti, Recti *slice, Recti *remainder, float amount, NSRectiEdge edge);

#endif  /* __RECT__ */
