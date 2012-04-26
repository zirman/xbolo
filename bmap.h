/*
 *  bmap.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __BMAP__
#define __BMAP__

#include "rect.h"
#include "bolo.h"

#include <stdint.h>

struct BOLO_Preamble {
  uint8_t ident[8];  /* "XBOLOGAM" */
  uint8_t version;   /* currently 0 */
  uint8_t player;    /* your player id */
  uint8_t hiddenmines;
  uint8_t pause;
  uint8_t gametype;

  union {
    struct {
      uint8_t type;
      uint8_t basecontrol;
    } domination;
  } __attribute__((__packed__)) game;

  struct {
    uint8_t used;
    uint8_t connected;
    uint32_t seq;
    uint8_t name[MAXNAME];
    uint8_t host[MAXHOST];
    uint16_t alliance;
  } __attribute__((__packed__)) players[MAXPLAYERS];
  uint32_t maplen;   /* the map length */
} __attribute__((__packed__));

struct BMAP_Preamble {
  uint8_t ident[8];  /* "BMAPBOLO" */
  uint8_t version;   /* currently 0 */
  uint8_t npills;    /* maximum 16 (at the moment) */
  uint8_t nbases;    /* maximum 16 (at the moment) */
  uint8_t nstarts;   /* maximum 16 (at the moment) */
} __attribute__((__packed__));

struct BMAP_PillInfo {
	uint8_t x;
	uint8_t y;
	uint8_t owner;   /* should be 0xFF except in speciality maps */
	uint8_t armour;  /* range 0-15 (dead pillbox = 0, full strength = 15) */
	uint8_t speed;   /* typically 50. Time between shots, in 20ms units */
                   /* Lower values makes the pillbox start off 'angry' */
} __attribute__((__packed__));

struct BMAP_BaseInfo {
	uint8_t x;
	uint8_t y;
	uint8_t owner;   /* should be 0xFF except in speciality maps */
	uint8_t armour;  /* initial stocks of base. Maximum value 90 */
	uint8_t shells;  /* initial stocks of base. Maximum value 90 */
	uint8_t mines;   /* initial stocks of base. Maximum value 90 */
} __attribute__((__packed__));

struct BMAP_StartInfo {
  uint8_t x;
  uint8_t y;
	uint8_t dir;  /* Direction towards land from this start. Range 0-15 */
} __attribute__((__packed__));

struct BMAP_Run {
	uint8_t datalen;  /* length of the data for this run */
                    /* INCLUDING this 4 byte header */
	uint8_t y;        /* y co-ordinate of this run. */
	uint8_t startx;   /* first square of the run */
	uint8_t endx;     /* last square of run + 1 */
                    /* (ie first deep sea square after run) */
/*	uint8_t data[0xFF];*/  /* actual length of data is always much less than 0xFF */
} __attribute__((__packed__));

extern const Recti kWorldRect;
extern const Recti kSeaRect;

int readrun(size_t *y, size_t *x, struct BMAP_Run *run, void *data, int terrain[][WIDTH]);
int writerun(struct BMAP_Run run, const void *buf, int terrain[][WIDTH]);

/* returns the default terrain type for x, y */
int defaultterrain(int x, int y);

/* converts terrain to a tile type */
int terraintotile(int terrain);

#endif /* __BMAP__ */
