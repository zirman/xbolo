/*
 *  bmap_server.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "bmap_server.h"
#include "server.h"
#include "terrain.h"
#include "errchk.h"

#include <stdlib.h>
#include <string.h>

/* server functions */
static ssize_t serverloadmapsize();

int serverloadmap(const void *buf, size_t nbytes) {
  int i, x, y;
  const struct BMAP_Preamble *preamble;
  const struct BMAP_PillInfo *pillInfos;
  const struct BMAP_BaseInfo *baseInfos;
  const struct BMAP_StartInfo *startInfos;
  const void *runData;
  int runDataLen;
  int offset;

TRY
  /* wipe the map clean */
  for (y = 0; y < WIDTH; y++) {
    for (x = 0; x < WIDTH; x++) {
      server.terrain[y][x] = defaultterrain(x, y);
    }
  }

  if (nbytes < sizeof(struct BMAP_Preamble)) LOGFAIL(ECORFILE)

  preamble = buf;

  if (strncmp((char *)preamble->ident, MAP_FILE_IDENT, MAP_FILE_IDENT_LEN) != 0)
    LOGFAIL(ECORFILE)

  if (preamble->version != CURRENT_MAP_VERSION) LOGFAIL(EINCMPAT)

  if (preamble->npills > MAXPILLS) LOGFAIL(ECORFILE)

  if (preamble->nbases > MAXBASES) LOGFAIL(ECORFILE)

  if (preamble->nstarts > MAX_STARTS) LOGFAIL(ECORFILE)

  if (nbytes <
      sizeof(struct BMAP_Preamble) +
      preamble->npills*sizeof(struct BMAP_PillInfo) +
      preamble->nbases*sizeof(struct BMAP_BaseInfo) +
      preamble->nstarts*sizeof(struct BMAP_StartInfo))
    LOGFAIL(ECORFILE)

  pillInfos = (struct BMAP_PillInfo *)(preamble + 1);
  baseInfos = (struct BMAP_BaseInfo *)(pillInfos + preamble->npills);
  startInfos = (struct BMAP_StartInfo *)(baseInfos + preamble->nbases);
  runData = (void *)(startInfos + preamble->nstarts);
  runDataLen =
    nbytes - (sizeof(struct BMAP_Preamble) +
              preamble->npills*sizeof(struct BMAP_PillInfo) +
              preamble->nbases*sizeof(struct BMAP_BaseInfo) +
              preamble->nstarts*sizeof(struct BMAP_StartInfo));

  server.npills = preamble->npills;
  server.nbases = preamble->nbases;
  server.nstarts = preamble->nstarts;

  /* load the pillinfo */
  for (i = 0; i < preamble->npills; i++) {
    server.pills[i].x = pillInfos[i].x;
    server.pills[i].y = pillInfos[i].y;
    server.pills[i].owner = NEUTRAL;  /* ignore pill owner */
    server.pills[i].armour = pillInfos[i].armour;
    server.pills[i].speed = (pillInfos[i].speed*MAXTICKSPERSHOT)/50;

    if (server.pills[i].speed > MAXTICKSPERSHOT) {
      server.pills[i].speed = MAXTICKSPERSHOT;
    }

    server.pills[i].counter = 0;
  }

  /* load the baseinfo */
  for (i = 0; i < preamble->nbases; i++) {
    server.bases[i].x = baseInfos[i].x;
    server.bases[i].y = baseInfos[i].y;
    server.bases[i].owner = NEUTRAL;  /* ignore base owner */
    server.bases[i].armour = baseInfos[i].armour;
    server.bases[i].shells = baseInfos[i].shells;
    server.bases[i].mines = baseInfos[i].mines;
    server.bases[i].counter = 0;
  }

  /* load the startinfo */
  for (i = 0; i < preamble->nstarts; i++) {
    server.starts[i].x = startInfos[i].x;
    server.starts[i].y = startInfos[i].y;
    server.starts[i].dir = startInfos[i].dir;
  }

  offset = 0;

  for (;;) {  /* write runs */
    struct BMAP_Run run;

    if (offset + sizeof(struct BMAP_Run) > runDataLen) {
      break;  /* ran out of bytes */
//      LOGFAIL(ECORFILE)
    }

    run = *(struct BMAP_Run *)(runData + offset);

    /* if last run */
    if (run.datalen == 4 && run.y == 0xff && run.startx == 0xff && run.endx == 0xff) {
      if (offset + run.datalen != runDataLen) {
        /* left over bytes extra game data??? ignore for now */
      }

      break;
    }

    if (offset + run.datalen > runDataLen) LOGFAIL(ECORFILE)
    if (writerun(run, runData + offset + sizeof(struct BMAP_Run), server.terrain) == -1) LOGFAIL(errno)
    offset += run.datalen;
  }

  /* clear all starting locations */
  for (i = 0; i < server.nstarts; i++) {
    server.terrain[server.starts[i].y][server.starts[i].x] = kSeaTerrain;
  }

  for (i = 0; i < server.npills; i++) {
    switch (server.terrain[server.pills[i].y][server.pills[i].x]) {
    case kSeaTerrain:
    case kBoatTerrain:
    case kWallTerrain:
    case kRiverTerrain:
    case kForestTerrain:
    case kDamagedWallTerrain0:
    case kDamagedWallTerrain1:
    case kDamagedWallTerrain2:
    case kDamagedWallTerrain3:
    case kMinedSeaTerrain:
    case kMinedForestTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kGrassTerrain0;
      break;

    case kMinedSwampTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kSwampTerrain0;
      break;

    case kMinedCraterTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kCraterTerrain;
      break;

    case kMinedRoadTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kRoadTerrain;
      break;

    case kMinedRubbleTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kRubbleTerrain0;

    case kMinedGrassTerrain:
      server.terrain[server.pills[i].y][server.pills[i].x] = kGrassTerrain0;
      break;

    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kCraterTerrain:
    case kRoadTerrain:
    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
      break;

    default:
      assert(0);
      break;
    }
  }


  for (i = 0; i < server.nbases; i++) {
    switch (server.terrain[server.bases[i].y][server.bases[i].x]) {
    case kSeaTerrain:
    case kBoatTerrain:
    case kWallTerrain:
    case kRiverTerrain:
    case kForestTerrain:
    case kDamagedWallTerrain0:
    case kDamagedWallTerrain1:
    case kDamagedWallTerrain2:
    case kDamagedWallTerrain3:
    case kMinedSeaTerrain:
    case kMinedForestTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kGrassTerrain0;
      break;

    case kMinedSwampTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kSwampTerrain0;
      break;

    case kMinedCraterTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kCraterTerrain;
      break;

    case kMinedRoadTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kRoadTerrain;
      break;

    case kMinedRubbleTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kRubbleTerrain0;

    case kMinedGrassTerrain:
      server.terrain[server.bases[i].y][server.bases[i].x] = kGrassTerrain0;
      break;

    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kCraterTerrain:
    case kRoadTerrain:
    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
      break;

    default:
      assert(0);
      break;
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

ssize_t serversavemap(void **data) {
  size_t y, x;
  int i;
  struct BMAP_Preamble *preamble;
  struct BMAP_PillInfo *pillInfos;
  struct BMAP_BaseInfo *baseInfos;
  struct BMAP_StartInfo *startInfos;
  void *runData;
  struct BMAP_Run *run;
  int offset;
  ssize_t size;

  *data = NULL;

TRY
  /* find the size of the map */
  if ((size = serverloadmapsize()) == -1) LOGFAIL(errno)

  /* allocate memory */
  if ((*data = malloc(size)) == NULL) LOGFAIL(errno)

  /* zero the bytes */
  bzero(*data, size);

  /* initialize the pointers */
  preamble = *data;
  pillInfos = (struct BMAP_PillInfo *)(preamble + 1);
  baseInfos = (struct BMAP_BaseInfo *)(pillInfos + server.npills);
  startInfos = (struct BMAP_StartInfo *)(baseInfos + server.nbases);
  runData = (void *)(startInfos + server.nstarts);

  /* write the preamble */
  bcopy(MAP_FILE_IDENT, preamble->ident, MAP_FILE_IDENT_LEN);
  preamble->version = CURRENT_MAP_VERSION;
  preamble->npills = server.npills;
  preamble->nbases = server.nbases;
  preamble->nstarts = server.nstarts;

  /* write the pillinfo */
  for (i = 0; i < server.npills; i++) {
    pillInfos[i].x = server.pills[i].x;
    pillInfos[i].y = server.pills[i].y;
    pillInfos[i].owner = server.pills[i].owner;
    pillInfos[i].armour = server.pills[i].armour;
    pillInfos[i].speed = server.pills[i].speed;
  }

  /* write the baseinfo */
  for (i = 0; i < server.nbases; i++) {
    baseInfos[i].x = server.bases[i].x;
    baseInfos[i].y = server.bases[i].y;
    baseInfos[i].owner = server.bases[i].owner;
    baseInfos[i].armour = server.bases[i].armour;
    baseInfos[i].shells = server.bases[i].shells;
    baseInfos[i].mines = server.bases[i].mines;
  }

  /* write the startinfo */
  for (i = 0; i < server.nstarts; i++) {
    startInfos[i].x = server.starts[i].x;
    startInfos[i].y = server.starts[i].y;
    startInfos[i].dir = server.starts[i].dir;
  }

  offset = 0;
  y = 0;
  x = 0;

  while (y < WIDTH && x < WIDTH) {
    int r;

    run = runData + offset;
    if ((r = readrun(&y, &x, run, run + 1, server.terrain)) == -1) LOGFAIL(errno)
    if (r == 1) break;
    offset += run->datalen;
  }

  data = NULL;

CLEANUP
  if (data != NULL && *data != NULL) {
    free(*data);
    *data = NULL;
  }

ERRHANDLER(size, -1)
END
}

ssize_t serverloadmapsize() {
  size_t x, y, len;

TRY
  x = 0;
  y = 0;
  len =
    sizeof(struct BMAP_Preamble) +
    server.npills*sizeof(struct BMAP_PillInfo) +
    server.nbases*sizeof(struct BMAP_BaseInfo) +
    server.nstarts*sizeof(struct BMAP_StartInfo);

  for (;;) {
    ssize_t r;
    struct BMAP_Run run;
    char buf[256];
    
    if ((r = readrun(&y, &x, &run, buf, server.terrain)) == -1) LOGFAIL(errno)
    len += run.datalen;
    /* if this is the last run */
    if (r == 1) SUCCESS
  }

CLEANUP
ERRHANDLER(len, -1)
END
}
