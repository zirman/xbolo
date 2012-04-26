//
//  GSRobotExternal.h
//  XBolo
//
//  Created by Michael Ash on 9/7/09.
//

#import <Cocoa/Cocoa.h>


#if !INTERNAL_GSROBOT_INCLUDE
enum {
    BUILDERNILL = -1,
    BUILDERTREE,
    BUILDERROAD,
    BUILDERWALL,
    BUILDERPILL,
    BUILDERMINE,
} ;

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

typedef struct Vec2f {
    float x;
    float y;
} Vec2f;
#endif

struct Tank
{
    BOOL friendly;
    Vec2f position;
    int direction;
};

#if INTERNAL_GSROBOT_INCLUDE
#define Shell ExternalShell
#endif
struct Shell
{
    float direction; // radians
    Vec2f position;
};

struct Builder
{
    BOOL isParachute;
    Vec2f position;
};

struct GSRobotGameState
{
    int worldwidth, worldheight;
    int *visibletiles; // worldwidth * worldheight elements, row major
    
    Vec2f tankposition;
    Vec2f gunsightposition;
    int tankdirection; // 0-15
    int tankarmor;
    int tankshells;
    int tankmines;
    int tanktrees;
    int tankhasboat;
    int tankpillcount;
    
    int builderstate; // 0 = dead, 1 = in tank, 2 = out of tank
    float builderdirection; // radians, only valid if builderstate is out of tank
    
    int tankscount;
    struct Tank *tanks;
    
    int shellscount;
    struct Shell *shells;
    
    int builderscount;
    struct Builder *builders;
    
    NSArray *messages; // array of NSString, may be nil if no messages
};
#if INTERNAL_GSROBOT_INCLUDE
#undef Shell
#endif

struct GSRobotCommandState
{
    BOOL accelerate;
    BOOL decelerate;
    BOOL left;
    BOOL right;
    BOOL gunup;
    BOOL gundown;
    BOOL mine;
    BOOL fire;
    
    int buildercommand;
    int builderx, buildery;
    
    NSArray *playersToAllyWith; // NSStrings containing player names
};

#define GS_ROBOT_CURRENT_INTERFACE_VERSION 1

@protocol GSRobot <NSObject>

+ (int)minimumRobotInterfaceVersionRequired;
- (id)init; // designated initializer
- (struct GSRobotCommandState)stepXBoloRobotWithGameState: (struct GSRobotGameState *)gameState freeFunction: (void (*)(void *))freeF freeContext: (void *)freeCtx;

@end
