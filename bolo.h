/*
 *  bolo.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/1/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __BOLO__
#define __BOLO__

#include "vector.h"
#include "rect.h"
#include "list.h"
#include "io.h"
#include "errchk.h"
#include "timing.h"
#include "terrain.h"
#include "tiles.h"
#include "images.h"

#include <stdint.h>
#include <errno.h>

#define CURRENT_MAP_VERSION (1)
#define NET_GAME_VERSION    (1)
#define MAXPLAYERS          (16)
#define MAXPILLS            (16)
#define MAXBASES            (16)
#define MAX_STARTS          (16)
#define MINEWIDTH           (10)
#define X_MIN_MINE          (10)
#define Y_MIN_MINE          (10)
#define X_MAX_MINE          (245)
#define Y_MAX_MINE          (245)
#define NET_GAME_IDENT      ("XBOLOGAM")
#define MAP_FILE_IDENT      ("BMAPBOLO")
#define MAP_FILE_IDENT_LEN  (8)
#define NEUTRAL             (0xff)
#define TICKSPERSEC         (50)
#define MAXTICKSPERSHOT     (100)
#define MINTICKSPERSHOT		(6)
#define COOLPILLTICKS       (32)
#define EXPLODETICKS        (45)
#define REPLENISHBASETICKS  (600)
#define REFUELARMOURTICKS   (46)
#define REFUELSHELLSTICKS   (7)
#define REFUELMINESTICKS    (7)
#define DRAINTICKS          (15)
#define TICKS_PER_FRAME     (USEC_PER_SEC / TICKSPERSEC)
#define ONBOARD             (0xff)
#define MAXSHELLS           (40)
#define MAXMINES            (40)
#define MAXARMOUR           (40)
#define MAXTREES            (40)
#define MAXNAME             (16)
#define MAXPASS             (32)
#define MAXHOST             (32)
#define CLUPDATEMAXSHELLS   (255)
#define CLUPDATEMAXEXPLOSIONS  (255)
#define RECVMAPBLOCKSIZE    (1024)

#define NOPILL              (0xff)

#define WIDTH               (256)
#define FWIDTH              (256.0)
#define PLANTERWIDTH        (32)  /* must divide width evenly */

#define SHELLVEL            (7.0)

#define SHELLRATE           (4)
#define DRANGE              (TICKSPERSEC / 6.0) /* 1 tile per 12 ticks */
#define MINRANGE            (1.0)
#define MAXRANGE            (7.0)

#define TANKRADIUS          (0.375)
#define BUILDERRADIUS       (0.125)
#define EXPLOSIONRADIUS     (0.5)
#define BOATMAXSPEED        (3.125)  /* squares per second */
#define ROADMAXSPEED		BOATMAXSPEED
#define GRASSMAXSPEED		(2.34375)
#define FORESTMAXSPEED		(1.171875)
#define RUBBLEMAXSPEED		(0.5859375)
#define TICKS_FOR_COMPLETE_STOP (64)
#define ACCEL               (BOATMAXSPEED * TICKSPERSEC / TICKS_FOR_COMPLETE_STOP)  /* squares per second per second */
#define ANGULARACCEL        (12.5663706143592)  /* radians per second per second */
#define BUILDERMAXSPEED     ROADMAXSPEED
#define PARACHUTESPEED      RUBBLEMAXSPEED
#define MAXANGULARVELOCITY  (2.5)
#define PUSHFORCE           (1.5625)
#define KICKFORCE           (3.125)

#define BUILDERBUILDTIME	(20)

#define CHAINTICKS          (6)
#define FLOODTICKS          (12)
#define RESPAWN_TICKS       (150)
#define EXPLOSIONTICKS      (24)

#define MAXPILLARMOUR       (15)

#define FORRESTTREES        (4)
#define ROADTREES           (2)
#define WALLTREES           (2)
#define BOATTREES           (20)
#define PILLTREES           (4)

#define MAXBASEARMOUR       (90)
#define MAXBASESHELLS       (90)
#define MAXBASEMINES        (90)
#define MINBASEARMOUR       (5)
#define MINBASESHELLS       (1)
#define MINBASEMINES        (1)

#define TREESBESTOF         (4200)  /* works best if is multiple of TREESPLANTRATE*TICKSPERSEC */
#define TREESPLANTRATE      (10)

/* key masks */
#define ACCELMASK (0x00000001)
#define BRAKEMASK (0x00000002)
#define TURNLMASK (0x00000004)
#define TURNRMASK (0x00000008)
#define LMINEMASK (0x00000010)
#define SHOOTMASK (0x00000020)
#define INCREMASK (0x00000040)
#define DECREMASK (0x00000080)

/* regular expression errors */
#define EREG_OFFSET     (ELAST + 16)                   /* Add this to regualar expression errors */
#define EREG_NOMATCH    (EREG_OFFSET + EREG_NOMATCH)   /* The regexec() function failed to match */
#define EREG_BADPAT     (EREG_OFFSET + EREG_BADPAT)    /* invalid regular expression */
#define EREG_ECOLLATE   (EREG_OFFSET + EREG_ECOLLATE)  /* invalid collating element */
#define EREG_ECTYPE     (EREG_OFFSET + EREG_ECTYPE)    /* invalid character class */
#define EREG_EESCAPE    (EREG_OFFSET + EREG_EESCAPE)   /* `\' applied to unescapable character */
#define EREG_ESUBREG    (EREG_OFFSET + EREG_ESUBREG)   /* invalid backreference number */
#define EREG_EBRACK     (EREG_OFFSET + EREG_EBRACK)    /* brackets `[ ]' not balanced */
#define EREG_EPAREN     (EREG_OFFSET + EREG_EPAREN)    /* parentheses `( )' not balanced */
#define EREG_EBRACE     (EREG_OFFSET + EREG_EBRACE)    /* braces `{ }' not balanced */
#define EREG_BADBR      (EREG_OFFSET + EREG_BADBR)     /* invalid repetition count(s) in `{ }' */
#define EREG_ERANGE     (EREG_OFFSET + EREG_ERANGE)    /* invalid character range in `[ ]' */
#define EREG_ESPACE     (EREG_OFFSET + EREG_ESPACE)    /* ran out of memory */
#define EREG_BADRPT     (EREG_OFFSET + EREG_BADRPT)    /* `?', `*', or `+' operand invalid */
#define EREG_EMPTY      (EREG_OFFSET + EREG_EMPTY)     /* empty (sub)expression */
#define EREG_ASSERT     (EREG_OFFSET + EREG_ASSERT)    /* cannot happen - you found a bug */
#define EREG_INVARG     (EREG_OFFSET + EREG_INVARG)    /* invalid argument, e.g. negative-length string */
#define EREG_ILLSEQ     (EREG_OFFSET + EREG_ILLSEQ)    /* illegal byte sequence (bad multibyte character) */

enum {
  BUILDERNILL = -1,
  BUILDERTREE,
  BUILDERROAD,
  BUILDERWALL,
  BUILDERPILL,
  BUILDERMINE,
} ;

enum {
  MSGEVERYONE,
  MSGALLIES,
  MSGNEARBY,
  MSGGAME,
} ;

/* client message types */

enum {
  kHangupClientMessage,
  CLSENDMESG,
  CLDROPBOAT,
  CLDROPPILLS,
  CLDROPMINE,
  CLTOUCH,
  CLGRABTILE,
  CLGRABTREES,
  CLBUILDROAD,
  CLBUILDWALL,
  CLBUILDBOAT,
  CLBUILDPILL,
  CLREPAIRPILL,
  CLPLACEMINE,
  CLDAMAGE,
  CLSMALLBOOM,
  CLSUPERBOOM,
  CLREFUEL,
  CLHITTANK,
  CLSETALLIANCE,
} ;

/* join messages */
enum {
  kBadVersionJOIN,
  kDisallowJOIN,
  kBadPasswordJOIN,
  kServerFullJOIN,
  kServerTimeLimitReachedJOIN,
  kBannedPlayerJOIN,
  kSendingPreambleJOIN,
} ;

/* server messages */

enum {
  SRPLAYERJOIN,
  SRPLAYERREJOIN,
  SRPLAYEREXIT,
  SRPLAYERDISC,
  SRPLAYERKICK,
  SRPLAYERBAN,
  SRHANGUP,  // not used
  SRSENDMESG,
  SRDAMAGE,
  SRGRABTREES,
  SRBUILD,
  SRGROW,
  SRFLOOD,
  SRPLACEMINE,
  SRDROPMINE,
  SRDROPBOAT,
  SRREPAIRPILL,
  SRCOOLPILL,
  SRCAPTUREPILL,
  SRBUILDPILL,
  SRDROPPILL,
  SRREPLENISHBASE,
  SRCAPTUREBASE,
  SRREFUEL,
  SRGRABBOAT,
  SRMINEACK,
  SRBUILDERACK,
  SRSMALLBOOM,
  SRSUPERBOOM,
  SRHITTANK,
  SRSETALLIANCE,
  SRTIMELIMIT,
  SRBASECONTROL,
  SRPAUSE,
} ;

/* join status */
enum {
  kJoinRESOLVING,
  kJoinCONNECTING,
  kJoinSENDJOIN,
  kJoinRECVPREAMBLE,
  kJoinRECVMAP,
  kJoinSUCCESS,
  /* errors */

  /* dns errors */
  kJoinEHOSTNOTFOUND,
  kJoinEHOSTNORECOVERY,
  kJoinEHOSTNODATA,

  /* connecion errors */
  kJoinETIMEOUT,
  kJoinECONNREFUSED,
  kJoinENETUNREACH,
  kJoinEHOSTUNREACH,

  /* other errors */
  kJoinEBADVERSION,
  kJoinEDISALLOW,
  kJoinEBADPASS,
  kJoinESERVERFULL,
  kJoinETIMELIMIT,
  kJoinEBANNEDPLAYER,

  /* server errors */
  kJoinESERVERERROR,
  kJoinECONNRESET,
} ;

/* register tracker status */
enum {
  kRegisterRESOLVING,
  kRegisterCONNECTING,
  kRegisterSENDINGDATA,
  kRegisterTESTINGTCP,
  kRegisterTESTINGUDP,
  kRegisterSUCCESS,

  /* dns errors */
  kRegisterEHOSTNOTFOUND,
  kRegisterEHOSTNORECOVERY,
  kRegisterEHOSTNODATA,

  /* connection errors */
  kRegisterETIMEOUT,
  kRegisterECONNREFUSED,
  kRegisterEHOSTDOWN,
  kRegisterEHOSTUNREACH,

  kRegisterEBADVERSION,
  kRegisterETCPCLOSED,
  kRegisterEUDPCLOSED,

  kRegisterECONNRESET,
} ;

enum {
  kGetListTrackerRESOLVING,
  kGetListTrackerCONNECTING,
  kGetListTrackerREGISTERING,
  kGetListTrackerGETTINGLIST,
  kGetListTrackerSUCCESS,

  /* dns errors */
  kGetListTrackerEHOSTNOTFOUND,
  kGetListTrackerEHOSTNORECOVERY,
  kGetListTrackerEHOSTNODATA,

  /* connection errors */
  kGetListTrackerECONNABORTED,
  kGetListTrackerETIMEOUT,
  kGetListTrackerECONNREFUSED,
  kGetListTrackerEHOSTDOWN,
  kGetListTrackerEHOSTUNREACH,
  kGetListTrackerEPIPE,

  /* version error */
  kGetListTrackerEBADVERSION,
} ;

/* game types */
enum {
  kDominationGameType = 0,
  kCTFGameType,
  kKOTHGameType,
  kBallGameType,
  kBodyGameType,
} ;

/* domination types */
enum {
  kOpenGame = 0,
  kTournamentGame,
  kStrictGame,
} ;

/* udp updates */
enum {
  kTankDead = 0,
  kTankFireball,
  kTankOnBoat,
  kTankNormal,
} ;

/* builder status */

enum {
  kBuilderReady = 0,
  kBuilderGoto,
  kBuilderWork,
  kBuilderWait,
  kBuilderReturn,
  kBuilderParachute,
} ;

/* builder tasks */

enum {
  kBuilderDoNothing,
  kBuilderGetTree,
  kBuilderBuildRoad,
  kBuilderBuildWall,
  kBuilderBuildBoat,
  kBuilderBuildPill,
  kBuilderRepairPill,
  kBuilderPlaceMine,
} ;

/* sounds */

enum {
  kBubblesSound,
  kBuildSound,
  kBuilderDeathSound,
  kExplosionSound,
  kFarBuildSound,
  kFarBuilderDeathSound,
  kFarExplosionSound,
  kFarHitTankSound,
  kFarHitTerrainSound,
  kFarHitTreeSound,
  kFarShotSound,
  kFarSinkSound,
  kFarSuperBoomSound,
  kFarTreeSound,
  kHitTankSound,
  kHitTerrainSound,
  kHitTreeSound,
  kMineSound,
  kMsgReceivedSound,
  kPillShotSound,
  kSinkSound,
  kSuperBoomSound,
  kTankShotSound,
  kTreeSound
} ;

/* shared data structures */

struct Pill {
	int x;
	int y;
	int owner;
	int armour;
	int speed;
  int counter;
} ;

struct Base {
	int x;
	int y;
	int owner;
	int armour;
	int shells;
	int mines;
  int counter;
} ;

struct Start {
	int x;
	int y;
	int dir;
} ;

struct Shell {
  int owner;
  Vec2f point;
  int boat;
  int pill;
  float dir;
  float range;
} ;

struct Explosion {
  Vec2f point;
  int counter;
} ;

struct Domination {
  int type;
  int basecontrol;
} ;

struct JOIN_Preamble {
  uint8_t ident[8];  /* "XBOLOGAM" */
  uint8_t version;   /* currently 0 */
  char name[MAXNAME];   /* NULL terminated name */
  char pass[MAXPASS];   /* NULL terminated password */
} __attribute__((__packed__));

/*
 * Runningbolo() returns true if there is a game in progress.
 */
 
int allowjoinserver();

/*
 * Initbolo() needs to be called once to initialize bolo.  Returns 0 on
 * success or -1 on failure and sets errno.
 */

int initbolo(
    void (*setplayerstatusfunc)(int player),
    void (*setpillstatusfunc)(int pill),
    void (*setbasestatusfunc)(int base),
    void (*settankstatusfunc)(),
    void (*playsound)(int sound),
    void (*printmessagefunc)(int type, const char *text),
    void (*joinprogress)(int statuscode, float scale),
    void (*clientloopupdate)(void)
  );

/*
 * Closegame() closes the game and releases all game resources.
 * Returns 0 on success -1 on failure and sets errno.
 */

void pauseresumegame();
void togglejoingame();

int listtracker(
    const char trackerhostname[], struct ListNode *node,
    void(*trackerprogress)(int statuscode)
  );

void stoptracker();

Vec2f dir2vec(float dir);

/*
 * Calcvis() returns a value between 0.0 and 1.0 based on how visible it is.
 */

float fogvis(Vec2f v);
float forestvis(Vec2f v);
float calcvis(Vec2f v);

float vec2dir(Vec2f v);
Vec2f dir2vec(float f);

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#endif  /* __BOLO__ */
