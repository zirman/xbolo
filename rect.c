/*
 *  rect.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/11/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "rect.h"
#include "errchk.h"

Pointi makepoint(int x, int y) {
  Pointi p;
  p.x = x;
  p.y = y;
  return p;
}

int isequalpoint(Pointi p1, Pointi p2) {
  return p1.x == p2.x && p1.y == p2.y;
}

Rangei makerange(int origin, unsigned size) {
  Rangei n;
  n.origin = origin;
  n.size = size;
  return n;
}

int intersectsrange(Rangei r1, Rangei r2) {
  if (r1.origin < r2.origin) {
    if (r1.origin + r1.size > r2.origin) {
      return 1;
    }
  }
  else {
    if (r2.origin + r2.size > r1.origin) {
      return 1;
    }
  }

  return 0;
}

int containsrange(Rangei r1, Rangei r2) {
  return r1.origin <= r2.origin && r1.origin + r1.size >= r2.origin + r2.size;
}

int inrange(Rangei r, int x) {
  return r.origin <= x && r.origin + r.size < x;
}

Sizei makesize(unsigned w, unsigned h) {
  Sizei s;
  s.width = w;
  s.height = h;
  return s;
}

int equalsizes(Sizei s1, Sizei s2) {
  return s1.width == s2.width && s1.height == s2.height;
}

Recti makerect(int x, int y, unsigned w, unsigned h) {
  Recti r;
  r.origin.x = x;
  r.origin.y = y;
  r.size.width = w;
  r.size.height = h;
  return r;
}

unsigned heightrect(Recti r) {
  return r.size.height;
}

unsigned widthrect(Recti r) {
  return r.size.width;
}

int maxxrect(Recti r) {
  return r.origin.x + r.size.width - 1;
}

int maxyrect(Recti r) {
  return r.origin.y + r.size.height - 1;
}

int midxrect(Recti r) {
  return r.origin.x + (r.size.width/2);
}

int midyrect(Recti r) {
  return r.origin.y + (r.size.height/2);
}

int minxrect(Recti r) {
  return r.origin.x;
}

int minyrect(Recti r) {
  return r.origin.y;
}

Recti offsetrect(Recti r, int dx, int dy) {
  Recti n;
  n.origin.x = r.origin.x + dx;
  n.origin.y = r.origin.y + dy;
  n.size.width = r.size.width;
  n.size.height = r.size.height;
  return n;
}

int ispointinrect(Recti r, Pointi p) {
  return minxrect(r) <= p.x && minyrect(r) <= p.y && maxxrect(r) >= p.x && maxyrect(r) >= p.y;
}

Recti unionrect(Recti r1, Recti r2) {
  Recti n;
  n.origin.x = r1.origin.x < r2.origin.x ? r1.origin.x : r2.origin.x;
  n.origin.y = r1.origin.y < r2.origin.y ? r1.origin.y : r2.origin.y;
  n.size.width =
    (r1.origin.x + r1.size.width > r2.origin.x + r2.size.width ? r1.origin.x + r1.size.width : r2.origin.x + r2.size.width) - n.origin.x;
  n.size.height =
    (r1.origin.y + r1.size.height > r2.origin.y + r2.size.height ? r1.origin.y + r1.size.height : r2.origin.y + r2.size.height) - n.origin.y;
  return n;
}

int containsrect(Recti r1, Recti r2) {
  return
    containsrange(makerange(r1.origin.x, r1.size.width), makerange(r2.origin.x, r2.size.width)) &&
    containsrange(makerange(r1.origin.y, r1.size.height), makerange(r2.origin.y, r2.size.height));
}

int isequalrect(Recti r1, Recti r2) {
  return r1.origin.x == r2.origin.x && r1.origin.y == r2.origin.y && r1.size.width == r2.size.width && r1.size.height == r2.size.height;
}

int isemptyrect(Recti r) {
  return r.size.width <= 0 || r.size.height <= 0;
}

Recti insetrect(Recti r, int dx, int dy) {
  Recti n;
  n.origin.x = r.origin.x + dx;
  n.origin.y = r.origin.y + dy;
  n.size.width = r.size.width - dx*2;
  n.size.height = r.size.height - dy*2;
  return n;
}

Recti intersectionrect(Recti r1, Recti r2) {
  Recti n;
  n.origin.x = r1.origin.x > r2.origin.x ? r1.origin.x : r2.origin.x;
  n.origin.y = r1.origin.y > r2.origin.y ? r1.origin.y : r2.origin.y;
  n.size.width =
    (r1.origin.x + r1.size.width < r2.origin.x + r2.size.width ? r1.origin.x + r1.size.width : r2.origin.x + r2.size.width) - n.origin.x;
  n.size.height =
    (r1.origin.y + r1.size.height < r2.origin.y + r2.size.height ? r1.origin.y + r1.size.height : r2.origin.y + r2.size.height) - n.origin.y;
  return n;
}

int intersectsrect(Recti r1, Recti r2) {
  return 
    intersectsrange(makerange(r1.origin.x, r1.size.width), makerange(r2.origin.x, r2.size.width)) &&
    intersectsrange(makerange(r1.origin.y, r1.size.height), makerange(r2.origin.y, r2.size.height));
}

void splitrect(Recti r, int x, int y, Recti *rects) {
  rects[0] = makerect(r.origin.x, r.origin.y, x - r.origin.x, y - r.origin.y);
  rects[1] = makerect(x, r.origin.y, (r.origin.x + r.size.width) - x, y - r.origin.y);
  rects[2] = makerect(r.origin.x, y, x - r.origin.x, (r.origin.y + r.size.height) - y);
  rects[3] = makerect(x, y, (r.origin.x + r.size.width) - x, (r.origin.y + r.size.height) - y);
}

void subtractrect(Recti r1, Recti r2, Recti *rects) {
  int minx, miny, maxx, maxy;
  int lxly, lxhy, hxly, hxhy;
//  int c;

  minx = r2.origin.x;
  miny = r2.origin.y;
  maxx = r2.origin.x + r2.size.width;
  maxy = r2.origin.y + r2.size.height;

  lxly = ispointinrect(r1, makepoint(minx - 1, miny - 1));
  lxhy = ispointinrect(r1, makepoint(minx - 1, maxy));
  hxly = ispointinrect(r1, makepoint(maxx, miny - 1));
  hxhy = ispointinrect(r1, makepoint(maxx, maxy));

  if (lxly && !lxhy && !hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(r1.origin.x, miny, minx - r1.origin.x, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(0, 0, 0, 0);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (!lxly && lxhy && !hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, minx - r1.origin.x, r1.size.height);
    rects[1] = makerect(minx, maxy, (r1.origin.x + r1.size.width) - minx, (r1.origin.y + r1.size.height) - maxy);
    rects[2] = makerect(0, 0, 0, 0);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (!lxly && !lxhy && hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(maxx, miny, (r1.origin.x + r1.size.width) - maxx, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(0, 0, 0, 0);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (!lxly && !lxhy && !hxly && hxhy) {
    rects[0] = makerect(maxx, r1.origin.y, (r1.origin.x + r1.size.width) - maxx, r1.size.height);
    rects[1] = makerect(r1.origin.x, maxy, maxx - r1.origin.x, (r1.origin.y + r1.size.height) - maxy);
    rects[2] = makerect(0, 0, 0, 0);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (lxly && !lxhy && hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(r1.origin.x, miny, minx - r1.origin.x, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(maxx, miny, (r1.origin.x + r1.size.width) - maxx, (r1.origin.y + r1.size.height) - miny);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (!lxly && !lxhy && hxly && hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(maxx, miny, (r1.origin.x + r1.size.width) - maxx, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(r1.origin.x, maxy, maxx - r1.origin.x, (r1.origin.y + r1.size.height) - maxy);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (!lxly && lxhy && !hxly && hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, minx - r1.origin.x, r1.size.height);
    rects[1] = makerect(maxx, r1.origin.y, (r1.origin.x + r1.size.width) - maxx, r1.size.height);
    rects[2] = makerect(minx, maxy, maxx - minx, (r1.origin.y + r1.size.height) - maxy);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (lxly && lxhy && !hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(r1.origin.x, miny, minx - r1.origin.x, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(minx, maxy, (r1.origin.x + r1.size.width) - minx, (r1.origin.y + r1.size.height) - maxy);
    rects[3] = makerect(0, 0, 0, 0);
  }
  else if (lxly && lxhy && hxly && hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, maxx - r1.origin.x, miny - r1.origin.y);
    rects[1] = makerect(r1.origin.x, miny, minx - r1.origin.x, (r1.origin.y + r1.size.height) - miny);
    rects[2] = makerect(minx, maxy, (r1.origin.x + r1.size.width) - minx, (r1.origin.y + r1.size.height) - maxy);
    rects[3] = makerect(maxx, r1.origin.y, (r1.origin.x + r1.size.width) - maxx, maxy - r1.origin.y);
  }
  else if (!lxly && !lxhy && !hxly && !hxhy) {
    rects[0] = makerect(r1.origin.x, r1.origin.y, r1.size.width, miny - r1.origin.y);
    rects[1] = makerect(r1.origin.x, r1.origin.y, minx - r1.origin.x, r1.size.height);
    rects[2] = makerect(r1.origin.x, maxy, r1.size.width, (r1.origin.y + r1.size.height) - maxy);
    rects[3] = makerect(maxx, r1.origin.y, (r1.origin.x + r1.size.width) - maxx, r1.size.height);
  }
}
