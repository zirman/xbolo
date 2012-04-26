/*
 *  tiles.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __TILES__
#define __TILES__

enum {
  kWallTile         = 0,
  kRiverTile        = 1,
  kSwampTile        = 2,
  kCraterTile       = 3,
  kRoadTile         = 4,
  kForestTile       = 5,
  kRubbleTile       = 6,
  kGrassTile        = 7,
  kDamagedWallTile  = 8,
  kBoatTile         = 9,

  kMinedSwampTile   = 10,
  kMinedCraterTile  = 11,
  kMinedRoadTile    = 12,
  kMinedForestTile  = 13,
  kMinedRubbleTile  = 14,
  kMinedGrassTile   = 15,

  kSeaTile,
  kMinedSeaTile,
  kFriendlyBaseTile,
  kHostileBaseTile,
  kNeutralBaseTile,
  kFriendlyPill00Tile,
  kFriendlyPill01Tile,
  kFriendlyPill02Tile,
  kFriendlyPill03Tile,
  kFriendlyPill04Tile,
  kFriendlyPill05Tile,
  kFriendlyPill06Tile,
  kFriendlyPill07Tile,
  kFriendlyPill08Tile,
  kFriendlyPill09Tile,
  kFriendlyPill10Tile,
  kFriendlyPill11Tile,
  kFriendlyPill12Tile,
  kFriendlyPill13Tile,
  kFriendlyPill14Tile,
  kFriendlyPill15Tile,
  kHostilePill00Tile,
  kHostilePill01Tile,
  kHostilePill02Tile,
  kHostilePill03Tile,
  kHostilePill04Tile,
  kHostilePill05Tile,
  kHostilePill06Tile,
  kHostilePill07Tile,
  kHostilePill08Tile,
  kHostilePill09Tile,
  kHostilePill10Tile,
  kHostilePill11Tile,
  kHostilePill12Tile,
  kHostilePill13Tile,
  kHostilePill14Tile,
  kHostilePill15Tile,
  kUnknownTile,
} ;

int isForestLikeTile(int tiles[][256], int x, int y);
int isCraterLikeTile(int tiles[][256], int x, int y);
int isRoadLikeTile(int tiles[][256], int x, int y);
int isWaterLikeToLandTile(int tiles[][256], int x, int y);
int isWaterLikeToWaterTile(int tiles[][256], int x, int y);
int isWallLikeTile(int tiles[][256], int x, int y);
int isSeaLikeTile(int tiles[][256], int x, int y);

int isMinedTile(int tiles[][256], int x, int y);

#endif  /* __TILES__ */
