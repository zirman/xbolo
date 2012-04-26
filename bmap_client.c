/*
 *  bmap_client.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "bmap_client.h"
#include "client.h"
#include "tiles.h"
#include "images.h"
#include "errchk.h"

#include <stdlib.h>
#include <string.h>

int clientloadmap(const void *buf, size_t len) {
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
      client.terrain[y][x] = defaultterrain(x, y);
      client.fog[y][x] = 0;
    }
  }

  if (len < sizeof(struct BMAP_Preamble)) LOGFAIL(ECORFILE)

  /* initialize the preamble pointer */
  preamble = buf;

  if (preamble->npills > MAXPILLS) LOGFAIL(ECORFILE)

  if (preamble->nbases > MAXBASES) LOGFAIL(ECORFILE)

  if (preamble->nstarts > MAX_STARTS) LOGFAIL(ECORFILE)

  if (strncmp((char *)preamble->ident, MAP_FILE_IDENT, MAP_FILE_IDENT_LEN) != 0)
    LOGFAIL(ECORFILE)

  if (preamble->version != CURRENT_MAP_VERSION) LOGFAIL(EINCMPAT)

  if (len < sizeof(struct BMAP_Preamble) +
      preamble->npills*sizeof(struct BMAP_PillInfo) +
      preamble->nbases*sizeof(struct BMAP_BaseInfo) +
      preamble->nstarts*sizeof(struct BMAP_StartInfo))
    LOGFAIL(ECORFILE)

  pillInfos = (struct BMAP_PillInfo *)(preamble + 1);
  baseInfos = (struct BMAP_BaseInfo *)(pillInfos + preamble->npills);
  startInfos = (struct BMAP_StartInfo *)(baseInfos + preamble->nbases);
  runData = (void *)(startInfos + preamble->nstarts);
  runDataLen = len - (sizeof(struct BMAP_Preamble) +
                      preamble->npills*sizeof(struct BMAP_PillInfo) +
                      preamble->nbases*sizeof(struct BMAP_BaseInfo) +
                      preamble->nstarts*sizeof(struct BMAP_StartInfo));

  /* load the pillinfo */
  client.npills = preamble->npills;
  for (i = 0; i < preamble->npills; i++) {
    client.pills[i].x = pillInfos[i].x;
    client.pills[i].y = pillInfos[i].y;
    client.pills[i].owner = pillInfos[i].owner;
    client.pills[i].armour = pillInfos[i].armour;
    client.pills[i].speed = pillInfos[i].speed;
    client.pills[i].counter = 0;
  }

  /* load the baseinfo */
  client.nbases = preamble->nbases;
  for (i = 0; i < preamble->nbases; i++) {
    client.bases[i].x = baseInfos[i].x;
    client.bases[i].y = baseInfos[i].y;
    client.bases[i].owner = baseInfos[i].owner;
    client.bases[i].armour = baseInfos[i].armour;
    client.bases[i].shells = baseInfos[i].shells;
    client.bases[i].mines = baseInfos[i].mines;
    client.bases[i].counter = 0;
  }

  /* load the startinfo */
  client.nstarts = preamble->nstarts;
  for (i = 0; i < preamble->nstarts; i++) {
    client.starts[i].x = startInfos[i].x;
    client.starts[i].y = startInfos[i].y;
    client.starts[i].dir = startInfos[i].dir;
  }

  offset = 0;

  for (;;) {  /* write runs */
    struct BMAP_Run run;

    if (offset + sizeof(struct BMAP_Run) > runDataLen) {
      LOGFAIL(ECORFILE)
    }

    run = *(struct BMAP_Run *)(runData + offset);

    if (run.datalen == 4 && run.y == 0xff &&
        run.startx == 0xff && run.endx == 0xff) {  /* last run */
      if (offset + run.datalen != runDataLen) {
        LOGFAIL(ECORFILE)
      }

      break;
    }

    if (offset + run.datalen > runDataLen) {
      LOGFAIL(ECORFILE)
    }

    if (writerun(run, runData + offset + sizeof(struct BMAP_Run), client.terrain) == -1) {
      LOGFAIL(errno)
    }

    offset += run.datalen;
  }

  for (y = 0; y < WIDTH; y++) {
    for (x = 0; x < WIDTH; x++) {
      if (client.fog[y][x] > 0) {
        client.seentiles[y][x] = terraintotile(client.terrain[y][x]);
      }
      else {
        client.seentiles[y][x] = kUnknownTile;
      }
    }
  }

  for (i = 0; i < client.nbases; i++) {
    if (client.fog[client.bases[i].y][client.bases[i].x] > 0) {
      if (client.bases[i].owner == NEUTRAL) {
        client.seentiles[client.bases[i].y][client.bases[i].x] = kNeutralBaseTile;
      }
      else if (
        (client.players[client.bases[i].owner].alliance & (1 << client.player)) &&
        (client.players[client.player].alliance & (1 << client.bases[i].owner))
      ) {
        client.seentiles[client.bases[i].y][client.bases[i].x] = kFriendlyBaseTile;
      }
      else {
        client.seentiles[client.bases[i].y][client.bases[i].x] = kHostileBaseTile;
      }
    }
  }

  for (i = 0; i < client.npills; i++) {
    if (client.pills[i].armour != ONBOARD) {
      if (client.fog[client.pills[i].y][client.pills[i].x] > 0) {
        if (client.pills[i].owner == NEUTRAL) {
          client.seentiles[client.pills[i].y][client.pills[i].x] = kHostilePill00Tile + client.pills[i].armour;
        }
        else if (
          (client.players[client.pills[i].owner].alliance & (1 << client.player)) &&
          (client.players[client.player].alliance & (1 << client.pills[i].owner))
        ) {
          client.seentiles[client.pills[i].y][client.pills[i].x] = kFriendlyPill00Tile + client.pills[i].armour;
        }
        else {
          client.seentiles[client.pills[i].y][client.pills[i].x] = kHostilePill00Tile + client.pills[i].armour;
        }
      }
    }
  }

  for (y = 0; y < WIDTH; y++) {
    for (x = 0; x < WIDTH; x++) {
      client.images[y][x] = mapimage(client.seentiles, x, y);
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

