/*
 *  terrain.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "terrain.h"

int isWaterLikeTerrain(int terrain) {
  switch (terrain) {
  case kRiverTerrain:
  case kSeaTerrain:
  case kMinedSeaTerrain:
  case kBoatTerrain:
    return 1;

  default:
    return 0;
  }
}
