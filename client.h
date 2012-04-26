/*
 *  client.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __CLIENT__
#define __CLIENT__

#include "bolo.h"
#include "buf.h"

#include <stdint.h>
#include <netinet/in.h>

struct Client {
  /* hostname of the server */
  char *hostname;

  /* data that should not be accessed from main thread witout a lock */
  
  int pause;

  int mainpipe[2];
  int threadpipe[2];

  int hiddenmines;
  int gametype;

  struct Buf sendbuf;
  struct Buf recvbuf;

  union {
    struct Domination domination;
  } game;

  /* number of elapsed ticks */
  int timelimitreached;
  int basecontrolreached;

  char name[MAXNAME];

  int passreq;
  char pass[MAXPASS];

  int npills;
  struct Pill pills[MAXPILLS];

  int nbases;
  struct Base bases[MAXBASES];

  int nstarts;
  struct Start starts[MAX_STARTS];

  int terrain[WIDTH][WIDTH];
  int seentiles[WIDTH][WIDTH];
  int images[WIDTH][WIDTH];
  int fog[WIDTH][WIDTH];

  struct {
    int used;
    int connected;
    char name[MAXNAME];
    char host[MAXHOST];
    int32_t seq;
    int lastupdate;
    int dead;
    int boat;
    Vec2f tank;
    float dir;
    float speed;
    float turnspeed;

    float kickdir;
    float kickspeed;

    /* builder */
    int builderstatus;
    Vec2f builder;
    Pointi buildertarget;
    int builderwait;

    /* other */
    uint16_t alliance;
    int32_t inputflags;
    struct ListNode shells;
    struct ListNode explosions;
  } players[MAXPLAYERS];

  /* player id */
  int player;

  /* respawn counter */
  int respawncounter;
  int spawned;

  /* shell firing counter */
  int shellcounter;

  /* range of shells fired */
  float range;

  /* refueling */
  int refueling;
  int refuelingbase;
  int refuelingcounter;

  /* river drain */
  int draincounter;

  /* resources */
  int armour;
  int shells;
  int mines;
  int trees;

  /* player stats */
  int kills;
  int deaths;

  /* builder */
//  int nextbuildertask;
  int nextbuildercommand;
  Pointi nextbuildertarget;
  int buildertask;
  int buildermines;
  int buildertrees;
  int builderpill;

  /* sounds */
  int tankshotsound;
  int pillshotsound;
  int sinksound;
  int builderdeathsound;

  /* list of explosions */
  struct ListNode explosions;

  /* changed tiles */
  struct ListNode changedtiles;

  /* server address and sockets */
  struct sockaddr_in srvaddr;
  int cntlsock;
  int dgramsock;

  /* call backs */
  void (*setplayerstatus)(int player);
  void (*setpillstatus)(int pill);
  void (*setbasestatus)(int base);
  void (*settankstatus)();
  void (*playsound)(int sound);
  void (*printmessage)(int type, const char *text);
  void (*joinprogress)(int statuscode, float scale);
  void (*loopupdate)(void);
} ;

/* Client Messages */
struct CLHangUp {
  uint8_t type;
} __attribute__((__packed__));

struct CLSendMesg {
  uint8_t type;
  uint8_t to;
  int16_t mask;
  /* null terminated string */
} __attribute__((__packed__));

struct CLDropBoat {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLDropPills {
  uint8_t type;
  uint32_t x;
  uint32_t y;
  uint16_t pills;
} __attribute__((__packed__));

struct CLDropMine {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLTouch {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLGrabTile {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLGrabTrees {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLBuildRoad {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t trees;
} __attribute__((__packed__));

struct CLBuildWall {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t trees;
} __attribute__((__packed__));

struct CLBuildBoat {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t trees;
} __attribute__((__packed__));

struct CLBuildPill {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t trees;
  uint8_t pill;
} __attribute__((__packed__));

struct CLRepairPill {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t trees;
} __attribute__((__packed__));

struct CLPlaceMine {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t mines;
} __attribute__((__packed__));

struct CLDamage {
  uint8_t type;
  uint8_t x;
  uint8_t y;
  uint8_t boat;
} __attribute__((__packed__));

struct CLSmallBoom {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLSuperBoom {
  uint8_t type;
  uint8_t x;
  uint8_t y;
} __attribute__((__packed__));

struct CLRefuel {
  uint8_t type;
  uint8_t base;
  uint8_t armour;
  uint8_t shells;
  uint8_t mines;
} __attribute__((__packed__));

struct CLHitTank {
  uint8_t type;
  uint8_t player;
  uint32_t dir;
} __attribute__((__packed__));

struct CLSetAlliance {
  uint8_t type;
  uint16_t alliance;
} __attribute__((__packed__));

struct CLUpdateShell {
  uint8_t owner;
  uint16_t shellx;
  uint16_t shelly;
  uint8_t boat;
  uint8_t pill;
  uint8_t shelldir;
  uint16_t range;
} __attribute__((__packed__));

struct CLUpdateExplosion {
  uint16_t explosionx;
  uint16_t explosiony;
  uint8_t tile;
  uint8_t counter;
} __attribute__((__packed__));

struct CLUpdate {
  /* header */
  struct {
    uint8_t player;
    int32_t seq[MAXPLAYERS];
    uint8_t tankstatus;
    uint32_t tankx;
    uint32_t tanky;
    uint32_t tankspeed;
    uint32_t tankturnspeed;
    uint32_t tankkickdir;
    uint32_t tankkickspeed;
    uint8_t tankdir;
    uint8_t builderstatus;
    uint32_t builderx;
    uint32_t buildery;
    uint8_t buildertargetx;
    uint8_t buildertargety;
    uint8_t builderwait;
    int32_t inputflags;
    uint8_t tankshotsound;
    uint8_t pillshotsound;
    uint8_t sinksound;
    uint8_t builderdeathsound;
    uint8_t nshells;
    uint8_t nexplosions;
  } __attribute__((__packed__)) hdr;
  uint8_t buf[CLUPDATEMAXSHELLS*sizeof(struct CLUpdateShell) + CLUPDATEMAXEXPLOSIONS*sizeof(struct CLUpdateExplosion)];
} __attribute__((__packed__));

/* call once during program startup to setup callbacks */
int initclient(void (*setplayerstatusfunc)(int player), void (*setpillstatusfunc)(int pill), void (*setbasestatusfunc)(int base), void (*settankstatusfunc)(), void (*playsound)(int sound), void (*printmessagefunc)(int type, const char *text), void (*joinprogress)(int statuscode, float scale), void (*loopupdate)(void));

/* call to start running client */
int startclient(const char *hostname, uint16_t tcpport, const char playername[], const char password[]);

/* call to stop running client */
int stopclient();

/* used to lock struct Client client for reading */
int lockclient();
int unlockclient();

/* keyboard events */
int keyevent(int mask, int set);

/* builder commands */
void buildercommand(int type, Pointi p);

/* alliance requests */
int requestalliance(uint16_t withplayers);
int leavealliance(uint16_t withplayers);

/* chat messages */
int sendmessage(const char *text, int to);

/* called when the view has been updated */
void clearchangedtiles();

/* set when client is running */
extern int client_running;

/* client data */
extern struct Client client;

#endif  /* __CLIENT__ */
