/*
 *  images.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "images.h"
#include "tiles.h"

#include <assert.h>

int mapimage(int tiles[][256], int x, int y) {
  assert(x >= 0 && x < 256 && y >= 0 && y < 256);

	switch (tiles[y][x]) {
	case kSeaTile:
  case kMinedSeaTile:
		switch ((isSeaLikeTile(tiles, x - 1, y) ? 1 : 0) |
					  (isSeaLikeTile(tiles, x, y - 1) ? 2 : 0) |
					  (isSeaLikeTile(tiles, x + 1, y) ? 4 : 0) |
					  (isSeaLikeTile(tiles, x, y + 1) ? 8 : 0)) {
		case 0:
		case 5:
		case 10:
		case 15:
			return SEAA00IMAGE;

		case 1:
		case 11:
			return SEAA01IMAGE;

		case 4:
		case 14:
			return SEAA02IMAGE;

		case 8:
		case 13:
			return SEAA03IMAGE;

		case 2:
		case 7:
			return SEAA04IMAGE;

		case 9:
			return SEAA05IMAGE;

		case 3:
			return SEAA06IMAGE;

		case 12:
			return SEAA07IMAGE;

		case 6:
			return SEAA08IMAGE;
		}

	case kRiverTile:
		switch ((isWaterLikeToWaterTile(tiles, x - 1, y) ? 1 : 0) |
					  (isWaterLikeToWaterTile(tiles, x, y - 1) ? 2 : 0) |
					  (isWaterLikeToWaterTile(tiles, x + 1, y) ? 4 : 0) |
					  (isWaterLikeToWaterTile(tiles, x, y + 1) ? 8 : 0)) {
		case 12:
			return RIVE00IMAGE;

		case 13:
			return RIVE01IMAGE;

		case 9:
			return RIVE02IMAGE;

		case 14:
			return RIVE03IMAGE;

		case 15:
			return RIVE04IMAGE;

		case 11:
			return RIVE05IMAGE;

		case 6:
			return RIVE06IMAGE;

		case 7:
			return RIVE07IMAGE;

		case 3:
			return RIVE08IMAGE;

		case 4:
			return RIVE09IMAGE;

		case 5:
			return RIVE10IMAGE;

		case 1:
			return RIVE11IMAGE;

		case 8:
			return RIVE12IMAGE;

		case 10:
			return RIVE13IMAGE;

		case 2:
			return RIVE14IMAGE;

		case 0:
			return RIVE15IMAGE;
		}

	case kSwampTile:
  case kMinedSwampTile:
		return SWAM00IMAGE;

	case kGrassTile:
  case kMinedGrassTile:
		return GRAS00IMAGE;

	case kForestTile:
  case kMinedForestTile:
		switch ((isForestLikeTile(tiles, x - 1, y) ? 1 : 0) |
					  (isForestLikeTile(tiles, x, y - 1) ? 2 : 0) |
					  (isForestLikeTile(tiles, x + 1, y) ? 4 : 0) |
					  (isForestLikeTile(tiles, x, y + 1) ? 8 : 0)) {
		case 12:
			return FORE00IMAGE;

		case 9:
			return FORE01IMAGE;

		case 5:
		case 7:
		case 10:
		case 11:
		case 13:
		case 14:
		case 15:
			return FORE02IMAGE;

		case 6:
			return FORE03IMAGE;

		case 3:
			return FORE04IMAGE;

		case 4:
			return FORE05IMAGE;

		case 1:
			return FORE06IMAGE;

		case 8:
			return FORE07IMAGE;

		case 2:
			return FORE08IMAGE;

		case 0:
			return FORE09IMAGE;
		}

	case kCraterTile:
  case kMinedCraterTile:
		switch ((isCraterLikeTile(tiles, x - 1, y) ? 1 : 0) |
					  (isCraterLikeTile(tiles, x, y - 1) ? 2 : 0) |
					  (isCraterLikeTile(tiles, x + 1, y) ? 4 : 0) |
					  (isCraterLikeTile(tiles, x, y + 1) ? 8 : 0)) {
		case 12:
			return CRAT00IMAGE;

		case 13:
			return CRAT01IMAGE;

		case 9:
			return CRAT02IMAGE;

		case 14:
			return CRAT03IMAGE;

		case 15:
			return CRAT04IMAGE;

		case 11:
			return CRAT05IMAGE;

		case 6:
			return CRAT06IMAGE;

		case 7:
			return CRAT07IMAGE;

		case 3:
			return CRAT08IMAGE;

		case 4:
			return CRAT09IMAGE;

		case 5:
			return CRAT10IMAGE;

		case 1:
			return CRAT11IMAGE;

		case 8:
			return CRAT12IMAGE;

		case 10:
			return CRAT13IMAGE;

		case 2:
			return CRAT14IMAGE;

		case 0:
			return CRAT15IMAGE;
		}

	case kRoadTile:
	case kMinedRoadTile:
		switch ((isRoadLikeTile(tiles, x - 1, y) ? 1 : 0) |
					  (isRoadLikeTile(tiles, x, y - 1) ? 2 : 0) |
					  (isRoadLikeTile(tiles, x + 1, y) ? 4 : 0) |
					  (isRoadLikeTile(tiles, x, y + 1) ? 8 : 0)) {
		case 0:
			switch ((isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x, y - 1) ? 2 : 0) |
              (isWaterLikeToLandTile(tiles, x + 1, y) ? 4 : 0) |
              (isWaterLikeToLandTile(tiles, x, y + 1) ? 8 : 0)) {
      case 15:
				return ROAD30IMAGE;

      case 5:
        return ROAD23IMAGE;

      case 10:
        return ROAD21IMAGE;

      default:
				return ROAD10IMAGE;
			}

		case 1:
		case 4:
		case 5:
			switch ((isWaterLikeToLandTile(tiles, x, y - 1) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x, y + 1) ? 2 : 0)) {
      case 3:
				return ROAD21IMAGE;

      default:
				return ROAD01IMAGE;
			}

		case 2:
		case 8:
		case 10:
			switch ((isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x + 1, y) ? 2 : 0)) {
      case 3:
				return ROAD23IMAGE;

      default:
        return ROAD03IMAGE;
			}

		case 6:
			switch ((isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x, y + 1) ? 2 : 0)) {
      case 3:
				return ROAD24IMAGE;

      default:
				switch (isRoadLikeTile(tiles, x + 1, y - 1) ? 1 : 0) {
        case 1:
					return ROAD12IMAGE;

        default:
					return ROAD04IMAGE;
				}
			}

		case 3:
			switch ((isWaterLikeToLandTile(tiles, x + 1, y) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x, y + 1) ? 2 : 0)) {
      case 3:
				return ROAD25IMAGE;

      default:
				switch (isRoadLikeTile(tiles, x - 1, y - 1) ? 1 : 0) {
        case 1:
					return ROAD14IMAGE;

        default:
					return ROAD05IMAGE;
				}
			}

		case 7:
			switch (isWaterLikeToLandTile(tiles, x, y + 1) ? 1 : 0) {
      case 1:
				return ROAD29IMAGE;

      default:
				switch ((isRoadLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
                (isRoadLikeTile(tiles, x + 1, y - 1) ? 2 : 0)) {
        case 0:
					return ROAD18IMAGE;

        default:
					return ROAD13IMAGE;
				}
			}

		case 12:
			switch ((isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x, y - 1) ? 2 : 0)) {
      case 3:
				return ROAD20IMAGE;

      default:
				switch (isRoadLikeTile(tiles, x + 1, y + 1) ? 1 : 0) {
        case 1:
					return ROAD06IMAGE;

        default:
					return ROAD00IMAGE;
				}
			}

		case 14:
			switch (isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) {
      case 1:
				return ROAD27IMAGE;

      default:
				switch ((isRoadLikeTile(tiles, x + 1, y - 1) ? 1 : 0) |
                (isRoadLikeTile(tiles, x + 1, y + 1) ? 2 : 0)) {
        case 0:
					return ROAD15IMAGE;

        default:
					return ROAD09IMAGE;
				}
			}

		case 9:
			switch ((isWaterLikeToLandTile(tiles, x, y - 1) ? 1 : 0) |
              (isWaterLikeToLandTile(tiles, x + 1, y) ? 2 : 0)) {
      case 3:
				return ROAD22IMAGE;

      default:
				switch (isRoadLikeTile(tiles, x - 1, y + 1) ? 1 : 0) {
        case 1:
					return ROAD08IMAGE;

        default:
          return ROAD02IMAGE;
				}
			}

		case 13:
			switch (isWaterLikeToLandTile(tiles, x, y - 1) ? 1 : 0) {
      case 1:
				return ROAD26IMAGE;

      default:
				switch ((isRoadLikeTile(tiles, x - 1, y + 1) ? 1 : 0) |
                (isRoadLikeTile(tiles, x + 1, y + 1) ? 2 : 0)) {
        case 0:
					return ROAD17IMAGE;

        default:
					return ROAD07IMAGE;
				}
			}

		case 11:
			switch (isWaterLikeToLandTile(tiles, x + 1, y) ? 1 : 0) {
      case 1:
				return ROAD28IMAGE;

      default:
				switch ((isRoadLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
                (isRoadLikeTile(tiles, x - 1, y + 1) ? 2 : 0)) {
        case 0:
					return ROAD16IMAGE;

        default:
					return ROAD11IMAGE;
				}
			}

		case 15:
			switch ((isRoadLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
              (isRoadLikeTile(tiles, x + 1, y - 1) ? 2 : 0) |
              (isRoadLikeTile(tiles, x - 1, y + 1) ? 4 : 0) |
              (isRoadLikeTile(tiles, x + 1, y + 1) ? 8 : 0)) {
      case 0:
				return ROAD19IMAGE;

      default:
				return ROAD10IMAGE;
			}
		}

	case kRubbleTile:
  case kMinedRubbleTile:
		return RUBB00IMAGE;

	case kDamagedWallTile:
		return DAMG00IMAGE;

	case kWallTile:
		switch ((isWallLikeTile(tiles, x - 1, y) ? 1 : 0) |
					  (isWallLikeTile(tiles, x, y - 1) ? 2 : 0) |
					  (isWallLikeTile(tiles, x + 1, y) ? 4 : 0) |
					  (isWallLikeTile(tiles, x, y + 1) ? 8 : 0)) {
		case 0:
			return WALL46IMAGE;

		case 4:
			return WALL17IMAGE;

		case 2:
			return WALL22IMAGE;

		case 6:
			switch (isWallLikeTile(tiles, x + 1, y - 1) ? 1 : 0) {
      case 1:
				return WALL12IMAGE;

      default:
        return WALL04IMAGE;
			}

		case 1:
			return WALL20IMAGE;

		case 5:
			return WALL01IMAGE;

		case 3:
			switch (isWallLikeTile(tiles, x - 1, y - 1) ? 1 : 0) {
      case 1:
				return WALL14IMAGE;

      default:
				return WALL05IMAGE;
			}

		case 7:
			switch ((isWallLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
						  (isWallLikeTile(tiles, x + 1, y - 1) ? 2 : 0)) {
			case 0:
				return WALL16IMAGE;

			case 1:
				return WALL38IMAGE;

			case 2:
				return WALL37IMAGE;
			
			case 3:
				return WALL13IMAGE;
			}

		case 8:
			return WALL15IMAGE;

		case 12:
			switch (isWallLikeTile(tiles, x + 1, y + 1) ? 1 : 0) {
      case 1:
				return WALL06IMAGE;

      default:
				return WALL00IMAGE;
			}

		case 10:
			return WALL03IMAGE;

		case 14:
			switch ((isWallLikeTile(tiles, x + 1, y - 1) ? 1 : 0) |
						  (isWallLikeTile(tiles, x + 1, y + 1) ? 2 : 0)) {
			case 0:
				return WALL19IMAGE;

			case 1:
				return WALL33IMAGE;

			case 2:
				return WALL31IMAGE;

			case 3:
				return WALL09IMAGE;
			}

		case 9:
			switch (isWallLikeTile(tiles, x - 1, y + 1) ? 1 : 0) {
      case 1:
				return WALL08IMAGE;

      default:
				return WALL02IMAGE;
			}

		case 13:
			switch ((isWallLikeTile(tiles, x - 1, y + 1) ? 1 : 0) |
						  (isWallLikeTile(tiles, x + 1, y + 1) ? 2 : 0)) {
			case 0:
				return WALL21IMAGE;

			case 1:
				return WALL36IMAGE;

			case 2:
				return WALL35IMAGE;

			case 3:
				return WALL07IMAGE;
			}

		case 11:
			switch ((isWallLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
						  (isWallLikeTile(tiles, x - 1, y + 1) ? 2 : 0)) {
			case 0:
				return WALL18IMAGE;

			case 1:
				return WALL34IMAGE;

			case 2:
				return WALL32IMAGE;

			case 3:
				return WALL11IMAGE;
			}

		case 15:
			switch ((isWallLikeTile(tiles, x - 1, y - 1) ? 1 : 0) |
						  (isWallLikeTile(tiles, x + 1, y - 1) ? 2 : 0) |
						  (isWallLikeTile(tiles, x - 1, y + 1) ? 4 : 0) |
						  (isWallLikeTile(tiles, x + 1, y + 1) ? 8 : 0)) {
			case 0:
				return WALL45IMAGE;

			case 1:
				return WALL29IMAGE;

			case 2:
				return WALL30IMAGE;

			case 3:
				return WALL26IMAGE;

			case 4:
				return WALL27IMAGE;

			case 5:
				return WALL25IMAGE;

			case 6:
				return WALL44IMAGE;

			case 7:
				return WALL42IMAGE;

			case 8:
				return WALL28IMAGE;

			case 9:
				return WALL43IMAGE;

			case 10:
				return WALL24IMAGE;

			case 11:
				return WALL41IMAGE;

			case 12:
				return WALL23IMAGE;

			case 13:
				return WALL40IMAGE;

			case 14:
				return WALL39IMAGE;

			case 15:
				return WALL10IMAGE;
			}
		}

	case kBoatTile:
		switch ((isWaterLikeToLandTile(tiles, x - 1, y) ? 1 : 0) |
					  (isWaterLikeToLandTile(tiles, x, y - 1) ? 2 : 0) |
					  (isWaterLikeToLandTile(tiles, x + 1, y) ? 4 : 0) |
					  (isWaterLikeToLandTile(tiles, x, y + 1) ? 8 : 0)) {
		case 0:
		case 6:
		case 15:
			return BOAT00IMAGE;

		case 2:
		case 7:
		case 10:
			return BOAT01IMAGE;

		case 3:
			return BOAT02IMAGE;

		case 1:
    case 11:
			return BOAT03IMAGE;

		case 9:
			return BOAT04IMAGE;

		case 8:
    case 13:
			return BOAT05IMAGE;

		case 12:
			return BOAT06IMAGE;

		case 4:
		case 5:
		case 14:
			return BOAT07IMAGE;
		}

	case kFriendlyBaseTile:
		return FBAS00IMAGE;

	case kFriendlyPill00Tile:
		return FPIL00IMAGE;

	case kFriendlyPill01Tile:
		return FPIL01IMAGE;

	case kFriendlyPill02Tile:
		return FPIL02IMAGE;

	case kFriendlyPill03Tile:
		return FPIL03IMAGE;

	case kFriendlyPill04Tile:
		return FPIL04IMAGE;

	case kFriendlyPill05Tile:
		return FPIL05IMAGE;

	case kFriendlyPill06Tile:
		return FPIL06IMAGE;

	case kFriendlyPill07Tile:
		return FPIL07IMAGE;

	case kFriendlyPill08Tile:
		return FPIL08IMAGE;

	case kFriendlyPill09Tile:
		return FPIL09IMAGE;

	case kFriendlyPill10Tile:
		return FPIL10IMAGE;

	case kFriendlyPill11Tile:
		return FPIL11IMAGE;

	case kFriendlyPill12Tile:
		return FPIL12IMAGE;

	case kFriendlyPill13Tile:
		return FPIL13IMAGE;

	case kFriendlyPill14Tile:
		return FPIL14IMAGE;

	case kFriendlyPill15Tile:
		return FPIL15IMAGE;

	case kHostileBaseTile:
		return HBAS00IMAGE;

	case kHostilePill00Tile:
		return HPIL00IMAGE;

	case kHostilePill01Tile:
		return HPIL01IMAGE;

	case kHostilePill02Tile:
		return HPIL02IMAGE;

	case kHostilePill03Tile:
		return HPIL03IMAGE;

	case kHostilePill04Tile:
		return HPIL04IMAGE;

	case kHostilePill05Tile:
		return HPIL05IMAGE;

	case kHostilePill06Tile:
		return HPIL06IMAGE;

	case kHostilePill07Tile:
		return HPIL07IMAGE;

	case kHostilePill08Tile:
		return HPIL08IMAGE;

	case kHostilePill09Tile:
		return HPIL09IMAGE;

	case kHostilePill10Tile:
		return HPIL10IMAGE;

	case kHostilePill11Tile:
		return HPIL11IMAGE;

	case kHostilePill12Tile:
		return HPIL12IMAGE;

	case kHostilePill13Tile:
		return HPIL13IMAGE;

	case kHostilePill14Tile:
		return HPIL14IMAGE;

	case kHostilePill15Tile:
		return HPIL15IMAGE;

	case kNeutralBaseTile:
		return NBAS00IMAGE;
	}

  return -1;
}
