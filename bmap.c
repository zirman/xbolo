/*
 *  bmap.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "bmap.h"
#include "tiles.h"
#include "terrain.h"
#include "server.h"
#include "errchk.h"

#include <string.h>
#include <assert.h>

const Recti kWorldRect = { { 0, 0 }, { 256, 256 } };
const Recti kSeaRect = { { 10, 10 }, { 236, 236 } };

/* client functions */

static int readnibble(const void *buf, size_t i);
static void writenibble(void *buf, size_t i, int nibble);

static int defaulttile(int x, int y);

int readrun(size_t *y, size_t *x, struct BMAP_Run *run, void *data, int terrain[][WIDTH]) {
  int nibs, len, i, retval;

TRY
  while (*y < WIDTH) {  /* find the beginning of a run */
    while (*x < WIDTH) {
      if (terraintotile(terrain[*y][*x]) != defaulttile(*x, *y)) {
        nibs = 0;
        run->y = *y;
        run->startx = *x;

        do {
          /* read the run */
          if (*x + 1 < WIDTH && terraintotile(terrain[*y][*x + 1]) == terraintotile(terrain[*y][*x])) {  /* sequence of like tiles */
            for (len = 2; *x + len < WIDTH && len < 9 && terraintotile(terrain[*y][*x + len]) == terraintotile(terrain[*y][*x]); len++);

            writenibble(data, nibs++, len + 6);
            writenibble(data, nibs++, terraintotile(terrain[*y][*x]));
          }
          else {  /* sequence of different tiles */
            len = 1;

            while (
              (*x + len < WIDTH) && (len < 8) &&
              (terraintotile(terrain[*y][*x + len]) != defaulttile(*x + len, *y)) &&
              (terraintotile(terrain[*y][*x + len]) != terraintotile(terrain[*y][*x + len + 1]))
            ) {
              len++;
            }

            writenibble(data, nibs++, len - 1);

            for (i = 0; i < len; i++) {
              writenibble(data, nibs++, terraintotile(terrain[*y][*x + i]));
            }
          }

          *x += len;
        } while (terraintotile(terrain[*y][*x]) != defaulttile(*x, *y));

        run->endx = *x;
        run->datalen = sizeof(struct BMAP_Run) + (nibs + 1)/2;

        retval = 0;
        SUCCESS
      }

      (*x)++;
    }

    (*y)++;
    *x = 0;
  }

  /* write the last run */
  run->datalen = 4;
  run->y = 0xff;
  run->startx = 0xff;
  run->endx = 0xff;

  retval = 1;

CLEANUP
  switch (errno) {
  case 0:
    RETURN(retval)

  default:
    RETERR(-1)
  }
END
}

int writerun(struct BMAP_Run run, const void *buf, int terrain[WIDTH][WIDTH]) {
  int i;
  int x;
  int offset;
  int serverTileType;

TRY
  x = run.startx;
  offset = 0;

  while (x < run.endx) {
    int len;

    if (sizeof(struct BMAP_Run) + (offset + 2)/2 > run.datalen) LOGFAIL(ECORFILE)

    len = readnibble(buf, offset++);

    if (len >= 0 && len <= 7) {  /* this is a sequence of different tiles */
      len += 1;

      if (sizeof(struct BMAP_Run) + (offset + len + 1)/2 > run.datalen) {
        LOGFAIL(ECORFILE)
      }

      for (i = 0; i < len; i++) {
        if ((serverTileType = tiletoterrain(readnibble(buf, offset++))) == -1) {
          LOGFAIL(ECORFILE)
        }

        terrain[run.y][x++] = serverTileType;
      }
    }
    else if (len >= 8 && len <= 15) {  /* this is a sequence of like tiles */
      len -= 6;

      if (sizeof(struct BMAP_Run) + (offset + 2)/2 > run.datalen) {
        LOGFAIL(ECORFILE)
      }

      if ((serverTileType = tiletoterrain(readnibble(buf, offset++))) == -1) {
        LOGFAIL(ECORFILE)
      }

      for (i = 0; i < len; i++) {
        terrain[run.y][x++] = serverTileType;
      }
    }
    else {
      LOGFAIL(ECORFILE)
    }
  }

  if (sizeof(struct BMAP_Run) + (offset + 1)/2 != run.datalen) {
    LOGFAIL(ECORFILE)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int readnibble(const void *buf, size_t i) {
  return i%2 ? *(uint8_t *)(buf + i/2) & 0x0f : (*(uint8_t *)(buf + i/2) & 0xf0) >> 4;
}

void writenibble(void *buf, size_t i, int nibble) {
  *(uint8_t *)(buf + i/2) = i%2 ? (*(uint8_t *)(buf + i/2) & 0xf0) ^ (((uint8_t)nibble) & 0x0f) : (*(uint8_t *)(buf + i/2) & 0x0f) ^ ((((uint8_t)nibble) & 0x0f) << 4);
}

int defaultterrain(int x, int y) {
  return (y >= Y_MIN_MINE && y <= Y_MAX_MINE && x >= X_MIN_MINE && x <= X_MAX_MINE) ? kSeaTerrain : kMinedSeaTerrain;
}

int defaulttile(int x, int y) {
  return (y >= Y_MIN_MINE && y <= Y_MAX_MINE && x >= X_MIN_MINE && x <= X_MAX_MINE) ? kSeaTile : kMinedSeaTile;
}

int terraintotile(int terrain) {
  switch (terrain) {
  case kSeaTerrain:  /* sea */
    return kSeaTile;

  case kWallTerrain:  /* wall */
    return kWallTile;

  case kRiverTerrain:  /* river */
    return kRiverTile;

  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
    return kSwampTile;

  case kCraterTerrain:  /* crater */
    return kCraterTile;

  case kRoadTerrain:  /* road */
    return kRoadTile;

  case kForestTerrain:  /* forest */
    return kForestTile;

  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
    return kRubbleTile;

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
    return kGrassTile;

  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
    return kDamagedWallTile;

  case kBoatTerrain:  /* river w/boat */
    return kBoatTile;

  case kMinedSeaTerrain:  /* mined sea */
    return kMinedSeaTile;

  case kMinedSwampTerrain:  /* mined swamp */
    return kMinedSwampTile;

  case kMinedCraterTerrain:  /* mined crater */
    return kMinedCraterTile;

  case kMinedRoadTerrain:  /* mined road */
    return kMinedRoadTile;

  case kMinedForestTerrain:  /* mined forest */
    return kMinedForestTile;

  case kMinedRubbleTerrain:  /* mined rubble */
    return kMinedRubbleTile;

  case kMinedGrassTerrain:  /* minded grass */
    return kMinedGrassTile;

  default:
    assert(0);
    return -1;
  }
}
