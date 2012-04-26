/*
 *  server.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __SERVER__
#define __SERVER__

#include "bolo.h"
#include "tracker.h"
#include "buf.h"
#include "errchk.h"

#include <stdint.h>

#define TRACKERUPDATESECONDS (60)

struct Server {
  int setup;
  int running;

  /* tracker */
  struct {
    char *hostname;
    int sock;
    struct sockaddr_in addr;
    char hostplayername[MAXNAME];
    char mapname[TRKMAPNAMELEN];
    uint16_t port;
    void (*callback)(int status);
    struct Buf recvbuf;
    struct Buf sendbuf;
  } tracker;

  /* sockets */
  int listensock;
  int dgramsock;
  int mainpipe[2];
  int threadpipe[2];

  /* server mutex */
  pthread_mutex_t mutex;

  /* chain detonation */
  struct ListNode chains[CHAINTICKS + 1];

  /* flood fill */
  struct ListNode floods[FLOODTICKS + 1];

  /* game info */
  int gametype;
  int timelimit;
  int hiddenmines;
  int passreq;
  char pass[MAXPASS];
  int pauseonplayerexit;
  int allowjoin;
  int pause;

  /* used in domination games */
  int basecontrol;

  /* number of elapsed ticks */
  uint32_t ticks;

  union {
    struct Domination domination;
  } game;
  
  int terrain[WIDTH][WIDTH];

  int npills;
  struct Pill pills[MAXPILLS];

  int nbases;
  struct Base bases[MAXBASES];

  int nstarts;
  struct Start starts[MAX_STARTS];

  /* tree growing */
  int growx;
  int growy;
  int growbestof;

  /* joining player data */
  struct {
    int cntlsock;
    struct sockaddr_in addr;
    int disconnect;
    struct Buf recvbuf;
    struct Buf sendbuf;
  } joiningplayer;

  /* player info */
  struct {
    /* connection variables */
    int used;
    int cntlsock;
    struct sockaddr_in addr;
    struct sockaddr_in dgramaddr;

    char name[MAXNAME];
    char host[MAXHOST];
    uint32_t seq;
    uint32_t lastupdate;
    uint16_t alliance;
    Vec2f tank;

    struct Buf recvbuf;
    struct Buf sendbuf;
  } players[MAXPLAYERS];

  /* banned players list */
  struct ListNode bannedplayers;
} ;

/* Server Messages */

struct SRHangUp {
  uint8_t type;
} ;

struct SRSendMesg {
  uint8_t type;
  uint8_t player;
  uint8_t to;
  /* null terminated string */
} __attribute__((__packed__));

struct SRDamage {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
  uint8_t terrain;
} __attribute__((__packed__));

struct SRGrabTrees {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRBuild {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t terrain;
} __attribute__((__packed__));

struct SRGrow {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRFlood {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRPlaceMine {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRDropMine {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRDropBoat {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRPlayerJoin {
  uint8_t type;
  uint8_t player;
  char name[MAXNAME];
  char host[MAXHOST];
} __attribute__((__packed__));

struct SRPlayerRejoin {
  uint8_t type;
  uint8_t player;
  char host[MAXHOST];
} __attribute__((__packed__));

struct SRPlayerExit {
  uint8_t type;
  uint8_t player;
} __attribute__((__packed__));

struct SRPlayerDisc {
  uint8_t type;
  uint8_t player;
} __attribute__((__packed__));

struct SRPlayerKick {
  uint8_t type;
  uint8_t player;
} __attribute__((__packed__));

struct SRPlayerBan {
  uint8_t type;
  uint8_t player;
} __attribute__((__packed__));

struct SRRepairPill {
  uint8_t type;
  uint8_t pill;
  uint8_t armour;
} __attribute__((__packed__));

struct SRCoolPill {
  uint8_t type;
  uint8_t pill;
} __attribute__((__packed__));

struct SRCapturePill {
  uint8_t type;
  uint8_t pill;
	uint8_t owner;
} __attribute__((__packed__));

struct SRBuildPill {
  uint8_t type;
  uint8_t pill;
  uint8_t x;
  uint8_t y;
  uint8_t armour;
} __attribute__((__packed__));

struct SRDropPill {
  uint8_t type;
  uint8_t pill;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRReplenishBase {
  uint8_t type;
  uint8_t base;
} __attribute__((__packed__));

struct SRCaptureBase {
  uint8_t type;
  uint8_t base;
	uint8_t owner;
} __attribute__((__packed__));

struct SRRefuel {
  uint8_t type;
  uint8_t base;
  uint8_t armour;
  uint8_t shells;
  uint8_t mines;
} __attribute__((__packed__));

struct SRGrabBoat {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRMineAck {
  uint8_t type;
  uint8_t success;
} __attribute__((__packed__));

struct SRBuilderAck {
  uint8_t type;
  uint8_t mines;
  uint8_t trees;
  uint8_t pill;
} __attribute__((__packed__));

struct SRSmallBoom {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRSuperBoom {
  uint8_t type;
  uint8_t player;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct SRHitTank {
  uint8_t type;
  uint8_t player;
  uint32_t dir;
} __attribute__((__packed__));

struct SRSetAlliance {
  uint8_t type;
  uint8_t player;
  uint16_t alliance;
} __attribute__((__packed__));

struct SRTimeLimit {
  uint8_t type;
  uint16_t timeremaining;
} __attribute__((__packed__));

struct SRBaseControl {
  uint8_t type;
  uint16_t timeleft;
} __attribute__((__packed__));

struct SRPause {
  uint8_t type;
  uint8_t pause;
} __attribute__((__packed__));

/* structure for banned player list */
struct BannedPlayer {
  char name[MAXNAME];
  struct in_addr sin_addr;
} ;

/**********************/
/* external variables */
/**********************/

extern struct Server server;

/*******************/
/* server routines */
/*******************/

int initserver();
int setupserver(
    int initiallypaused, const void *buf, size_t nbytes, uint16_t port,
    const char password[], int timelimit, int hiddenmines, int pauseonplayerexit,
    int gametype, const void *gamedata
  );
int startserverthread();
int startserverthreadwithtracker(
    const char trackerhostname[], uint16_t port, const char hostplayername[],
    const char mapname[], void (*callback)(int status)
  );
int stopserver();
int lockserver();
int unlockserver();

int togglejoinserver();
int kickplayer(int player);
int banplayer(int player);
int unbanplayer(int player);

int tiletoterrain(int tile);

int pauseresumeserver();
int getservertcpport();
int getserverudpport();

/* lock server first before calling these */
int getallowjoinserver();
void setallowjoinserver(int allowjoin);

int getpauseserver();
void pauseserver();
void resumeserver();

#endif /* __SERVER__ */
