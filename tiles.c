/*
 *  tiles.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "tiles.h"

int isForestLikeTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kForestTile:
    case kMinedForestTile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isCraterLikeTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kCraterTile:
    case kRiverTile:
    case kSeaTile:
    case kMinedCraterTile:
    case kMinedSeaTile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isRoadLikeTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kRoadTile:
    case kMinedRoadTile:
    case kFriendlyBaseTile:
    case kHostileBaseTile:
    case kNeutralBaseTile:
    case kFriendlyPill00Tile:
    case kFriendlyPill01Tile:
    case kFriendlyPill02Tile:
    case kFriendlyPill03Tile:
    case kFriendlyPill04Tile:
    case kFriendlyPill05Tile:
    case kFriendlyPill06Tile:
    case kFriendlyPill07Tile:
    case kFriendlyPill08Tile:
    case kFriendlyPill09Tile:
    case kFriendlyPill10Tile:
    case kFriendlyPill11Tile:
    case kFriendlyPill12Tile:
    case kFriendlyPill13Tile:
    case kFriendlyPill14Tile:
    case kFriendlyPill15Tile:
    case kHostilePill00Tile:
    case kHostilePill01Tile:
    case kHostilePill02Tile:
    case kHostilePill03Tile:
    case kHostilePill04Tile:
    case kHostilePill05Tile:
    case kHostilePill06Tile:
    case kHostilePill07Tile:
    case kHostilePill08Tile:
    case kHostilePill09Tile:
    case kHostilePill10Tile:
    case kHostilePill11Tile:
    case kHostilePill12Tile:
    case kHostilePill13Tile:
    case kHostilePill14Tile:
    case kHostilePill15Tile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isWaterLikeToLandTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kRiverTile:
    case kBoatTile:
    case kSeaTile:
    case kMinedSeaTile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isWaterLikeToWaterTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kRoadTile:
    case kRiverTile:
    case kBoatTile:
    case kSeaTile:
    case kCraterTile:
    case kMinedRoadTile:
    case kMinedSeaTile:
    case kMinedCraterTile:
    case kFriendlyBaseTile:
    case kHostileBaseTile:
    case kNeutralBaseTile:
    case kFriendlyPill00Tile:
    case kFriendlyPill01Tile:
    case kFriendlyPill02Tile:
    case kFriendlyPill03Tile:
    case kFriendlyPill04Tile:
    case kFriendlyPill05Tile:
    case kFriendlyPill06Tile:
    case kFriendlyPill07Tile:
    case kFriendlyPill08Tile:
    case kFriendlyPill09Tile:
    case kFriendlyPill10Tile:
    case kFriendlyPill11Tile:
    case kFriendlyPill12Tile:
    case kFriendlyPill13Tile:
    case kFriendlyPill14Tile:
    case kFriendlyPill15Tile:
    case kHostilePill00Tile:
    case kHostilePill01Tile:
    case kHostilePill02Tile:
    case kHostilePill03Tile:
    case kHostilePill04Tile:
    case kHostilePill05Tile:
    case kHostilePill06Tile:
    case kHostilePill07Tile:
    case kHostilePill08Tile:
    case kHostilePill09Tile:
    case kHostilePill10Tile:
    case kHostilePill11Tile:
    case kHostilePill12Tile:
    case kHostilePill13Tile:
    case kHostilePill14Tile:
    case kHostilePill15Tile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isWallLikeTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kRubbleTile:
    case kDamagedWallTile:
    case kWallTile:
    case kMinedRubbleTile:
    case kUnknownTile:
      return 1;

    default:
      return 0;
    }
  }
}

int isSeaLikeTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kSeaTile:
    case kMinedSeaTile:
    case kUnknownTile:
      return 1;
  
    default:
      return 0;
    }
  }
}

int isMinedTile(int tiles[][256], int x, int y) {
  if (x < 0 || x >= 256 || y < 0 || y >= 256) {
    return 1;
  }
  else {
    switch (tiles[y][x]) {
    case kMinedSwampTile:
    case kMinedCraterTile:
    case kMinedRoadTile:
    case kMinedForestTile:
    case kMinedRubbleTile:
    case kMinedGrassTile:
    case kMinedSeaTile:
      return 1;

    default:
      return 0;
    }
  }
}
