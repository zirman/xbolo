/*
 *  terrain.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __TERRAIN__
#define __TERRAIN__

enum {
	kSeaTerrain,
	kBoatTerrain,
	kWallTerrain,
	kRiverTerrain,
	kSwampTerrain0,
	kSwampTerrain1,
	kSwampTerrain2,
	kSwampTerrain3,
	kCraterTerrain,
	kRoadTerrain,
	kForestTerrain,
	kRubbleTerrain0,
	kRubbleTerrain1,
	kRubbleTerrain2,
	kRubbleTerrain3,
	kGrassTerrain0,
	kGrassTerrain1,
	kGrassTerrain2,
	kGrassTerrain3,
	kDamagedWallTerrain0,
	kDamagedWallTerrain1,
	kDamagedWallTerrain2,
	kDamagedWallTerrain3,

  /* mined terrain */
	kMinedSeaTerrain,
	kMinedSwampTerrain,
	kMinedCraterTerrain,
	kMinedRoadTerrain,
	kMinedForestTerrain,
	kMinedRubbleTerrain,
	kMinedGrassTerrain,
} ;

int isWaterLikeTerrain(int terrain);

#endif /* __TERRAIN__ */
