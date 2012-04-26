/*
 *  client.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "client.h"
#include "server.h"
#include "terrain.h"
#include "tiles.h"
#include "images.h"
#include "bmap_client.h"
#include "io.h"
#include "errchk.h"
#include "timing.h"
#include "resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**********************/
/* external variables */
/**********************/

int client_running = 0;
struct Client client;

/********************/
/* static variables */
/********************/

static struct {
  /* client mutex */
  pthread_mutex_t mutex;
} iclient;

static Pointi target;
int buildertask;
int collisionowner;

/*******************/
/* static routines */
/*******************/

static int cleanupclient();

/* client receive routines */

static int recvsrpause();
static int recvsrsendmesg();
static int recvsrdamage();
static int recvsrgrabtrees();
static int recvsrbuild();
static int recvsrgrow();
static int recvsrflood();
static int recvsrplacemine();
static int recvsrdropmine();
static int recvsrdropboat();
static int recvsrplayerjoin();
static int recvsrplayerrejoin();
static int recvsrplayerexit();
static int recvsrplayerdisc();
static int recvsrplayerkick();
static int recvsrplayerban();
static int recvsrrepairpill();
static int recvsrcoolpill();
static int recvsrcapturepill();
static int recvsrbuildpill();
static int recvsrdroppill();
static int recvsrreplenishbase();
static int recvsrcapturebase();
static int recvsrrefuel();
static int recvsrgrabboat();
static int recvsrmineack();
static int recvsrbuilderack();
static int recvsrsmallboom();
static int recvsrsuperboom();
static int recvsrhittank();
static int recvsrsetalliance();
static int recvsrtimelimit();
static int recvsrbasecontrol();

/* client send routines */

static int sendcldropboat(int x, int y);
static int sendcldroppills(float x, float y, uint16_t pills);
static int sendcldropmine(int x, int y);
static int sendcltouch(int x, int y);
static int sendclgrabtile(int x, int y);
static int sendclgrabtrees(int x, int y);
static int sendclbuildroad(int x, int y, int trees);
static int sendclbuildwall(int x, int y, int trees);
static int sendclbuildboat(int x, int y, int trees);
static int sendclbuildpill(int x, int y, int trees, int pill);
static int sendclrepairpill(int x, int y, int trees);
static int sendclplacemine(int x, int y);
static int sendcldamage(int x, int y, int boat);
static int sendclsmallboom(int x, int y);
static int sendclsuperboom(int x, int y);
static int sendclrefuel(int base, int armour, int shells, int mines);
static int sendclhittank(int player, float dir);
static int sendclupdate();

/* builder speed routines */

static float maxspeed(int x, int y);
static float maxturnspeed(int x, int y);
static float builderspeed(int x, int y, int player);
static float buildertargetspeed(int x, int y);

/* visibility routines */

static int decreasevis(Recti r);
static int increasevis(Recti r);

/* refreshes a changed square */
static int refresh(int x, int y);

/* terrain physics */
static int enter(Pointi new, Pointi old);

/* starts the player */
static int spawn();

/* game logic routines */
static int tankmovelogic(int player);
static int tanklocallogic(Pointi old);
static int builderlogic(int player);
static int pilllogic(Vec2f old);
static int shelllogic(int player);
static int explosionlogic(int player);

static int testhiddenmine(int x, int y);

/* convert world scale to tile */
static int w2t(float f);

/* converts radians to rudimentary direction */
static float rounddir(float dir);

/*  */
static Vec2f collisiondetect(Vec2f p, float radius, int (*func)(Pointi square));
static int tankcollision(Pointi square);
static int buildercollision(Pointi square);

/* kill builder routines */
static int killsquarebuilder(Pointi p);
static int killpointbuilder(Vec2f p);
static int killbuilder();

/* terrain to tile mapping functions */
static int tilefor(int x, int y);
static int fogtilefor(int x, int y, int tile);

/* tank killing routines */
static int killtank();
static int drown();
static int smallboom();
static int superboom();

/* pill and base finding routines */
static int findpill(int x, int y);
static int findbase(int x, int y);

static int printmessage(int type, const char *body);

static int shellcollisiontest(struct Shell *shell, int player);
static int tanktest(int x, int y);
static int tankonaboattest(int x, int y);
static int testalliance(int p1, int p2);
static int circlesquare(Vec2f point, float radius, Pointi square);

static int getbuildertaskforcommand(int command, Pointi at);

/* client pthread routines */
static void *clientmainthread(void *);

static int dgramclient();

static int joinclient();

int initclient(void (*setplayerstatusfunc)(int player), void (*setpillstatusfunc)(int pill), void (*setbasestatusfunc)(int base), void (*settankstatusfunc)(), void (*playsound)(int sound), void (*printmessagefunc)(int type, const char *text), void (*joinprogress)(int statuscode, float scale), void (*loopupdate)(void)) {
  int err;

TRY
  client.hostname = NULL;
  client.hiddenmines = 1;
  client.player = -1;

  if ((err = pthread_mutex_init(&iclient.mutex, NULL))) LOGFAIL(err)

  client.setplayerstatus = setplayerstatusfunc;
  client.setpillstatus = setpillstatusfunc;
  client.setbasestatus = setbasestatusfunc;
  client.settankstatus = settankstatusfunc;
  client.playsound = playsound;
  client.printmessage = printmessagefunc;
  client.joinprogress = joinprogress;
  client.loopupdate = loopupdate;

  client.cntlsock = -1;
  client.dgramsock = -1;

  initlist(&client.changedtiles);
  initlist(&client.explosions);

  if (initbuf(&client.sendbuf)) LOGFAIL(errno)
  if (initbuf(&client.recvbuf)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, errno)
END
}

int startclient(const char *hostname, uint16_t port, const char playername[], const char password[]) {
  int y, x, i;
  int flags;
  pthread_t thread;
  int err;

  assert(!client_running);

TRY
  client.timelimitreached = 0;
  client.basecontrolreached = 0;
  client.spawned = 0;
  client.kills = 0;
  client.deaths = 0;

  client.buildermines = 0;
  client.buildertrees = 0;
  client.builderpill = NOPILL;

  /* initialize the players */
  for (i = 0; i < MAXPLAYERS; i++) {
    client.players[i].used = 0;
    client.players[i].connected = 0;

    bzero(client.players[i].name, MAXNAME);
    bzero(client.players[i].host, MAXHOST);

    client.players[i].seq = 0;
    client.players[i].lastupdate = 0;
    client.players[i].dead = 1;
    client.players[i].boat = 1;
    client.players[i].tank = make2f(0.0, 0.0);
    client.players[i].dir = 0.0;
    client.players[i].speed = 0.0;
    client.players[i].turnspeed = 0.0;
    client.players[i].kickdir = 0.0;
    client.players[i].kickspeed = 0.0;
    client.players[i].builderstatus = kBuilderReady;
    client.players[i].builder = make2f(0.0, 0.0);
    client.players[i].buildertarget = makepoint(0, 0);
    client.players[i].builderwait = 0;
    client.players[i].alliance = 0;
    client.players[i].inputflags = 0;
    initlist(&client.players[i].shells);
    initlist(&client.players[i].explosions);

    if (client.setplayerstatus) {
      client.setplayerstatus(i);
    }
  }

  /* initialize the player variables */
  client.players[client.player].inputflags = 0;

  /* copy name and password */
  strncpy(client.name, playername, sizeof(client.name) - 1);

  if ((client.passreq = password != NULL)) {
    strncpy(client.pass, password, MAXPASS - 1);
  }
  else {
    client.pass[0] = '\0';
  }

  /* make the screen blank */
  for (y = 0; y < WIDTH; y++) {
    for (x = 0; x < WIDTH; x++) {
      client.images[y][x] = SEAA00IMAGE;
      client.fog[y][x] = 0;
    }
  }

  /* copy hostname */
  if ((client.hostname = (char *)malloc(strlen(hostname) + 1)) == NULL) LOGFAIL(errno)
  strcpy(client.hostname, hostname);

  /* set port number */
  client.srvaddr.sin_port = htons(port);

  /* create the client.cntlsock */
  if ((client.cntlsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)
  if ((flags = fcntl(client.cntlsock, F_GETFL, 0)) == -1) LOGFAIL(errno)
  if (fcntl(client.cntlsock, F_SETFL, flags | O_NONBLOCK)) LOGFAIL(errno);

  /* create the client.dgramsock */
  if ((client.dgramsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) LOGFAIL(errno)
  if ((flags = fcntl(client.dgramsock, F_GETFL, 0)) == -1) LOGFAIL(errno)
  if (fcntl(client.dgramsock, F_SETFL, flags | O_NONBLOCK)) LOGFAIL(errno);

  /* clear buffers */
  if (readbuf(&client.sendbuf, NULL, client.sendbuf.nbytes) == -1) LOGFAIL(errno)
  if (readbuf(&client.recvbuf, NULL, client.recvbuf.nbytes) == -1) LOGFAIL(errno)

  /* initialize control pipes */
  if (pipe(client.mainpipe)) LOGFAIL(errno)
  if (pipe(client.threadpipe)) LOGFAIL(errno)

  /* create the mainthread */
  if ((err = pthread_create(&thread, NULL, clientmainthread, NULL))) LOGFAIL(err)
  if ((err = pthread_detach(thread))) LOGFAIL(err)

  client_running = 1;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (client.cntlsock != -1) {
      while (close(client.cntlsock) && errno == EINTR);
      client.cntlsock = -1;
    }

    if (client.dgramsock != -1) {
      while (close(client.dgramsock) && errno == EINTR);
      client.dgramsock = -1;
    }

    RETERR(-1)
  }
END
}

int stopclient() {
  char *buf[10];
  ssize_t r;

  assert(client_running);

TRY
  /* close mainpipe */
  if (closesock(client.mainpipe + 1)) LOGFAIL(errno)

  /* wait for threadpipe to be closed */
  for (;;) {
    if ((r = read(client.threadpipe[0], buf, sizeof(buf))) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
    }
    else if (r == 0) {
      break;
    }
  }

  if (closesock(client.threadpipe)) LOGFAIL(errno)

  client_running = 0;

CLEANUP
ERRHANDLER(0, -1)
END
}

int cleanupclient() {
  int i;

TRY
  free(client.hostname);
  client.hostname = NULL;

  client.player = -1;

  if (closesock(&client.cntlsock)) LOGFAIL(errno)
  if (closesock(&client.dgramsock)) LOGFAIL(errno)
  clearlist(&client.changedtiles, free);

  for (i = 0; i < MAXPLAYERS; i++) {
    client.players[i].builderstatus = kBuilderReady;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int lockclient() {
  int err;

TRY
  if ((err = pthread_mutex_lock(&iclient.mutex))) LOGFAIL(err)

CLEANUP
ERRHANDLER(0, -1)
END
}

int unlockclient() {
  int err;

TRY
  if ((err = pthread_mutex_unlock(&iclient.mutex))) LOGFAIL(err)

CLEANUP
ERRHANDLER(0, -1)
END
}

int runclient() {
  int i;
  Vec2f old;

TRY
  if (client.timelimitreached || client.basecontrolreached || client.pause) {
    SUCCESS;
  }

  client.players[client.player].seq++;
  old = client.players[client.player].tank;

  /* update status of lagged players */
  if (client.setplayerstatus) {
    for (i = 0; i < MAXPLAYERS; i++) {
      if (client.players[client.player].seq - client.players[i].lastupdate == 3*TICKSPERSEC) {
        client.setplayerstatus(i);
      }
      else if (client.players[client.player].seq - client.players[i].lastupdate == TICKSPERSEC) {
        client.setplayerstatus(i);
      }
    }
  }

  /* move tanks */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected && client.players[i].seq != 0) {
      Pointi old, new;

      old = makepoint(client.players[i].tank.x, client.players[i].tank.y);
      if (tankmovelogic(i)) LOGFAIL(errno)
      new = makepoint(client.players[i].tank.x, client.players[i].tank.y);

      if (!isequalpoint(new, old) && testalliance(client.player, i)) {
        if (increasevis(makerect(new.x - 14, new.y - 14, 29, 29))) LOGFAIL(errno)
        if (decreasevis(makerect(old.x - 14, old.y - 14, 29, 29))) LOGFAIL(errno)
      }
    }
  }

  /* local tank logic */
  if (tanklocallogic(makepoint(old.x, old.y))) LOGFAIL(errno)

  /* builder logic */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (builderlogic(i)) LOGFAIL(errno)
  }

  /* pill logic */
  if (pilllogic(old)) LOGFAIL(errno)

  /* perform shell logic */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (shelllogic(i)) LOGFAIL(errno)
  }

  /* perform explosion logic */
  for (i = -1; i < MAXPLAYERS; i++) {
    if (explosionlogic(i)) LOGFAIL(errno)
  }

  /* send updated location */
  if (client.players[client.player].seq%5 == 0) {
    if (sendclupdate()) LOGFAIL(errno)
  }
    
  /* loop update callback */
  client.loopupdate();

CLEANUP
ERRHANDLER(0, -1)
END
}

int joinclient() {
  int i;
  struct JOIN_Preamble joinpreamble;
  struct BOLO_Preamble bolopreamble;
  uint8_t msg;
  int ret;
  int lookup = -1;
  int gotlock = 0;

TRY
  /* see if already a dot quad */
  client.srvaddr.sin_addr.s_addr = inet_addr(client.hostname);

  /* not a dot quad, resolve */
  if (client.srvaddr.sin_addr.s_addr == INADDR_NONE) {
    if (client.joinprogress) {
      client.joinprogress(kJoinRESOLVING, 0.0);
    }

    /* lookup address */
    if ((lookup = nslookup(client.hostname)) == -1) LOGFAIL(errno)

    /* wait for exit or dns reply */
    if ((ret = selectreadread(client.mainpipe[0], lookup)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      if (closesock(&lookup)) LOGFAIL(errno)
      SUCCESS
    }

    /* get nslookup result */
    if (nslookupresult(lookup, &client.srvaddr.sin_addr)) {
      LOGFAIL(errno)
    }

    if (closesock(&lookup)) LOGFAIL(errno)
  }

  /* initialize name to addr and port */
  client.srvaddr.sin_family = AF_INET;
  bzero(client.srvaddr.sin_zero, 8);

  /* connect to server */
  if (client.joinprogress) {
    client.joinprogress(kJoinCONNECTING, 0.0);
  }
  
  if ((connect(client.cntlsock, (struct sockaddr *)&client.srvaddr, INET_ADDRSTRLEN))) {
    if (errno != EINPROGRESS) LOGFAIL(errno)
  }
  
  /* wait for connection to complete */
  for (;;) {
    int nfds;
    fd_set readfds;
    fd_set writefds;
    
    /* select until we have success */
    for (;;) {
      nfds = 0;
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);

      FD_SET(client.mainpipe[0], &readfds);
      nfds = MAX(nfds, client.mainpipe[0]);

      FD_SET(client.cntlsock, &writefds);
      nfds = MAX(nfds, client.cntlsock);
      
      if ((nfds = select(nfds + 1, &readfds, &writefds, NULL, NULL)) == -1) {
        if (errno != EINTR) LOGFAIL(errno)
      }
      else {
        break;
      }
    }
    
    if (FD_ISSET(client.mainpipe[0], &readfds)) {
      ret = 1;
      SUCCESS
    }
    else if (FD_ISSET(client.cntlsock, &writefds)) {
      if (connect(client.cntlsock, (struct sockaddr *)&client.srvaddr, INET_ADDRSTRLEN) && errno != EISCONN) LOGFAIL(errno)
      break;
    }
  }

  /* get local address of cntlsock */
  struct sockaddr_in saddr;
  socklen_t addrlen;

  addrlen = INET_ADDRSTRLEN;
  if (getsockname(client.cntlsock, (struct sockaddr *)&saddr, &addrlen)) LOGFAIL(errno)

  /* bind dgramsock to local address */
  while (bind(client.dgramsock, (struct sockaddr *)&saddr, addrlen)) {
    if (errno != EAGAIN) {
      LOGFAIL(errno)
    }

    usleep(10000);
  }

  /* set remote end for dgramsock */
  if ((connect(client.dgramsock, (struct sockaddr *)&client.srvaddr, INET_ADDRSTRLEN))) LOGFAIL(errno)

  bcopy(NET_GAME_IDENT, joinpreamble.ident, sizeof(NET_GAME_IDENT));
  joinpreamble.version = NET_GAME_VERSION;
  bzero(joinpreamble.name, MAXNAME);
  strncpy(joinpreamble.name, client.name, sizeof(joinpreamble.name) - 1);
  bzero(joinpreamble.pass, MAXPASS);
  strncpy(joinpreamble.pass, client.pass, sizeof(joinpreamble.pass) - 1);

  if (client.joinprogress) {
    client.joinprogress(kJoinSENDJOIN, 0.0);
  }

  /* send join preamble */
  if (writebuf(&client.sendbuf, &joinpreamble, sizeof(joinpreamble)) == -1) LOGFAIL(errno)

  if ((ret = cntlsend(client.mainpipe[0], client.cntlsock, &client.sendbuf)) == -1) {
    LOGFAIL(errno)
  }
  else if (ret == 1) {
    SUCCESS
  }

  if (client.joinprogress) {
    client.joinprogress(kJoinRECVPREAMBLE, 0.0);
  }

  /* recv msg */
  if ((ret = cntlrecv(client.mainpipe[0], client.cntlsock, &client.recvbuf, sizeof(msg))) == -1) {
    LOGFAIL(errno)
  }
  else if (ret == 1) {
    SUCCESS
  }

  if (readbuf(&client.recvbuf, &msg, sizeof(msg)) == -1) LOGFAIL(errno)

  if (msg == kBadVersionJOIN) FAIL(EBADVERSION)
  if (msg == kDisallowJOIN) FAIL(EDISSALLOW)
  if (msg == kBadPasswordJOIN) FAIL(EBADPASS)
  if (msg == kServerFullJOIN) FAIL(ESERVERFULL)
  if (msg == kServerTimeLimitReachedJOIN) FAIL(ETIMELIMIT)
  if (msg == kBannedPlayerJOIN) FAIL(EBANNEDPLAYER)
  if (msg != kSendingPreambleJOIN) LOGFAIL(ESERVERERROR)

  /* recv preamble */
  if ((ret = cntlrecv(client.mainpipe[0], client.cntlsock, &client.recvbuf, sizeof(bolopreamble))) == -1) {
    LOGFAIL(errno)
  }
  else if (ret == 1) {
    SUCCESS
  }

  if (readbuf(&client.recvbuf, &bolopreamble, sizeof(bolopreamble)) == -1) LOGFAIL(errno)

  /* convert byte order */
  bolopreamble.maplen = ntohl(bolopreamble.maplen);

  /* recv map data */
  ssize_t r;
  if ((r = cntlrecv(client.mainpipe[0], client.cntlsock, &client.recvbuf, bolopreamble.maplen)) == -1) {
    LOGFAIL(errno)
  }
  else if (r == 1) {
    SUCCESS
  }
  if (client.joinprogress) {
    client.joinprogress(kJoinRECVMAP, 100.0);
  }
    
  /*
  while (client.recvbuf.nbytes < bolopreamble.maplen) {
    ssize_t r;

    if ((r = cntlrecv(client.mainpipe[0], client.cntlsock, &client.recvbuf, MIN(bolopreamble.maplen - client.recvbuf.nbytes, RECVMAPBLOCKSIZE))) == -1) {
      LOGFAIL(errno)
    }
    else if (r == 1) {
      SUCCESS
    }

    if (client.joinprogress) {
      client.joinprogress(kJoinRECVMAP, (client.recvbuf.nbytes*100.0)/bolopreamble.maplen);
    }
  }
  */

  /* modify client */
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  client.player = bolopreamble.player;
  client.hiddenmines = bolopreamble.hiddenmines;

  if (bolopreamble.pause == 255) {
    client.pause = -1;
  }
  else {
    client.pause = bolopreamble.pause;
  }

  client.gametype = bolopreamble.gametype;

  if (bolopreamble.gametype == kDominationGameType) {
    /* the number of seconds to have control of all bases to win */
    switch (bolopreamble.game.domination.type) {
    case kOpenGame:
    case kTournamentGame:
    case kStrictGame:
      client.game.domination.type = bolopreamble.game.domination.type;
      break;

    default:
      assert(0);
    }

    client.game.domination.basecontrol = bolopreamble.game.domination.basecontrol;
  }
  else {
    assert(0);
  }

  /* initialize the players */
  for (i = 0; i < MAXPLAYERS; i++) {
    client.players[i].used = bolopreamble.players[i].used;
    client.players[i].connected = bolopreamble.players[i].connected;
    client.players[i].seq = ntohl(bolopreamble.players[i].seq);

    strncpy(client.players[i].name, (char *)bolopreamble.players[i].name, MAXNAME - 1);
    strncpy(client.players[i].host, (char *)bolopreamble.players[i].host, MAXHOST - 1);

    client.players[i].builderstatus = kBuilderReady;
    client.players[i].alliance = ntohs(bolopreamble.players[i].alliance);

    if (client.setplayerstatus) {
      client.setplayerstatus(i);
    }
  }

  /* load map data */
  if (clientloadmap(client.recvbuf.ptr, bolopreamble.maplen)) LOGFAIL(errno)
  if (readbuf(&client.recvbuf, NULL, bolopreamble.maplen) == -1) LOGFAIL(errno)

  spawn();
  if (increasevis(makerect(((int)client.players[bolopreamble.player].tank.x) - 14, ((int)client.players[bolopreamble.player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
//  if (increasevis(makerect(0, 0, WIDTH, WIDTH)) == -1) LOGFAIL(errno) /* for cheaters */

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

  /* update pill status */
  if (client.setpillstatus) {
    for (i = 0; i < client.npills; i++) {
      client.setpillstatus(i);
    }
  }

  /* update base status */
  if (client.setbasestatus) {
    for (i = 0; i < client.nbases; i++) {
      client.setbasestatus(i);
    }
  }

  /* join complete */
  if (client.joinprogress) {
    client.joinprogress(kJoinSUCCESS, 100.0);
  }

  ret = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(ret)

  default:
    if (lookup != -1) {
      closesock(&lookup);
    }

    if (gotlock) {
      unlockclient();
    }

    RETERR(-1)
  }
END
}

int recvclient() {
TRY
  /* receive loop */
  for (;;) {
    if (client.recvbuf.nbytes < sizeof(uint8_t)) FAIL(EAGAIN)

    switch (*(uint8_t *)client.recvbuf.ptr) {
    case SRPAUSE:
      if (recvsrpause()) FAIL(errno)
      break;

    case SRSENDMESG:
      if (recvsrsendmesg()) FAIL(errno)
      break;

    case SRDAMAGE:
      if (recvsrdamage()) FAIL(errno)
      break;

    case SRGRABTREES:
      if (recvsrgrabtrees()) FAIL(errno)
      break;

    case SRBUILD:
      if (recvsrbuild()) FAIL(errno)
      break;

    case SRGROW:
      if (recvsrgrow()) FAIL(errno)
      break;

    case SRFLOOD:
      if (recvsrflood()) FAIL(errno)
      break;

    case SRPLACEMINE:
      if (recvsrplacemine()) FAIL(errno)
      break;

    case SRDROPMINE:
      if (recvsrdropmine()) FAIL(errno)
      break;

    case SRDROPBOAT:
      if (recvsrdropboat()) FAIL(errno)
      break;

    case SRPLAYERJOIN:
      if (recvsrplayerjoin()) FAIL(errno)
      break;

    case SRPLAYERREJOIN:
      if (recvsrplayerrejoin()) FAIL(errno)
      break;

    case SRPLAYEREXIT:
      if (recvsrplayerexit()) FAIL(errno)
      if (!client.players[client.player].connected) SUCCESS
      break;

    case SRPLAYERDISC:
      if (recvsrplayerdisc()) FAIL(errno)
      if (!client.players[client.player].connected) SUCCESS
      break;

    case SRPLAYERKICK:
      if (recvsrplayerkick()) FAIL(errno)
      if (!client.players[client.player].connected) SUCCESS
      break;

    case SRPLAYERBAN:
      if (recvsrplayerban()) FAIL(errno)
      if (!client.players[client.player].connected) SUCCESS
      break;

    case SRREPAIRPILL:
      if (recvsrrepairpill()) FAIL(errno)
      break;

    case SRCOOLPILL:
      if (recvsrcoolpill()) FAIL(errno)
      break;

    case SRCAPTUREPILL:
      if (recvsrcapturepill()) FAIL(errno)
      break;

    case SRBUILDPILL:
      if (recvsrbuildpill()) FAIL(errno)
      break;

    case SRDROPPILL:
      if (recvsrdroppill()) FAIL(errno)
      break;

    case SRREPLENISHBASE:
      if (recvsrreplenishbase()) FAIL(errno)
      break;

    case SRCAPTUREBASE:
      if (recvsrcapturebase()) FAIL(errno)
      break;

    case SRREFUEL:
      if (recvsrrefuel()) FAIL(errno)
      break;

    case SRGRABBOAT:
      if (recvsrgrabboat()) FAIL(errno)
      break;

    case SRMINEACK:
      if (recvsrmineack()) FAIL(errno)
      break;

    case SRBUILDERACK:
      if (recvsrbuilderack()) FAIL(errno)
      break;

    case SRSMALLBOOM:
      if (recvsrsmallboom()) FAIL(errno)
      break;

    case SRSUPERBOOM:
      if (recvsrsuperboom()) FAIL(errno)
      break;

    case SRHITTANK:
      if (recvsrhittank()) FAIL(errno)
      break;

    case SRSETALLIANCE:
      if (recvsrsetalliance()) FAIL(errno)
      break;

    case SRTIMELIMIT:
      if (recvsrtimelimit()) FAIL(errno)
      break;

    case SRBASECONTROL:
      if (recvsrbasecontrol()) FAIL(errno)
      break;

    default:
      LOGFAIL(EINVAL)
      break;
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int selectclient(fd_set *readfds, fd_set *writefds, struct timeval *timeout) {
  int nfds;

TRY
  nfds = 0;
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  
  /* read on mainpipe */
  FD_SET(client.mainpipe[0], readfds);
  nfds = MAX(nfds, client.mainpipe[0]);
  
  /* read on dgram sock */
  FD_SET(client.dgramsock, readfds);
  nfds = MAX(nfds, client.dgramsock);
  
  /* read/write cntlsock */
  FD_SET(client.cntlsock, readfds);
  nfds = MAX(nfds, client.cntlsock);
      
  if (client.sendbuf.nbytes > 0) {
    FD_SET(client.cntlsock, writefds);
  }

  if ((nfds = select(nfds + 1, readfds, writefds, NULL, timeout)) == -1) {
    if (errno != EINTR) {
      LOGFAIL(errno)
    }

    nfds = 0;
  }
  
CLEANUP
ERRHANDLER(nfds, -1)
END
}

void *clientmainthread(void *arg) {
  int i;
  uint64_t currenttime;
  uint64_t nexttick;
  int gotlock = 0;

TRY
  if ((i = joinclient()) == -1) {
    LOGFAIL(errno)
  }
  else if (i == 1) {
    SUCCESS
  }

  /* set TCP_NODELAY on socket */
  i = 1;
  if (setsockopt(client.cntlsock, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i))) LOGFAIL(errno)

  /* initialize time */
  nexttick = getcurrenttime();

  /* main loop */
  for (;;) {
    if ((currenttime = getcurrenttime()) < nexttick) {
      int nfds;
      fd_set readfds;
      fd_set writefds;
      struct timeval timeout;

      timeout.tv_sec = 0;
      timeout.tv_usec = (nexttick - currenttime)/1000;

      if ((nfds = selectclient(&readfds, &writefds, &timeout)) == -1) LOGFAIL(errno)

      /* time to quit */
      if (FD_ISSET(client.mainpipe[0], &readfds)) {
        break;
      }

      /* we have i/o */
      if (nfds > 0) {
        if (FD_ISSET(client.dgramsock, &readfds)) {
          /* process dgram packets */
          if (lockclient()) LOGFAIL(errno)
          gotlock = 1;

          if (dgramclient()) LOGFAIL(errno)

          if (unlockclient()) LOGFAIL(errno)
          gotlock = 1;
        }

        if (FD_ISSET(client.cntlsock, &readfds)) {
          ssize_t r;

          if ((r = recvbuf(&client.recvbuf, client.cntlsock)) == -1) {
            if (errno == ECONNRESET) {
              CLEARERRLOG
              /* process remaining buf and disconnect */
              if (lockclient()) LOGFAIL(errno)
              gotlock = 1;

              if (recvclient()) {
                if (errno != EAGAIN) {
                  LOGFAIL(errno)
                }
                else {
                  CLEARERRLOG
                }
              }
              if (printmessage(MSGGAME, "disconnected")) LOGFAIL(errno)

              break;
            }
            else {
              LOGFAIL(errno)
            }
          }
          else if (r == 0) {  /* socket closed. */
            /* process remaining buf and disconnect */
            if (lockclient()) LOGFAIL(errno)
            gotlock = 1;

            if (recvclient()) {
              if (errno != EAGAIN) {
                LOGFAIL(errno)
              }
              else {
                CLEARERRLOG
              }
            }
            if (printmessage(MSGGAME, "disconnected")) LOGFAIL(errno)

            break;
          }
          else {
            /* process buf */
            if (lockclient()) LOGFAIL(errno)
            gotlock = 1;

            if (recvclient()) {  /* returns errno of EAGAIN until exit, disconnected, kick or ban received for this player */
              if (errno != EAGAIN) {
                LOGFAIL(errno)
              }
              else {
                CLEARERRLOG
              }
            }
            else {  /* player has exited, disconnected, kicked or banned */
              if (printmessage(MSGGAME, "disconnected")) LOGFAIL(errno)
              break;
            }

            if (unlockclient()) LOGFAIL(errno)
            gotlock = 0;
          }
        }

        if (FD_ISSET(client.cntlsock, &writefds)) {
          ssize_t r;
          if ((r = sendbuf(&client.sendbuf, client.cntlsock)) == -1) LOGFAIL(errno)
        }
      }
    }
    else {
      if (lockclient()) LOGFAIL(errno)
      gotlock = 1;

      if (runclient()) LOGFAIL(errno)

      if (unlockclient()) LOGFAIL(errno)
      gotlock = 0;

      currenttime = getcurrenttime();
      nexttick = nexttick + (1000000000ull/TICKSPERSEC);

      if (currenttime >= nexttick) {
        nexttick = currenttime + (1000000000ull/TICKSPERSEC);
      }
    }
  }

CLEANUP
  if (!gotlock) {
    lockclient();
  }

  cleanupclient();
  unlockclient();

  switch (ERROR) {
  case 0:
    break;

  case EHOSTNOTFOUND:
    if (client.joinprogress) {
      client.joinprogress(kJoinEHOSTNOTFOUND, 0.0);
    }

    break;

  case EHOSTNORECOVERY:
    if (client.joinprogress) {
      client.joinprogress(kJoinEHOSTNORECOVERY, 0.0);
    }

    break;

  case EHOSTNODATA:
    if (client.joinprogress) {
      client.joinprogress(kJoinEHOSTNODATA, 0.0);
    }

    break;

  case ETIMEDOUT:
    if (client.joinprogress) {
      client.joinprogress(kJoinETIMEOUT, 0.0);
    }

    break;

  case ECONNREFUSED:
    if (client.joinprogress) {
      client.joinprogress(kJoinECONNREFUSED, 0.0);
    }

    break;

  case ENETUNREACH:
    if (client.joinprogress) {
      client.joinprogress(kJoinENETUNREACH, 0.0);
    }

    break;

  case EHOSTUNREACH:
    if (client.joinprogress) {
      client.joinprogress(kJoinEHOSTUNREACH, 0.0);
    }

    break;

  case EBADVERSION:
    if (client.joinprogress) {
      client.joinprogress(kJoinEBADVERSION, 0.0);
    }

    break;

  case EDISSALLOW:
    if (client.joinprogress) {
      client.joinprogress(kJoinEDISALLOW, 0.0);
    }

    break;

  case EBADPASS:
    if (client.joinprogress) {
      client.joinprogress(kJoinEBADPASS, 0.0);
    }

    break;

  case ESERVERFULL:
    if (client.joinprogress) {
      client.joinprogress(kJoinESERVERFULL, 0.0);
    }

    break;

  case ETIMELIMIT:
    if (client.joinprogress) {
      client.joinprogress(kJoinETIMELIMIT, 0.0);
    }

    break;

  case EBANNEDPLAYER:
    if (client.joinprogress) {
      client.joinprogress(kJoinEBANNEDPLAYER, 0.0);
    }

    break;

  case ESERVERERROR:
    if (client.joinprogress) {
      client.joinprogress(kJoinESERVERERROR, 0.0);
    }

    break;

  case ECONNRESET:
    if (client.joinprogress) {
      client.joinprogress(kJoinECONNRESET, 0.0);
    }

    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }

  /* close thread pipe */
  closesock(client.mainpipe);
  closesock(client.threadpipe + 1);
  CLEARERRLOG
  pthread_exit(NULL);
END
}

int printmessage(int type, const char *body) {
  char *text = NULL;

TRY
  if (client.printmessage) {
    if (asprintf(&text, body) == -1) LOGFAIL(errno)
    client.printmessage(type, text);
    free(text);
    text = NULL;
  }

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int dgramclient() {
  struct CLUpdate clupdate;
  struct CLUpdateShell *clupdateshells;
  struct CLUpdateExplosion *clupdateexplosions;
  ssize_t r;
  int i;
  int oldseq;
  Pointi oldsqr, newsqr;

TRY
  for (;;) {
    if ((r = recv(client.dgramsock, &clupdate, sizeof(clupdate), O_NONBLOCK)) == -1) {
      if (errno == EAGAIN) {
        break;
      }
      else if (errno == EINTR) {
        continue;
      }
      else {
        LOGFAIL(errno)
      }
    }

    if (  /* sanity check psize */
      r < sizeof(clupdate.hdr) ||
      r != sizeof(clupdate.hdr) + clupdate.hdr.nshells*sizeof(struct CLUpdateShell) + clupdate.hdr.nexplosions*sizeof(struct CLUpdateExplosion) ||
      clupdate.hdr.player >= MAXPLAYERS ||
      clupdate.hdr.player == client.player
    ) {
      continue;
    }

    /* network to host byte order */
    for (i = 0; i < MAXPLAYERS; i++) {
      clupdate.hdr.seq[i] = ntohl(clupdate.hdr.seq[i]);
    }

    clupdate.hdr.tankx = ntohl(clupdate.hdr.tankx);
    clupdate.hdr.tanky = ntohl(clupdate.hdr.tanky);
    clupdate.hdr.tankspeed = ntohl(clupdate.hdr.tankspeed);
    clupdate.hdr.tankturnspeed = ntohl(clupdate.hdr.tankturnspeed);
    clupdate.hdr.tankkickdir = ntohl(clupdate.hdr.tankkickdir);
    clupdate.hdr.tankkickspeed = ntohl(clupdate.hdr.tankkickspeed);
    clupdate.hdr.builderx = ntohl(clupdate.hdr.builderx);
    clupdate.hdr.buildery = ntohl(clupdate.hdr.buildery);
    clupdate.hdr.inputflags = ntohl(clupdate.hdr.inputflags);

    clupdateshells = (void *)clupdate.buf;
    clupdateexplosions = (void *)(clupdateshells + clupdate.hdr.nshells);

    /* verify this is a valid player */
    if (client.players[clupdate.hdr.player].connected) {
      /* make sure this is not an old update */
      if ((clupdate.hdr.seq[clupdate.hdr.player] - client.players[clupdate.hdr.player].seq) > 0) {
        struct ListNode *node;

//        if (client.srvaddr.sin_addr.s_addr != addr.sin_port) {
//          client.srvaddr.sin_port = addr.sin_port;
//        }

        if (client.setplayerstatus && client.players[client.player].seq - client.players[clupdate.hdr.player].lastupdate >= TICKSPERSEC) {
          client.setplayerstatus(clupdate.hdr.player);
        }

        oldseq = client.players[clupdate.hdr.player].seq;
        oldsqr = makepoint(client.players[clupdate.hdr.player].tank.x, client.players[clupdate.hdr.player].tank.y);

        client.players[clupdate.hdr.player].lastupdate = client.players[client.player].seq;
        client.players[clupdate.hdr.player].seq = clupdate.hdr.seq[clupdate.hdr.player];
        client.players[clupdate.hdr.player].dead = clupdate.hdr.tankstatus == kTankDead;
        client.players[clupdate.hdr.player].boat = clupdate.hdr.tankstatus == kTankOnBoat;
        client.players[clupdate.hdr.player].dir = clupdate.hdr.tankdir*((k2Pif)/FWIDTH);
        client.players[clupdate.hdr.player].tank.x = *((float *)&clupdate.hdr.tankx);
        client.players[clupdate.hdr.player].tank.y = *((float *)&clupdate.hdr.tanky);
        client.players[clupdate.hdr.player].speed = *((float *)&clupdate.hdr.tankspeed);
        client.players[clupdate.hdr.player].turnspeed = *((float *)&clupdate.hdr.tankturnspeed);
        client.players[clupdate.hdr.player].kickdir = *((float *)&clupdate.hdr.tankkickdir);
        client.players[clupdate.hdr.player].kickspeed = *((float *)&clupdate.hdr.tankkickspeed);
        client.players[clupdate.hdr.player].builderstatus = clupdate.hdr.builderstatus;
        client.players[clupdate.hdr.player].builder.x = *((float *)&clupdate.hdr.builderx);
        client.players[clupdate.hdr.player].builder.y = *((float *)&clupdate.hdr.buildery);
        client.players[clupdate.hdr.player].buildertarget.x = clupdate.hdr.buildertargetx;
        client.players[clupdate.hdr.player].buildertarget.y = clupdate.hdr.buildertargety;
        client.players[clupdate.hdr.player].builderwait = clupdate.hdr.builderwait;
        client.players[clupdate.hdr.player].inputflags = clupdate.hdr.inputflags;

        /* sounds that have played since last update */
        if (client.playsound) {
          if (clupdate.hdr.tankshotsound) {
            if (client.fog[(int)client.players[clupdate.hdr.player].tank.y][(int)client.players[clupdate.hdr.player].tank.x] > 0) {
              client.playsound(kTankShotSound);
            }
            else {
              client.playsound(kFarShotSound);
            }
          }

          if (clupdate.hdr.pillshotsound) {
            client.playsound(kFarShotSound);
          }

          if (clupdate.hdr.sinksound) {
            if (client.fog[(int)client.players[clupdate.hdr.player].tank.y][(int)client.players[clupdate.hdr.player].tank.x] > 0) {
              client.playsound(kSinkSound);
            }
            else {
              client.playsound(kFarSinkSound);
            }
          }

          if (clupdate.hdr.builderdeathsound) {
            if (client.fog[(int)client.players[clupdate.hdr.player].builder.y][(int)client.players[clupdate.hdr.player].builder.x] > 0) {
              client.playsound(kBuilderDeathSound);
            }
            else {
              client.playsound(kFarBuilderDeathSound);
            }
          }
        }
        if (client.printmessage && clupdate.hdr.builderdeathsound) {
            char *text;
            if (asprintf(&text, "%s just lost his builder", client.players[clupdate.hdr.player].name) == -1) LOGFAIL(errno)
                client.printmessage(MSGGAME, text);
            free(text);
        }


        /* build new shell list */
        clearlist(&client.players[clupdate.hdr.player].shells, free);
        node = &client.players[clupdate.hdr.player].shells;

        for (i = 0; i < clupdate.hdr.nshells; i++) {
          struct Shell *shell;

          if ((shell = (struct Shell *)malloc(sizeof(struct Shell))) == NULL) LOGFAIL(errno)
          shell->owner = clupdateshells[i].owner;
          shell->point.x = ntohs(clupdateshells[i].shellx)/FWIDTH;
          shell->point.y = ntohs(clupdateshells[i].shelly)/FWIDTH;
          shell->boat = clupdateshells[i].boat;
          shell->pill = clupdateshells[i].pill;
          shell->dir = clupdateshells[i].shelldir*((k2Pif)/FWIDTH);
          shell->range = ntohs(clupdateshells[i].range)/FWIDTH;
          if (addlist(node, shell)) LOGFAIL(errno)
          node = nextlist(node);
        }

        /* build new explosion list */
        clearlist(&client.players[clupdate.hdr.player].explosions, free);
        node = &client.players[clupdate.hdr.player].explosions;

        for (i = 0; i < clupdate.hdr.nexplosions; i++) {
          struct Explosion *explosion;

          if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
          explosion->point.x = ntohs(clupdateexplosions[i].explosionx)/FWIDTH;
          explosion->point.y = ntohs(clupdateexplosions[i].explosiony)/FWIDTH;
          explosion->counter = clupdateexplosions[i].counter;
          if (addlist(node, explosion)) LOGFAIL(errno)

          if (explosion->counter < 5) {
            killpointbuilder(explosion->point);
          }

          node = nextlist(node);
        }

        /* extrapolate the future position of the tank and builder to compensate for latency */
        if (clupdate.hdr.seq[client.player] != 0) {  /* make sure they have one update from us */
          for (i = 0; i < ((client.players[client.player].seq - clupdate.hdr.seq[client.player])/2); i++) {
            if (tankmovelogic(clupdate.hdr.player)) LOGFAIL(errno)
            if (builderlogic(clupdate.hdr.player)) LOGFAIL(errno)
            if (shelllogic(clupdate.hdr.player)) LOGFAIL(errno)
            if (explosionlogic(clupdate.hdr.player)) LOGFAIL(errno)
          }
        }

        newsqr = makepoint(client.players[clupdate.hdr.player].tank.x, client.players[clupdate.hdr.player].tank.y);

        if (!isequalpoint(newsqr, oldsqr) && testalliance(client.player, clupdate.hdr.player)) {
          if (increasevis(makerect(newsqr.x - 14, newsqr.y - 14, 29, 29))) LOGFAIL(errno)

          if (oldseq != 0) {
            if (decreasevis(makerect(oldsqr.x - 14, oldsqr.y - 14, 29, 29))) LOGFAIL(errno)
          }
        }
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

static int recvsrpause() {
  struct SRPause *srpause;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPause)) FAIL(EAGAIN)
  srpause = (struct SRPause *)client.recvbuf.ptr;

  if (srpause->pause == 255) {
    client.pause = -1;
  }
  else {
    client.pause = srpause->pause;
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPause)) == -1) FAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrsendmesg() {
  struct SRSendMesg *srsendmesg;
  char *messagetext = NULL;
  int i;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRSendMesg)) FAIL(EAGAIN)

  /* check for complete string following struct */
  for (i = sizeof(struct SRSendMesg) ; i < client.recvbuf.nbytes; i++) {
    if (((char *)client.recvbuf.ptr)[i] == '\0') {
      break;
    }
  }

  /* buf does not have the whole string */
  if (!(i < client.recvbuf.nbytes)) FAIL(EAGAIN)

  srsendmesg = (struct SRSendMesg *)client.recvbuf.ptr;

  /* print the message on the client side */
  if (client.printmessage) {
    if (asprintf(&messagetext, "%s: %s", client.players[srsendmesg->player].name, (char *)(srsendmesg + 1)) == -1) LOGFAIL(errno)
    client.printmessage(srsendmesg->to, messagetext);
    free(messagetext);
    messagetext = NULL;
  }

  /* clear buffer of read data */
  if (readbuf(&client.recvbuf, NULL, i + 1) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

static int recvsrdamage() {
  struct SRDamage *srdamage;
  int pill, base;
  struct Explosion *explosion;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRDamage)) FAIL(EAGAIN)
  srdamage = (struct SRDamage *)client.recvbuf.ptr;

  if ((pill = findpill(srdamage->x, srdamage->y)) != -1) {
    /* damage pill */
    if (client.pills[pill].armour > 0) {
      client.pills[pill].armour--;

      /* fog of war */
      if (client.pills[pill].armour == 0) {
        if (testalliance(client.pills[pill].owner, client.player)) {
          if (refresh(srdamage->x, srdamage->y)) LOGFAIL(errno)
          if (decreasevis(makerect(client.pills[pill].x - 7, client.pills[pill].y - 7, 15, 15))) LOGFAIL(errno)
        }

        if (client.setpillstatus) {
          client.setpillstatus(pill);
        }
      }

      /* heat pill */
      client.pills[pill].speed /= 2;
      client.pills[pill].speed = MAX(client.pills[pill].speed, MINTICKSPERSHOT);
    }
  }
  else if ((base = findbase(srdamage->x, srdamage->y)) != -1) {
    /* damage base */
    if (client.bases[base].armour >= MINBASEARMOUR) {
      client.bases[base].armour -= MINBASEARMOUR;

      /* heat pills */
      for (pill = 0; pill < client.npills; pill++) {
        if (
          mag2f(sub2f(make2f(client.pills[pill].x + 0.5, client.pills[pill].y + 0.5), make2f(client.bases[base].x + 0.5, client.bases[base].y + 0.5))) <= 8.0 &&
          client.pills[pill].owner != NEUTRAL && client.bases[base].owner != NEUTRAL &&
          testalliance(client.pills[pill].owner, client.bases[base].owner)
        ) {
          client.pills[pill].speed /= 2;
          client.pills[pill].speed = MAX(client.pills[pill].speed, MINTICKSPERSHOT);
        }
      }

      if (client.setbasestatus) {
        client.setbasestatus(base);
      }
    }
  }

  /* play sound */
  if (client.playsound) {
    if (client.terrain[srdamage->y][srdamage->x] == kForestTerrain) {
      if (client.fog[srdamage->y][srdamage->x] > 0) {
        client.playsound(kHitTreeSound);
      }
      else {
        client.playsound(kFarHitTreeSound);
      }
    }
    else {
      if (client.fog[srdamage->y][srdamage->x] > 0) {
        client.playsound(kHitTerrainSound);
      }
      else {
        client.playsound(kFarHitTerrainSound);
      }
    }
  }

  client.terrain[srdamage->y][srdamage->x] = srdamage->terrain;

  /* create explosion */
  if (srdamage->player != client.player) {
    explosion = (void *)malloc(sizeof(struct Explosion));
    if (explosion == NULL) LOGFAIL(errno)
    explosion->point.x = srdamage->x + 0.5;
    explosion->point.y = srdamage->y + 0.5;
    explosion->counter = 0;
    addlist(&client.explosions, explosion);
    if (killsquarebuilder(makepoint(srdamage->x, srdamage->y))) LOGFAIL(errno)
  }

  /* refresh seen tiles */
  if (refresh(srdamage->x, srdamage->y)) LOGFAIL(errno)

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRDamage)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrgrabtrees() {
  struct SRGrabTrees *srgrabtrees;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRGrabTrees)) FAIL(EAGAIN)
  srgrabtrees = (struct SRGrabTrees *)client.recvbuf.ptr;

  if (client.terrain[srgrabtrees->y][srgrabtrees->x] == kMinedForestTerrain) {
    client.terrain[srgrabtrees->y][srgrabtrees->x] = kMinedGrassTerrain;
  }
  else {
    client.terrain[srgrabtrees->y][srgrabtrees->x] = kGrassTerrain0;
  }

  if (refresh(srgrabtrees->x, srgrabtrees->y) == -1) LOGFAIL(errno)

  if (client.playsound) {
    if (client.fog[srgrabtrees->y][srgrabtrees->x] > 0) {
      client.playsound(kTreeSound);
    }
    else {
      client.playsound(kFarTreeSound);
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRGrabTrees)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrbuild() {
  struct SRBuild *srbuild;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRBuild)) FAIL(EAGAIN)
  srbuild = (struct SRBuild *)client.recvbuf.ptr;

  client.terrain[srbuild->y][srbuild->x] = srbuild->terrain;
  if (refresh(srbuild->x, srbuild->y) == -1) LOGFAIL(errno)

  if (client.playsound) {
    if (client.fog[srbuild->y][srbuild->x] > 0) {
      client.playsound(kBuildSound);
    }
    else {
      client.playsound(kFarBuildSound);
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRBuild)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrgrow() {
  struct SRGrow *srgrow;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRGrow)) FAIL(EAGAIN)
  srgrow = (struct SRGrow *)client.recvbuf.ptr;

  switch (client.terrain[srgrow->y][srgrow->x]) {
    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
    case kCraterTerrain:
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kRoadTerrain:
      client.terrain[srgrow->y][srgrow->x] = kForestTerrain;
      if (refresh(srgrow->x, srgrow->y)) LOGFAIL(errno)
      break;

    case kMinedGrassTerrain:
    case kMinedRubbleTerrain:
    case kMinedCraterTerrain:
    case kMinedSwampTerrain:
    case kMinedRoadTerrain:
      client.terrain[srgrow->y][srgrow->x] = kMinedForestTerrain;
      if (refresh(srgrow->x, srgrow->y)) LOGFAIL(errno)
      break;

    default:
      break;
    }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRGrow)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrflood() {
  struct SRFlood *srflood;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRFlood)) FAIL(EAGAIN)
  srflood = (struct SRFlood *)client.recvbuf.ptr;

  client.terrain[srflood->y][srflood->x] = kRiverTerrain;
  if (refresh(srflood->x, srflood->y) == -1) LOGFAIL(errno)

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRFlood)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrplacemine() {
  struct SRPlaceMine *srplacemine;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlaceMine)) FAIL(EAGAIN)
  srplacemine = (struct SRPlaceMine *)client.recvbuf.ptr;

  switch (client.terrain[srplacemine->y][srplacemine->x]) {
	case kSwampTerrain0:
	case kSwampTerrain1:
	case kSwampTerrain2:
	case kSwampTerrain3:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedSwampTerrain;
    break;

	case kCraterTerrain:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedCraterTerrain;
    break;

	case kRoadTerrain:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedRoadTerrain;
    break;

	case kForestTerrain:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedForestTerrain;
    break;

	case kRubbleTerrain0:
	case kRubbleTerrain1:
	case kRubbleTerrain2:
	case kRubbleTerrain3:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedRubbleTerrain;
    break;

	case kGrassTerrain0:
	case kGrassTerrain1:
	case kGrassTerrain2:
	case kGrassTerrain3:
    client.terrain[srplacemine->y][srplacemine->x] = kMinedGrassTerrain;
    break;

	case kSeaTerrain:
	case kBoatTerrain:
	case kWallTerrain:
	case kRiverTerrain:
	case kDamagedWallTerrain0:
	case kDamagedWallTerrain1:
	case kDamagedWallTerrain2:
	case kDamagedWallTerrain3:
	case kMinedSeaTerrain:
	case kMinedSwampTerrain:
	case kMinedCraterTerrain:
	case kMinedRoadTerrain:
	case kMinedForestTerrain:
	case kMinedRubbleTerrain:
	case kMinedGrassTerrain:
    break;

  default:
    break;
  }

  if (client.playsound && client.fog[srplacemine->y][srplacemine->x] > 0) {
    client.playsound(kMineSound);
  }

  if (!client.hiddenmines || testalliance(client.player, srplacemine->player)) {
    if (refresh(srplacemine->x, srplacemine->y) == -1) LOGFAIL(errno)
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlaceMine)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrdropmine() {
  struct SRDropMine *srdropmine;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRDropMine)) FAIL(EAGAIN)
  srdropmine = (struct SRDropMine *)client.recvbuf.ptr;

  switch (client.terrain[srdropmine->y][srdropmine->x]) {
	case kSwampTerrain0:
	case kSwampTerrain1:
	case kSwampTerrain2:
	case kSwampTerrain3:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedSwampTerrain;
    break;

	case kCraterTerrain:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedCraterTerrain;
    break;

	case kRoadTerrain:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedRoadTerrain;
    break;

	case kForestTerrain:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedForestTerrain;
    break;

	case kRubbleTerrain0:
	case kRubbleTerrain1:
	case kRubbleTerrain2:
	case kRubbleTerrain3:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedRubbleTerrain;
    break;

	case kGrassTerrain0:
	case kGrassTerrain1:
	case kGrassTerrain2:
	case kGrassTerrain3:
    client.terrain[srdropmine->y][srdropmine->x] = kMinedGrassTerrain;
    break;

	case kSeaTerrain:
	case kBoatTerrain:
	case kWallTerrain:
	case kRiverTerrain:
	case kDamagedWallTerrain0:
	case kDamagedWallTerrain1:
	case kDamagedWallTerrain2:
	case kDamagedWallTerrain3:
	case kMinedSeaTerrain:
	case kMinedSwampTerrain:
	case kMinedCraterTerrain:
	case kMinedRoadTerrain:
	case kMinedForestTerrain:
	case kMinedRubbleTerrain:
	case kMinedGrassTerrain:
    break;

  default:
    break;
  }

  if (client.playsound && client.fog[srdropmine->y][srdropmine->x] > 0) {
    client.playsound(kMineSound);
  }

  if (refresh(srdropmine->x, srdropmine->y) == -1) LOGFAIL(errno)

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRDropMine)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrdropboat() {
  struct SRDropBoat *srdropboat;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRDropBoat)) FAIL(EAGAIN)
  srdropboat = (struct SRDropBoat *)client.recvbuf.ptr;

  switch (client.terrain[srdropboat->y][srdropboat->x]) {
	case kRiverTerrain:
    client.terrain[srdropboat->y][srdropboat->x] = kBoatTerrain;
    break;

	case kSeaTerrain:
	case kSwampTerrain0:
	case kSwampTerrain1:
	case kSwampTerrain2:
	case kSwampTerrain3:
	case kCraterTerrain:
	case kRoadTerrain:
	case kForestTerrain:
	case kRubbleTerrain0:
	case kRubbleTerrain1:
	case kRubbleTerrain2:
	case kRubbleTerrain3:
	case kGrassTerrain0:
	case kGrassTerrain1:
	case kGrassTerrain2:
	case kGrassTerrain3:
	case kBoatTerrain:
	case kWallTerrain:
	case kDamagedWallTerrain0:
	case kDamagedWallTerrain1:
	case kDamagedWallTerrain2:
	case kDamagedWallTerrain3:
	case kMinedSeaTerrain:
	case kMinedSwampTerrain:
	case kMinedCraterTerrain:
	case kMinedRoadTerrain:
	case kMinedForestTerrain:
	case kMinedRubbleTerrain:
	case kMinedGrassTerrain:
    break;

  default:
    break;
  }

  if (refresh(srdropboat->x, srdropboat->y) == -1) LOGFAIL(errno)

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRDropBoat)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrplayerjoin() {
  struct SRPlayerJoin *srplayerjoin;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerJoin)) FAIL(EAGAIN)
  srplayerjoin = (struct SRPlayerJoin *)client.recvbuf.ptr;

  client.players[srplayerjoin->player].used = 1;
  client.players[srplayerjoin->player].connected = 1;
  client.players[srplayerjoin->player].seq = 0;
  client.players[srplayerjoin->player].lastupdate = client.players[client.player].seq;
  strncpy(client.players[srplayerjoin->player].name, srplayerjoin->name, sizeof(client.players[srplayerjoin->player].name) - 1);
  strncpy(client.players[srplayerjoin->player].host, srplayerjoin->host, sizeof(client.players[srplayerjoin->player].host) - 1);

  client.players[srplayerjoin->player].alliance = 1 << srplayerjoin->player;

  if (client.printmessage) {
    if (asprintf(&text, "%s joined", srplayerjoin->name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerjoin->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerJoin)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrplayerrejoin() {
  struct SRPlayerRejoin *srplayerrejoin;
  int i;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerRejoin)) FAIL(EAGAIN)
  srplayerrejoin = (struct SRPlayerRejoin *)client.recvbuf.ptr;

  client.players[srplayerrejoin->player].connected = 1;
  client.players[srplayerrejoin->player].seq = 0;
  client.players[srplayerrejoin->player].lastupdate = client.players[client.player].seq;

  if (client.player == srplayerrejoin->player) {
    for (i = 0; i < client.npills; i++) {
      if (testalliance(client.player, client.pills[i].owner)) {
        if (client.pills[i].armour != ONBOARD && client.pills[i].armour > 0) {
          if (increasevis(makerect(client.pills[i].x - 7, client.pills[i].y - 7, 15, 15))) LOGFAIL(errno)
        }

        if (client.setpillstatus) {
          client.setpillstatus(i);
        }
      }
    }
  }

  if (client.printmessage) {
    if (asprintf(&text, "%s rejoined", client.players[srplayerrejoin->player].name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerrejoin->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerRejoin)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrplayerexit() {
  struct SRPlayerExit *srplayerexit;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerExit)) FAIL(EAGAIN)
  srplayerexit = (struct SRPlayerExit *)client.recvbuf.ptr;

  if (client.printmessage) {
    if (asprintf(&text, "%s left", client.players[srplayerexit->player].name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (client.players[srplayerexit->player].seq != 0 && testalliance(client.player, srplayerexit->player)) {
    if (decreasevis(makerect(((int)client.players[srplayerexit->player].tank.x) - 14, ((int)client.players[srplayerexit->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
  }

  client.players[srplayerexit->player].connected = 0;
  client.players[srplayerexit->player].seq = 0;

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerexit->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerExit)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

static int recvsrplayerdisc() {
  struct SRPlayerDisc *srplayerdisc;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerDisc)) FAIL(EAGAIN)
  srplayerdisc = (struct SRPlayerDisc *)client.recvbuf.ptr;

  if (client.printmessage) {
    if (asprintf(&text, "%s disconnected", client.players[srplayerdisc->player].name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (srplayerdisc->player != client.player && client.players[srplayerdisc->player].seq != 0 && testalliance(client.player, srplayerdisc->player)) {
    if (decreasevis(makerect(((int)client.players[srplayerdisc->player].tank.x) - 14, ((int)client.players[srplayerdisc->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
  }

  client.players[srplayerdisc->player].connected = 0;
  client.players[srplayerdisc->player].seq = 0;

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerdisc->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerDisc)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

static int recvsrplayerkick() {
  struct SRPlayerKick *srplayerkick;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerKick)) FAIL(EAGAIN)
  srplayerkick = (struct SRPlayerKick *)client.recvbuf.ptr;

  if (client.printmessage) {
    if (asprintf(&text, "%s kicked", client.players[srplayerkick->player].name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (srplayerkick->player != client.player && client.players[srplayerkick->player].seq != 0 && testalliance(client.player, srplayerkick->player)) {
    if (decreasevis(makerect(((int)client.players[srplayerkick->player].tank.x) - 14, ((int)client.players[srplayerkick->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
  }

  client.players[srplayerkick->player].connected = 0;
  client.players[srplayerkick->player].seq = 0;

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerkick->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerKick)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

static int recvsrplayerban() {
  struct SRPlayerBan *srplayerban;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRPlayerBan)) FAIL(EAGAIN)
  srplayerban = (struct SRPlayerBan *)client.recvbuf.ptr;

  if (client.printmessage) {
    if (asprintf(&text, "%s banned", client.players[srplayerban->player].name) == -1) LOGFAIL(errno)
    client.printmessage(MSGGAME, text);
    free(text);
    text = NULL;
  }

  if (srplayerban->player != client.player && client.players[srplayerban->player].seq != 0 && testalliance(client.player, srplayerban->player)) {
    if (decreasevis(makerect(((int)client.players[srplayerban->player].tank.x) - 14, ((int)client.players[srplayerban->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
  }

  client.players[srplayerban->player].connected = 0;
  client.players[srplayerban->player].seq = 0;

  if (client.setplayerstatus) {
    client.setplayerstatus(srplayerban->player);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRPlayerBan)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrrepairpill() {
  struct SRRepairPill *srrepairpill;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRRepairPill)) FAIL(EAGAIN)
  srrepairpill = (struct SRRepairPill *)client.recvbuf.ptr;

  /* update the fog of war and status */
  if (
    testalliance(client.pills[srrepairpill->pill].owner, client.player) &&
    client.pills[srrepairpill->pill].armour == 0 && srrepairpill->armour != 0 && srrepairpill->armour != ONBOARD
  ) {
    if (increasevis(makerect(client.pills[srrepairpill->pill].x - 7, client.pills[srrepairpill->pill].y - 7, 15, 15))) LOGFAIL(errno)
  }

  client.pills[srrepairpill->pill].armour = srrepairpill->armour;

  /* refresh seen tiles */
  if (refresh(client.pills[srrepairpill->pill].x, client.pills[srrepairpill->pill].y) == -1) LOGFAIL(errno)

  if (client.playsound) {
    if (client.fog[client.pills[srrepairpill->pill].y][client.pills[srrepairpill->pill].x] > 0) {
      client.playsound(kBuildSound);
    }
    else {
      client.playsound(kFarBuildSound);
    }
  }

  /* update pill status */
  if (client.setpillstatus) {
    client.setpillstatus(srrepairpill->pill);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRRepairPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrcoolpill() {
  struct SRCoolPill *srcoolpill;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRCoolPill)) FAIL(EAGAIN)
  srcoolpill = (struct SRCoolPill *)client.recvbuf.ptr;

  /* cool pill */
  client.pills[srcoolpill->pill].speed++;

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRCoolPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrcapturepill() {
  struct SRCapturePill *srcapturepill;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRCapturePill)) FAIL(EAGAIN)
  srcapturepill = (struct SRCapturePill *)client.recvbuf.ptr;

  if (client.printmessage && client.pills[srcapturepill->pill].owner != srcapturepill->owner) {
    if (client.pills[srcapturepill->pill].owner == NEUTRAL) {
      if (asprintf(&text, "%s captured neutral pill %d", client.players[srcapturepill->owner].name, srcapturepill->pill) == -1) LOGFAIL(errno)
      client.printmessage(MSGGAME, text);
      free(text);
      text = NULL;
    }
    else {
      if (asprintf(&text, "%s captured pill %d from %s", client.players[srcapturepill->owner].name, srcapturepill->pill, client.players[client.pills[srcapturepill->pill].owner].name) == -1) LOGFAIL(errno)
      client.printmessage(MSGGAME, text);
      free(text);
      text = NULL;
    }
  }

  client.pills[srcapturepill->pill].owner = srcapturepill->owner;
  client.pills[srcapturepill->pill].armour = ONBOARD;
  client.pills[srcapturepill->pill].speed = MAXTICKSPERSHOT;

  if (client.pills[srcapturepill->pill].x == (int)client.players[client.player].tank.x && client.pills[srcapturepill->pill].y == (int)client.players[client.player].tank.y) {
    switch (client.terrain[(int)client.players[client.player].tank.y][(int)client.players[client.player].tank.x]) {
    case kSeaTerrain:  /* drown */
      if (!client.players[client.player].boat) {
        if (drown()) LOGFAIL(errno)
      }

      break;

    case kBoatTerrain:  /* grab boat */
      if (sendclgrabtile(client.pills[srcapturepill->pill].x, client.pills[srcapturepill->pill].y)) LOGFAIL(errno)
      break;

    case kWallTerrain:
    case kDamagedWallTerrain0:
    case kDamagedWallTerrain1:
    case kDamagedWallTerrain2:
    case kDamagedWallTerrain3:
      if (superboom()) LOGFAIL(errno)
      break;

    case kRiverTerrain:
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kCraterTerrain:
    case kRoadTerrain:
    case kForestTerrain:
    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
      /* do nothing */
      break;

    case kMinedSeaTerrain:
      if (drown()) LOGFAIL(errno)
      if (sendclgrabtile(client.pills[srcapturepill->pill].x, client.pills[srcapturepill->pill].y)) LOGFAIL(errno)
      break;

    case kMinedSwampTerrain:
    case kMinedCraterTerrain:
    case kMinedRoadTerrain:
    case kMinedForestTerrain:
    case kMinedRubbleTerrain:
    case kMinedGrassTerrain:
      /* explode mine */
      if (sendclgrabtile(client.pills[srcapturepill->pill].x, client.pills[srcapturepill->pill].y)) LOGFAIL(errno)
      break;

    default:
      assert(0);
    }
  }

  /* refresh tiles */
  if (refresh(client.pills[srcapturepill->pill].x, client.pills[srcapturepill->pill].y) == -1) LOGFAIL(errno)

  if (client.setpillstatus) {
    client.setpillstatus(srcapturepill->pill);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRCapturePill)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrbuildpill() {
  struct SRBuildPill *srbuildpill;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRBuildPill)) FAIL(EAGAIN)
  srbuildpill = (struct SRBuildPill *)client.recvbuf.ptr;

  client.pills[srbuildpill->pill].x = srbuildpill->x;
  client.pills[srbuildpill->pill].y = srbuildpill->y;
  client.pills[srbuildpill->pill].armour = srbuildpill->armour;
  client.pills[srbuildpill->pill].speed = MAXTICKSPERSHOT;

  /* refresh seen tiles */
  if (refresh(srbuildpill->x, srbuildpill->y) == -1) LOGFAIL(errno)

  /* play sound */
  if (client.playsound) {
    if (client.fog[srbuildpill->y][srbuildpill->x] > 0) {
      client.playsound(kBuildSound);
    }
    else {
      client.playsound(kFarBuildSound);
    }
  }

  /* fog of war */
  if (testalliance(client.pills[srbuildpill->pill].owner, client.player) && srbuildpill->armour != 0 && srbuildpill->armour != ONBOARD) {
    if (increasevis(makerect(client.pills[srbuildpill->pill].x - 7, client.pills[srbuildpill->pill].y - 7, 15, 15))) LOGFAIL(errno)
  }

  /* update and status */
  if (client.setpillstatus) {
    client.setpillstatus(srbuildpill->pill);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRBuildPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrdroppill() {
  struct SRDropPill *srdroppill;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRDropPill)) FAIL(EAGAIN)
  srdroppill = (struct SRDropPill *)client.recvbuf.ptr;

  client.pills[srdroppill->pill].x = srdroppill->x;
  client.pills[srdroppill->pill].y = srdroppill->y;
  client.pills[srdroppill->pill].armour = 0;
  client.pills[srdroppill->pill].speed = MAXTICKSPERSHOT;

  /* refresh seen tiles */
  if (refresh(srdroppill->x, srdroppill->y) == -1) LOGFAIL(errno)

  /* update pill status */
  if (client.setpillstatus) {
    client.setpillstatus(srdroppill->pill);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRDropPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrreplenishbase() {
  struct SRReplenishBase *srreplenishbase;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRReplenishBase)) FAIL(EAGAIN)
  srreplenishbase = (struct SRReplenishBase *)client.recvbuf.ptr;

  if (++client.bases[srreplenishbase->base].armour > MAXBASEARMOUR) {
    client.bases[srreplenishbase->base].armour = MAXBASEARMOUR;
  }

  if (++client.bases[srreplenishbase->base].shells > MAXBASESHELLS) {
    client.bases[srreplenishbase->base].shells = MAXBASESHELLS;
  }

  if (++client.bases[srreplenishbase->base].mines > MAXBASEMINES) {
    client.bases[srreplenishbase->base].mines = MAXBASEMINES;
  }

  if (client.setbasestatus) {
    client.setbasestatus(srreplenishbase->base);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRReplenishBase)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrcapturebase() {
  struct SRCaptureBase *srcapturebase;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRCaptureBase)) FAIL(EAGAIN)
  srcapturebase = (struct SRCaptureBase *)client.recvbuf.ptr;

  if (client.bases[srcapturebase->base].owner == NEUTRAL) {
    if (client.printmessage) {
      if (asprintf(&text, "%s captured neutral base %d", client.players[srcapturebase->owner].name, srcapturebase->base) == -1) LOGFAIL(errno)
      client.printmessage(MSGGAME, text);
      free(text);
      text = NULL;
    }

    client.bases[srcapturebase->base].shells = MAXBASESHELLS;
    client.bases[srcapturebase->base].armour = MAXBASEARMOUR;
    client.bases[srcapturebase->base].mines = MAXBASEMINES;
  }
  else {
    if (client.printmessage) {
      if (asprintf(&text, "%s captured base %d from %s", client.players[srcapturebase->owner].name, srcapturebase->base, client.players[client.bases[srcapturebase->base].owner].name) == -1) LOGFAIL(errno)
      client.printmessage(MSGGAME, text);
      free(text);
      text = NULL;
    }

    client.bases[srcapturebase->base].shells = 0;
    client.bases[srcapturebase->base].armour = 0;
    client.bases[srcapturebase->base].mines = 0;
  }

  client.bases[srcapturebase->base].owner = srcapturebase->owner;

  if (refresh(client.bases[srcapturebase->base].x, client.bases[srcapturebase->base].y) == -1) LOGFAIL(errno)

  if (client.setbasestatus) {
    client.setbasestatus(srcapturebase->base);
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRCaptureBase)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

static int recvsrrefuel() {
  struct SRRefuel *srrefuel;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRRefuel)) FAIL(EAGAIN)
  srrefuel = (struct SRRefuel *)client.recvbuf.ptr;

  assert(srrefuel->base < client.nbases);

  client.bases[srrefuel->base].armour -= srrefuel->armour;
  client.bases[srrefuel->base].shells -= srrefuel->shells;
  client.bases[srrefuel->base].mines -= srrefuel->mines;

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRRefuel)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrgrabboat() {
  struct SRGrabBoat *srgrabboat;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRGrabBoat)) FAIL(EAGAIN)
  srgrabboat = (struct SRGrabBoat *)client.recvbuf.ptr;

  if (srgrabboat->player == client.player) {
    client.players[client.player].boat = 1;
  }

  if (client.terrain[srgrabboat->y][srgrabboat->x] == kBoatTerrain) {
    client.terrain[srgrabboat->y][srgrabboat->x] = kRiverTerrain;
    if (refresh(srgrabboat->x, srgrabboat->y)) LOGFAIL(errno)
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRGrabBoat)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrmineack() {
  struct SRMineAck *srmineack;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRMineAck)) FAIL(EAGAIN)
  srmineack = (struct SRMineAck *)client.recvbuf.ptr;

  if (!srmineack->success) {
    client.mines++;
  }

  if (client.settankstatus) {
    client.settankstatus();
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRMineAck)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrbuilderack() {
  struct SRBuilderAck *srbuilderack;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRBuilderAck)) FAIL(EAGAIN)
  srbuilderack = (struct SRBuilderAck *)client.recvbuf.ptr;

  switch (client.players[client.player].builderstatus) {
  case kBuilderWork:
    switch (client.buildertask) {
    case kBuilderGetTree:
      client.buildertrees = srbuilderack->trees;
      client.players[client.player].builderstatus = kBuilderWait;
      client.players[client.player].builderwait = 0;
      break;

    case kBuilderBuildRoad:
    case kBuilderBuildWall:
    case kBuilderBuildBoat:
    case kBuilderRepairPill:
      client.buildertrees = srbuilderack->trees;
      client.players[client.player].builderstatus = kBuilderWait;
      client.players[client.player].builderwait = 0;
      break;
      
    case kBuilderBuildPill:
      client.buildertrees = srbuilderack->trees;
      client.builderpill = srbuilderack->pill;
      client.players[client.player].builderstatus = kBuilderWait;
      client.players[client.player].builderwait = 0;
      break;

    case kBuilderPlaceMine:
      client.buildermines = srbuilderack->mines;
      client.players[client.player].builderstatus = kBuilderWait;
      client.players[client.player].builderwait = 0;
      break;

    case kBuilderDoNothing:
    default:
      break;
    }

    break;

  case kBuilderReady:
  case kBuilderGoto:
  case kBuilderReturn:
  case kBuilderParachute:
  default:
    break;
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRBuilderAck)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrsmallboom() {
  struct SRSmallBoom *srsmallboom;
  struct Explosion *explosion = NULL;
  Vec2f point;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRSmallBoom)) FAIL(EAGAIN)
  srsmallboom = (struct SRSmallBoom *)client.recvbuf.ptr;

  /* turn terrain to crater */
  if (client.terrain[srsmallboom->y][srsmallboom->x] != kSeaTerrain && client.terrain[srsmallboom->y][srsmallboom->x] != kMinedSeaTerrain) {
    client.terrain[srsmallboom->y][srsmallboom->x] = kCraterTerrain;
    if (refresh(srsmallboom->x, srsmallboom->y)) LOGFAIL(errno)
  }

  point.x = srsmallboom->x + 0.5;
  point.y = srsmallboom->y + 0.5;

  /* create explosions */
  if (srsmallboom->player != client.player) {
    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(srsmallboom->x, srsmallboom->y))) LOGFAIL(errno)
  }

  /* check for damage to tank */
  if (!client.players[client.player].dead && mag2f(sub2f(client.players[client.player].tank, point)) <= 1.0) {
    client.armour -= 10;
    client.players[client.player].boat = 0;

    if (client.armour < 0) {
      client.armour = 0;
      if (client.mines > 32) {
        superboom();
      }
      else if (client.mines > 0 || client.shells > 0) {
        smallboom();
      }
      else {
        killtank();
      }
    }

    if (client.settankstatus) {
      client.settankstatus();
    }

    /* play sound */
    if (client.playsound) {
      client.playsound(kHitTankSound);
    }
  }

  /* play sound */
  if (client.playsound) {
    if (client.fog[srsmallboom->y][srsmallboom->x] > 0) {
      client.playsound(kExplosionSound);
    }
    else {
      client.playsound(kFarExplosionSound);
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRSmallBoom)) == -1) LOGFAIL(errno)

CLEANUP
  if (explosion) {
    free(explosion);
  }

ERRHANDLER(0, -1)
END
}

int recvsrsuperboom() {
  struct SRSuperBoom *srsuperboom;
  struct Explosion *explosion = NULL;
  Vec2f point;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRSuperBoom)) FAIL(EAGAIN)
  srsuperboom = (struct SRSuperBoom *)client.recvbuf.ptr;

  /* turn terrain to crater */
  if (client.terrain[srsuperboom->y][srsuperboom->x] != kSeaTerrain && client.terrain[srsuperboom->y][srsuperboom->x] != kMinedSeaTerrain) {
    client.terrain[srsuperboom->y][srsuperboom->x] = kCraterTerrain;
    if (refresh(srsuperboom->x, srsuperboom->y)) LOGFAIL(errno)
  }
  if (client.terrain[srsuperboom->y][srsuperboom->x + 1] != kSeaTerrain && client.terrain[srsuperboom->y][srsuperboom->x + 1] != kMinedSeaTerrain) {
    client.terrain[srsuperboom->y][srsuperboom->x + 1] = kCraterTerrain;
    if (refresh(srsuperboom->x + 1, srsuperboom->y)) LOGFAIL(errno)
  }
  if (client.terrain[srsuperboom->y + 1][srsuperboom->x] != kSeaTerrain && client.terrain[srsuperboom->y + 1][srsuperboom->x] != kMinedSeaTerrain) {
    client.terrain[srsuperboom->y + 1][srsuperboom->x] = kCraterTerrain;
    if (refresh(srsuperboom->x, srsuperboom->y + 1)) LOGFAIL(errno)
  }
  if (client.terrain[srsuperboom->y + 1][srsuperboom->x + 1] != kSeaTerrain && client.terrain[srsuperboom->y + 1][srsuperboom->x + 1] != kMinedSeaTerrain) {
    client.terrain[srsuperboom->y + 1][srsuperboom->x + 1] = kCraterTerrain;
    if (refresh(srsuperboom->x + 1, srsuperboom->y + 1)) LOGFAIL(errno)
  }

  /* create explosions */
  if (srsuperboom->player != client.player) {
    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = srsuperboom->x + 0.5;
    explosion->point.y = srsuperboom->y + 0.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(srsuperboom->x, srsuperboom->y))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = srsuperboom->x + 1.5;
    explosion->point.y = srsuperboom->y + 0.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(srsuperboom->x + 1, srsuperboom->y))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = srsuperboom->x + 0.5;
    explosion->point.y = srsuperboom->y + 1.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(srsuperboom->x, srsuperboom->y + 1))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = srsuperboom->x + 1.5;
    explosion->point.y = srsuperboom->y + 1.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(srsuperboom->x + 1, srsuperboom->y + 1))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = srsuperboom->x + 0.25;
    point.y = srsuperboom->y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = srsuperboom->x + 1.0;
    point.y = srsuperboom->y + 0.25;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = srsuperboom->x + 1.75;
    point.y = srsuperboom->y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = srsuperboom->x + 1.0;
    point.y = srsuperboom->y + 1.75;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = srsuperboom->x + 1.0;
    point.y = srsuperboom->y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    /* check for damage to tank */
    if (!client.players[client.player].dead && mag2f(sub2f(client.players[client.player].tank, make2f(srsuperboom->x + 1, srsuperboom->y + 1))) <= 1.5) {
      client.armour -= 20;
      client.players[client.player].boat = 0;

      if (client.armour < 0) {
        client.armour = 0;
        if (client.mines > 32) {
          superboom();
        }
        else if (client.mines > 0 || client.shells > 0) {
          smallboom();
        }
        else {
          killtank();
        }
      }

      if (client.settankstatus) {
        client.settankstatus();
      }

      if (client.playsound) {
        client.playsound(kHitTankSound);
      }
    }

    /* play sound */
    if (client.playsound) {
      if (client.fog[srsuperboom->y][srsuperboom->x] > 0) {
        client.playsound(kSuperBoomSound);
      }
      else {
        client.playsound(kFarSuperBoomSound);
      }
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRSuperBoom)) == -1) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (explosion) {
      free(explosion);
    }

    RETERR(-1)
  }
END
}

int recvsrhittank() {
  struct SRHitTank *srhittank;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRHitTank)) FAIL(EAGAIN)
  srhittank = (struct SRHitTank *)client.recvbuf.ptr;

  srhittank->dir = ntohl(srhittank->dir);

  client.players[client.player].boat = 0;
  client.players[client.player].kickdir = *((float *)&srhittank->dir);
  client.players[client.player].kickspeed = KICKFORCE;

  client.armour -= 5;

  if (client.armour < 0) {
    client.armour = 0;
    killtank();
  }

  if (client.playsound) {
    client.playsound(kHitTankSound);
  }

  if (client.settankstatus) {
    client.settankstatus();
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRHitTank)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvsrsetalliance() {
  struct SRSetAlliance *srsetalliance;
  int i;
  uint16_t xor;
  char *text = NULL;

TRY
  if (client.recvbuf.nbytes < sizeof(struct SRSetAlliance)) FAIL(EAGAIN)
  srsetalliance = (struct SRSetAlliance *)client.recvbuf.ptr;

  /* convert byte order */
  srsetalliance->alliance = ntohs(srsetalliance->alliance);

  xor = client.players[srsetalliance->player].alliance ^ srsetalliance->alliance;
  client.players[srsetalliance->player].alliance = srsetalliance->alliance;

  /* their alliance bit has changed */
  if (xor & (1 << client.player)) {
    /* my alliance bit is set */
    if (client.players[client.player].alliance & (1 << srsetalliance->player)) {
      /* their alliance bit is set */
      if (srsetalliance->alliance & (1 << client.player)) {
        if (client.printmessage) {
          if (asprintf(&text, "%s accepted the alliance", client.players[srsetalliance->player].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
          free(text);
          text = NULL;
        }

        if (client.setplayerstatus) {
          client.setplayerstatus(srsetalliance->player);
        }

        for (i = 0; i < client.nbases; i++) {
          if (client.bases[i].owner == srsetalliance->player) {
            refresh(client.bases[i].x, client.bases[i].y);

            if (client.setbasestatus) {
              client.setbasestatus(i);
            }
          }
        }

        for (i = 0; i < client.npills; i++) {
          if (client.pills[i].owner == srsetalliance->player) {
            refresh(client.pills[i].x, client.pills[i].y);

            /* fog of war */
            if (client.pills[i].armour != ONBOARD && client.pills[i].armour > 0) {
              if (increasevis(makerect(client.pills[i].x - 7, client.pills[i].y - 7, 15, 15))) LOGFAIL(errno)
            }

            if (client.setpillstatus) {
              client.setpillstatus(i);
            }
          }
        }

        if (increasevis(makerect(((int)client.players[srsetalliance->player].tank.x) - 14, ((int)client.players[srsetalliance->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
      }
      /* their alliance bit is unset */
      else {
        if (client.printmessage) {
          if (asprintf(&text, "%s left the alliance", client.players[srsetalliance->player].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
          free(text);
          text = NULL;
        }

        if (client.setplayerstatus) {
          client.setplayerstatus(srsetalliance->player);
        }

        for (i = 0; i < client.nbases; i++) {
          if (client.bases[i].owner == srsetalliance->player) {
            if (client.setbasestatus) {
              client.setbasestatus(i);
            }

            refresh(client.bases[i].x, client.bases[i].y);
          }
        }

        for (i = 0; i < client.npills; i++) {
          if (client.pills[i].owner == srsetalliance->player) {
            /* fog of war */
            if (client.pills[i].armour != ONBOARD && client.pills[i].armour > 0) {
              if (refresh(client.pills[i].x, client.pills[i].y)) LOGFAIL(errno)
              if (decreasevis(makerect(client.pills[i].x - 7, client.pills[i].y - 7, 15, 15))) LOGFAIL(errno)
            }

            if (client.setpillstatus) {
              client.setpillstatus(i);
            }
          }
        }

        if (decreasevis(makerect(((int)client.players[srsetalliance->player].tank.x) - 14, ((int)client.players[srsetalliance->player].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)

        if (leavealliance(1 << srsetalliance->player)) LOGFAIL(errno)
      }
    }
    /* my alliance bit is unset */
    else {
      /* their alliance bit is set */
      if (client.printmessage && srsetalliance->alliance & (1 << client.player)) {
        if (asprintf(&text, "%s requests an alliance", client.players[srsetalliance->player].name) == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRSetAlliance)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrtimelimit() {
  struct SRTimeLimit *srtimelimit;
  int minutes, seconds;
  char *text = NULL;
  
TRY
  if (client.recvbuf.nbytes < sizeof(struct SRTimeLimit)) FAIL(EAGAIN)
  srtimelimit = (struct SRTimeLimit *)client.recvbuf.ptr;

  /* convert byte order */
  srtimelimit->timeremaining = ntohs(srtimelimit->timeremaining);

  minutes = srtimelimit->timeremaining/60;
  seconds = srtimelimit->timeremaining%60;

  if (client.printmessage) {
    if (minutes != 0) {
      if (seconds != 0) {
        if (asprintf(&text, "%d Minute%s and %d Second%s Remaining!", minutes, minutes > 1 ? "s" : "", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
      else {
        if (asprintf(&text, "%d Minute%s Remaining!", minutes, minutes > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
    }
    else {
      if (seconds != 0) {
        if (asprintf(&text, "%d Second%s Remaining!", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
      else {
        if (asprintf(&text, "Time Limit Reached!", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
        client.timelimitreached = 1;
      }
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRTimeLimit)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

int recvsrbasecontrol() {
  struct SRBaseControl *srbasecontrol;
  int minutes, seconds;
  char *text = NULL;
  
TRY
  if (client.recvbuf.nbytes < sizeof(struct SRBaseControl)) FAIL(EAGAIN)
  srbasecontrol = (struct SRBaseControl *)client.recvbuf.ptr;

  /* convert byte order */
  srbasecontrol->timeleft = ntohs(srbasecontrol->timeleft);

  minutes = srbasecontrol->timeleft/60;
  seconds = srbasecontrol->timeleft%60;

  if (client.printmessage) {
    if (minutes != 0) {
      if (seconds != 0) {
        if (asprintf(&text, "%d Minute%s and %d Second%s Remaining!", minutes, minutes > 1 ? "s" : "", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
      else {
        if (asprintf(&text, "%d Minute%s Remaining!", minutes, minutes > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
    }
    else {
      if (seconds != 0) {
        if (asprintf(&text, "%d Second%s Remaining!", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
      }
      else {
        if (asprintf(&text, "Base Control Reached!", seconds, seconds > 1 ? "s" : "") == -1) LOGFAIL(errno)
        client.printmessage(MSGGAME, text);
        free(text);
        text = NULL;
        client.basecontrolreached = 1;
      }
    }
  }

  if (readbuf(&client.recvbuf, NULL, sizeof(struct SRBaseControl)) == -1) LOGFAIL(errno)

CLEANUP
  if (text) {
    free(text);
  }

ERRHANDLER(0, -1)
END
}

static int sendclsmallboom(int x, int y) {
  struct CLSmallBoom clsmallboom;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clsmallboom.type = CLSMALLBOOM;
  clsmallboom.x = x;
  clsmallboom.y = y;

  if (writebuf(&client.sendbuf, &clsmallboom, sizeof(clsmallboom)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

static int sendclsuperboom(int x, int y) {
  struct CLSuperBoom clsuperboom;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clsuperboom.type = CLSUPERBOOM;
  clsuperboom.x = x;
  clsuperboom.y = y;

  if (writebuf(&client.sendbuf, &clsuperboom, sizeof(clsuperboom)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendcldropmine(int x, int y) {
  struct CLDropMine cldropmine;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  cldropmine.type = CLDROPMINE;
  cldropmine.x = x;
  cldropmine.y = y;

  if (writebuf(&client.sendbuf, &cldropmine, sizeof(cldropmine)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

static int sendcltouch(int x, int y) {
  struct CLTouch cltouch;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  cltouch.type = CLTOUCH;
  cltouch.x = x;
  cltouch.y = y;

  if (writebuf(&client.sendbuf, &cltouch, sizeof(cltouch)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclgrabtile(int x, int y) {
  struct CLGrabTile clgrabtile;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clgrabtile.type = CLGRABTILE;
  clgrabtile.x = x;
  clgrabtile.y = y;

  if (writebuf(&client.sendbuf, &clgrabtile, sizeof(clgrabtile)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclgrabtrees(int x, int y) {
  struct CLGrabTrees clgrabtrees;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clgrabtrees.type = CLGRABTREES;
  clgrabtrees.x = x;
  clgrabtrees.y = y;

  if (writebuf(&client.sendbuf, &clgrabtrees, sizeof(clgrabtrees)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclbuildroad(int x, int y, int trees) {
  struct CLBuildRoad clbuildroad;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clbuildroad.type = CLBUILDROAD;
  clbuildroad.x = x;
  clbuildroad.y = y;
  clbuildroad.trees = trees;

  if (writebuf(&client.sendbuf, &clbuildroad, sizeof(clbuildroad)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclbuildwall(int x, int y, int trees) {
  struct CLBuildWall clbuildwall;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clbuildwall.type = CLBUILDWALL;
  clbuildwall.x = x;
  clbuildwall.y = y;
  clbuildwall.trees = trees;

  if (writebuf(&client.sendbuf, &clbuildwall, sizeof(clbuildwall)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclbuildboat(int x, int y, int trees) {
  struct CLBuildBoat clbuildboat;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clbuildboat.type = CLBUILDBOAT;
  clbuildboat.x = x;
  clbuildboat.y = y;
  clbuildboat.trees = trees;

  if (writebuf(&client.sendbuf, &clbuildboat, sizeof(clbuildboat)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclbuildpill(int x, int y, int trees, int pill) {
  struct CLBuildPill clbuildpill;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clbuildpill.type = CLBUILDPILL;
  clbuildpill.x = x;
  clbuildpill.y = y;
  clbuildpill.trees = trees;
  clbuildpill.pill = pill;

  if (writebuf(&client.sendbuf, &clbuildpill, sizeof(clbuildpill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclrepairpill(int x, int y, int trees) {
  struct CLRepairPill clrepairpill;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clrepairpill.type = CLREPAIRPILL;
  clrepairpill.x = x;
  clrepairpill.y = y;
  clrepairpill.trees = trees;

  if (writebuf(&client.sendbuf, &clrepairpill, sizeof(clrepairpill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

static int sendclplacemine(int x, int y) {
  struct CLPlaceMine clplacemine;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  clplacemine.type = CLPLACEMINE;
  clplacemine.x = x;
  clplacemine.y = y;

  if (writebuf(&client.sendbuf, &clplacemine, sizeof(clplacemine)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

static int sendcldamage(int x, int y, int boat) {
  struct CLDamage cldamage;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  cldamage.type = CLDAMAGE;
  cldamage.x = x;
  cldamage.y = y;
  cldamage.boat = boat;

  if (writebuf(&client.sendbuf, &cldamage, sizeof(cldamage)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendcldropboat(int x, int y) {
  struct CLDropBoat cldropboat;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  cldropboat.type = CLDROPBOAT;
  cldropboat.x = x;
  cldropboat.y = y;

  if (writebuf(&client.sendbuf, &cldropboat, sizeof(cldropboat)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendcldroppills(float x, float y, uint16_t pills) {
  int i;
  struct CLDropPills cldroppills;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  for (i = 0; i < client.npills; i++) {
    if (pills & (1 << i)) {
      assert(client.pills[i].owner == client.player);
    }
  }

  assert((pills >> i) == 0);

TRY
  cldroppills.type = CLDROPPILLS;
  cldroppills.x = htonl(*((uint32_t *)&x));
  cldroppills.y = htonl(*((uint32_t *)&y));
  cldroppills.pills = htons(pills);

  if (writebuf(&client.sendbuf, &cldroppills, sizeof(cldroppills)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclrefuel(int base, int armour, int shells, int mines) {
  struct CLRefuel clrefuel;

  assert(base >= 0);
  assert(base < client.nbases);
  assert(armour >= 0);
  assert(armour < 256);
  assert(shells >= 0);
  assert(shells < 256);
  assert(mines >= 0);
  assert(mines < 256);

TRY
  clrefuel.type = CLREFUEL;
  clrefuel.base = base;
  clrefuel.armour = armour;
  clrefuel.shells = shells;
  clrefuel.mines = mines;

  if (writebuf(&client.sendbuf, &clrefuel, sizeof(clrefuel)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclhittank(int player, float dir) {
  struct CLHitTank clhittank;

  assert(player >= 0);
  assert(client.players[player].connected);
  assert(dir >= 0.0);
  assert(dir < k2Pif);

TRY
  clhittank.type = CLHITTANK;
  clhittank.player = player;
  clhittank.dir = htonl(*((uint32_t *)&dir));

  if (writebuf(&client.sendbuf, &clhittank, sizeof(clhittank)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendclupdate() {
  struct CLUpdate clupdate;
  struct CLUpdateShell *updateshells;
  struct CLUpdateExplosion *updateexplosions;
  struct ListNode *node;
  int i;

TRY
  client.players[client.player].lastupdate = client.players[client.player].seq;

  clupdate.hdr.player = client.player;

  for (i = 0; i < MAXPLAYERS; i++) {
    clupdate.hdr.seq[i] = htonl(client.players[i].seq);
  }

  clupdate.hdr.tankstatus = client.players[client.player].dead ? kTankDead : (client.players[client.player].boat ? kTankOnBoat : kTankNormal);
  clupdate.hdr.tankx = htonl(*((uint32_t *)&client.players[client.player].tank.x));
  clupdate.hdr.tanky = htonl(*((uint32_t *)&client.players[client.player].tank.y));
  clupdate.hdr.tankspeed = htonl(*((uint32_t *)&client.players[client.player].speed));
  clupdate.hdr.tankturnspeed = htonl(*((uint32_t *)&client.players[client.player].turnspeed));
  clupdate.hdr.tankkickdir = htonl(*((uint32_t *)&client.players[client.player].kickdir));
  clupdate.hdr.tankkickspeed = htonl(*((uint32_t *)&client.players[client.player].kickspeed));
  clupdate.hdr.tankdir = (uint8_t)(client.players[client.player].dir*(FWIDTH/k2Pif));
  clupdate.hdr.builderstatus = client.players[client.player].builderstatus;
  clupdate.hdr.builderx = htonl(*((uint32_t *)&client.players[client.player].builder.x));
  clupdate.hdr.buildery = htonl(*((uint32_t *)&client.players[client.player].builder.y));
  clupdate.hdr.buildertargetx = client.players[client.player].buildertarget.x;
  clupdate.hdr.buildertargety = client.players[client.player].buildertarget.y;
  clupdate.hdr.builderwait = client.players[client.player].builderwait;
  clupdate.hdr.inputflags = htonl(client.players[client.player].inputflags);
  clupdate.hdr.tankshotsound = client.tankshotsound;
  clupdate.hdr.pillshotsound = client.pillshotsound;
  clupdate.hdr.sinksound = client.sinksound;
  clupdate.hdr.builderdeathsound = client.builderdeathsound;
  clupdate.hdr.nshells = 0;
  clupdate.hdr.nexplosions = 0;

  client.tankshotsound = 0;
  client.pillshotsound = 0;
  client.sinksound = 0;
  client.builderdeathsound = 0;

  updateshells = (void *)clupdate.buf;
  node = nextlist(&client.players[client.player].shells);

  while (node != NULL && clupdate.hdr.nshells < CLUPDATEMAXSHELLS) {
    struct Shell *shell;

    shell = ptrlist(node);

    updateshells[clupdate.hdr.nshells].owner = shell->owner;
    updateshells[clupdate.hdr.nshells].shellx = htons((uint16_t)(shell->point.x*FWIDTH));
    updateshells[clupdate.hdr.nshells].shelly = htons((uint16_t)(shell->point.y*FWIDTH));
    updateshells[clupdate.hdr.nshells].boat = shell->boat;
    updateshells[clupdate.hdr.nshells].pill = shell->pill;
    updateshells[clupdate.hdr.nshells].shelldir = (uint8_t)(shell->dir*(FWIDTH/k2Pif));
    updateshells[clupdate.hdr.nshells].range = htons((uint16_t)(shell->range*FWIDTH));
    node = nextlist(node);
    clupdate.hdr.nshells++;
  }

  updateexplosions = (void *)(updateshells + clupdate.hdr.nshells);
  node = nextlist(&client.players[client.player].explosions);

  while (node != NULL && clupdate.hdr.nexplosions < CLUPDATEMAXEXPLOSIONS) {
    struct Explosion *explosion;

    explosion = ptrlist(node);
    updateexplosions[clupdate.hdr.nexplosions].explosionx = htons((uint16_t)(explosion->point.x*FWIDTH));
    updateexplosions[clupdate.hdr.nexplosions].explosiony = htons((uint16_t)(explosion->point.y*FWIDTH));
    updateexplosions[clupdate.hdr.nexplosions].counter = explosion->counter;
    node = nextlist(node);
    clupdate.hdr.nexplosions++;
  }

  if (send(client.dgramsock, &clupdate, sizeof(clupdate.hdr) + clupdate.hdr.nshells*sizeof(struct CLUpdateShell) + clupdate.hdr.nexplosions*sizeof(struct CLUpdateExplosion), 0) == -1) {
    LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

float maxspeed(int x, int y) {
  int pill, base;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  if ((pill = findpill(x, y)) != -1) {
    if (client.pills[pill].armour > 0) {
      return 0.0;
    }
    else {
      return 3.125;
    }
  }

  if ((base = findbase(x, y)) != -1) {
    return 3.125;
  }

  switch (client.terrain[y][x]) {
  case kRiverTerrain:  /* river */
  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRubbleTerrain:  /* mined rubble */
    return RUBBLEMAXSPEED;

  case kForestTerrain:  /* forest */
  case kMinedForestTerrain:  /* mined forest */
    return FORESTMAXSPEED;

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kMinedGrassTerrain:  /* mined grass */
    return GRASSMAXSPEED;

  case kRoadTerrain:  /* road */
  case kBoatTerrain:  /* river w/boat */
  case kMinedRoadTerrain:  /* mined road */
    return ROADMAXSPEED;

  case kSeaTerrain:  /* sea */
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
  case kMinedSeaTerrain:  /* mined sea */
  default:
    return 0.0;
  }
}

float maxturnspeed(int x, int y) {
  int pill, base;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  if ((pill = findpill(x, y)) != -1) {
    if (client.pills[pill].armour > 0) {
      return 0.0;
    }
    else {
      return 2.5;
    }
  }

  if ((base = findbase(x, y)) != -1) {
    return 2.5;
  }

  switch (client.terrain[y][x]) {
  case kRiverTerrain:  /* river */
  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRubbleTerrain:  /* mined rubble */
    return 0.625;

  case kForestTerrain:  /* forest */
  case kMinedForestTerrain:  /* mined forest */
    return 1.25;

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kMinedGrassTerrain:  /* mined grass */
  case kRoadTerrain:  /* road */
  case kBoatTerrain:  /* river w/boat */
  case kMinedRoadTerrain:  /* mined road */
    return 2.5;

  case kSeaTerrain:  /* sea */
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
  case kMinedSeaTerrain:  /* mined sea */
  default:
    return 0.0;
  }
}

float builderspeed(int x, int y, int player) {
  int pill, base;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  if ((pill = findpill(x, y)) != -1) {
    if (client.pills[pill].armour > 0) {
      return 0.0;
    }
    else {
      return BUILDERMAXSPEED;
    }
  }

  if ((base = findbase(x, y)) != -1) {
    if (
      client.bases[base].owner == NEUTRAL || client.bases[base].armour == 0 ||
      testalliance(client.bases[base].owner, player)
    ) {
      return BUILDERMAXSPEED;
    }
    else {
      return 0.0;
    }
  }

  switch (client.terrain[y][x]) {
  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRubbleTerrain:  /* mined rubble */
    return BUILDERMAXSPEED*0.25;

  case kForestTerrain:  /* forest */
  case kMinedForestTerrain:  /* mined forest */
    return BUILDERMAXSPEED*0.5;

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kMinedGrassTerrain:  /* mined grass */
  case kRoadTerrain:  /* road */
  case kBoatTerrain:  /* river w/boat */
  case kMinedRoadTerrain:  /* mined road */
    return BUILDERMAXSPEED;

  case kRiverTerrain:  /* river */
  case kSeaTerrain:  /* sea */
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
  case kMinedSeaTerrain:  /* mined sea */
    return 0.0;

  default:
    assert(0);
  }
}

float buildertargetspeed(int x, int y) {
  int pill, base;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  if ((pill = findpill(x, y)) != -1) {
    return BUILDERMAXSPEED;
  }

  if ((base = findbase(x, y)) != -1) {
    return BUILDERMAXSPEED;
  }

  switch (client.terrain[y][x]) {
  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRubbleTerrain:  /* mined rubble */
    return BUILDERMAXSPEED*0.25;

  case kForestTerrain:  /* forest */
  case kMinedForestTerrain:  /* mined forest */
    return BUILDERMAXSPEED*0.5;

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kMinedGrassTerrain:  /* mined grass */
  case kRoadTerrain:  /* road */
  case kBoatTerrain:  /* river w/boat */
  case kMinedRoadTerrain:  /* mined road */
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
  case kRiverTerrain:  /* river */
    return BUILDERMAXSPEED;

  case kSeaTerrain:  /* sea */
  case kMinedSeaTerrain:  /* mined sea */
    return 0.0;

  default:
    assert(0);
    return 0.0;
  }
}

int decreasevis(Recti r) {
  int x, y;

  assert(r.size.width > 0 && r.size.height > 0);

TRY
  r = intersectionrect(makerect(0, 0, WIDTH, WIDTH), r);

  for (y = minyrect(r); y <= maxyrect(r); y++) {
    for (x = minxrect(r); x <= maxxrect(r); x++) {
      if (--client.fog[y][x] == 0) {
        Pointi *p;

        if ((p = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
        p->x = x;
        p->y = y;
        if (addlist(&client.changedtiles, p) == -1) LOGFAIL(errno)
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int increasevis(Recti r) {
  int x, y;

  assert(r.size.width > 0 && r.size.height > 0);

TRY
  r = intersectionrect(makerect(0, 0, WIDTH, WIDTH), r);

  for (y = minyrect(r); y <= maxyrect(r); y++) {
    for (x = minxrect(r); x <= maxxrect(r); x++) {
      client.fog[y][x]++;
    }
  }

  r = insetrect(r, -1, -1);
  r = intersectionrect(makerect(0, 0, WIDTH, WIDTH), r);

  for (y = minyrect(r); y <= maxyrect(r); y++) {
    for (x = minxrect(r); x <= maxxrect(r); x++) {
      if (client.fog[y][x] <= 1) {
        client.seentiles[y][x] = fogtilefor(x, y, client.seentiles[y][x]);
      }
    }
  }

  for (y = minyrect(r); y <= maxyrect(r); y++) {
    for (x = minxrect(r); x <= maxxrect(r); x++) {
      Pointi *p;
      int image;

      image = mapimage(client.seentiles, x, y);

      if (image != client.images[y][x] || client.fog[y][x] == 1) {
        client.images[y][x] = image;
        if ((p = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
        p->x = x;
        p->y = y;
        if (addlist(&client.changedtiles, p) == -1) LOGFAIL(errno)
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int isshore(int x, int y) {
  int i;

  if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
    return 0;
  }

  for (i = 0; i < client.nbases; i++) {
    if (client.bases[i].x == x && client.bases[i].y == y) {
      return 1;
    }
  }

  switch (client.terrain[y][x]) {
	case kSeaTerrain:
	case kRiverTerrain:
	case kMinedSeaTerrain:
    return 0;

	case kBoatTerrain:
	case kWallTerrain:
	case kSwampTerrain0:
	case kSwampTerrain1:
	case kSwampTerrain2:
	case kSwampTerrain3:
	case kCraterTerrain:
	case kRoadTerrain:
	case kForestTerrain:
	case kRubbleTerrain0:
	case kRubbleTerrain1:
	case kRubbleTerrain2:
	case kRubbleTerrain3:
	case kGrassTerrain0:
	case kGrassTerrain1:
	case kGrassTerrain2:
	case kGrassTerrain3:
	case kDamagedWallTerrain0:
	case kDamagedWallTerrain1:
	case kDamagedWallTerrain2:
	case kDamagedWallTerrain3:
	case kMinedSwampTerrain:
	case kMinedCraterTerrain:
	case kMinedRoadTerrain:
	case kMinedForestTerrain:
  case kMinedRubbleTerrain:
	case kMinedGrassTerrain:
    return 1;

  default:
    assert(0);
    return 0;
  }
}

int tankmovelogic(int player) {
TRY
  if (client.players[player].connected) {
    if (client.players[player].dead) {
      if (player == client.player) {
        client.respawncounter++;

        if (client.respawncounter < EXPLODETICKS) {
          client.players[player].tank = add2f(client.players[player].tank, mul2f(dir2vec(client.players[player].kickdir), client.players[player].kickspeed/TICKSPERSEC));

          /* collisions with terrain */
          collisionowner = player;
          client.players[player].tank = collisiondetect(client.players[player].tank, TANKRADIUS, tankcollision);

          if (client.respawncounter%5 == 0) {
            struct Explosion *explosion;

            switch (client.terrain[w2t(client.players[player].tank.y)][w2t(client.players[player].tank.x)]) {
            case kSeaTile:
            case kMinedSeaTile:
              break;

            default:
              if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
              explosion->point = client.players[client.player].tank;
              explosion->counter = 0;
              if (addlist(&client.players[client.player].explosions, explosion)) LOGFAIL(errno)
              if (killpointbuilder(explosion->point)) LOGFAIL(errno)
              break;
            }
          }
        }
        else if (client.respawncounter == EXPLODETICKS) {
          if (client.mines >= 32) {
            if (superboom()) LOGFAIL(errno)
          }
          else if (client.mines > 0 || client.shells > 0) {
            if (smallboom()) LOGFAIL(errno)
          }
        }
        else if (client.respawncounter >= RESPAWN_TICKS) {
          if (spawn()) LOGFAIL(errno)
        }
      }
    }
    else {
      float max;
      /* turn */
      if ((client.players[player].inputflags & TURNLMASK) && !(client.players[player].inputflags & TURNRMASK)) {
        if(client.players[player].turnspeed < 0)
          client.players[player].turnspeed = 0;
        max = client.players[player].boat ? MAXANGULARVELOCITY : maxturnspeed((int)client.players[client.player].tank.x, (int)client.players[client.player].tank.y);

        if (client.players[player].turnspeed > max) {
          client.players[player].turnspeed -= ANGULARACCEL/TICKSPERSEC;

          if (client.players[player].turnspeed < max) {
            client.players[player].turnspeed = max;
          }
        }
        else {
          client.players[player].turnspeed += ANGULARACCEL/TICKSPERSEC;

          if (client.players[player].turnspeed > max) {
            client.players[player].turnspeed = max;
          }
        }
      }
      else if ((client.players[player].inputflags & TURNRMASK) && !(client.players[player].inputflags & TURNLMASK)) {
        if(client.players[player].turnspeed > 0)
          client.players[player].turnspeed = 0;
        max = client.players[player].boat ? MAXANGULARVELOCITY : maxturnspeed((int)client.players[client.player].tank.x, (int)client.players[client.player].tank.y);
          
        if (client.players[player].turnspeed < -max) {
          client.players[player].turnspeed += ANGULARACCEL/TICKSPERSEC;
            
          if (client.players[player].turnspeed > -max) {
            client.players[player].turnspeed = -max;
          }
        }
        else {
          client.players[player].turnspeed -= ANGULARACCEL/TICKSPERSEC;
            
          if (client.players[player].turnspeed < -max) {
            client.players[player].turnspeed = -max;
          }
        }
      }
      else {
        client.players[player].turnspeed = 0.0;
      }

      client.players[player].dir += client.players[player].turnspeed/TICKSPERSEC;

      if (client.players[player].dir > k2Pif) {
        client.players[player].dir -= k2Pif*floorf(client.players[player].dir/k2Pif);
      }
      else if (client.players[player].dir < 0.0) {
        client.players[player].dir += k2Pif*floorf(client.players[player].dir/-k2Pif + 1.0);
      }

      /* acceleration */
      max = client.players[player].boat ? BOATMAXSPEED : maxspeed((int)client.players[client.player].tank.x, (int)client.players[client.player].tank.y);

      /* accelerate forward */
      if ((client.players[player].inputflags & ACCELMASK) && !(client.players[player].inputflags & BRAKEMASK)) {
        if (client.players[player].speed < max) {
          client.players[player].speed += ACCEL/TICKSPERSEC;

          if (client.players[player].speed > max) {
            client.players[player].speed = max;
          }
        }
        else {
          client.players[player].speed -= ACCEL/TICKSPERSEC;

          if (client.players[player].speed < max) {
            client.players[player].speed = max;
          }
        }
      } /* accelerate backward */
      else if (!(client.players[player].inputflags & ACCELMASK) && (client.players[player].inputflags & BRAKEMASK)) {
        client.players[player].speed -= ACCEL/TICKSPERSEC;

        if (client.players[player].speed < 0.0) {
          client.players[player].speed = 0.0;
        }
      }
      else if (client.players[player].speed > max) {
        client.players[player].speed -= ACCEL/TICKSPERSEC;

        if (client.players[player].speed < max) {
          client.players[player].speed = max;
        }
      }

      client.players[player].tank = add2f(client.players[player].tank, div2f(add2f(mul2f(dir2vec(rounddir(client.players[player].dir)), client.players[player].speed), mul2f(dir2vec(client.players[player].kickdir), client.players[player].kickspeed)), TICKSPERSEC));

      /* reduce kickspeed */
      client.players[player].kickspeed -= 12.0/TICKSPERSEC;

      if (client.players[player].kickspeed < 0.0) {
        client.players[player].kickspeed = 0.0;
      }

      /* shore push */
      if (client.players[player].boat) {
        int x, y, fxc, cxc, fyc, cyc;
        float fx, fy, cx, cy;
        Vec2f push = make2f(0.0, 0.0);

        x = (int)client.players[player].tank.x;
        y = (int)client.players[player].tank.y;
        fx = client.players[player].tank.x - x;
        fy = client.players[player].tank.y - y;
        cx = 1.0 - fx;
        cy = 1.0 - fy;
        fxc = (fx < TANKRADIUS) && isshore(x - 1, y);
        cxc = ((1.0 - fx) < TANKRADIUS) && isshore(x + 1, y);
        fyc = (fy < TANKRADIUS) && isshore(x, y - 1);
        cyc = ((1.0 - fy) < TANKRADIUS) && isshore(x, y + 1);

        if (!fxc && !fyc && (((fx*fx + fy*fy) < (TANKRADIUS*TANKRADIUS)) && isshore(x - 1, y - 1))) {
          push.x = fx;
          push.y = fy;
        }
        else if (!cxc && !fyc && (((cx*cx + fy*fy) < (TANKRADIUS*TANKRADIUS)) && isshore(x + 1, y - 1))) {
          push.x -= cx;
          push.y = fy;
        }
        else if (!fxc && !cyc && (((fx*fx + cy*cy) < (TANKRADIUS*TANKRADIUS)) && isshore(x - 1, y + 1))) {
          push.x = fx;
          push.y -= cy;
        }
        else if (!cxc && !cyc && (((cx*cx + cy*cy) < (TANKRADIUS*TANKRADIUS)) && isshore(x + 1, y + 1))) {
          push.x -= cx;
          push.y -= cy;
        }
        else {
          if (fxc) {
            if (fyc) {
              push.x = fy;
              push.y = fx;
            }
            else if (cyc) {
              push.x = cy;
              push.y = -fx;
            }
            else {
              push.x = fx;
              push.y = 0.0;
            }
          }
          else if (cxc) {
            if (fyc) {
              push.x = -fy;
              push.y = cx;
            }
            else if (cyc) {
              push.x = -cy;
              push.y = -cx;
            }
            else {
              push.x = -cx;
              push.y = 0.0;
            }
          }
          else {
            if (fyc) {
              push.x = 0.0;
              push.y = fy;
            }
            else if (cyc) {
              push.x = 0.0;
              push.y = -cy;
            }
          }
        }

        if (mag2f(push) > 0.00001) {
          float f;

          f = mag2f(prj2f(push, mul2f(dir2vec(client.players[player].dir), client.players[player].speed)));

          if (f < PUSHFORCE) {
            client.players[player].tank = add2f(client.players[player].tank, mul2f(unit2f(push), PUSHFORCE/TICKSPERSEC));
          }

          if (!((client.players[player].inputflags & ACCELMASK) && !(client.players[player].inputflags & BRAKEMASK))) {
            client.players[player].speed -= ACCEL/TICKSPERSEC;

            if (client.players[player].speed < 0.0) {
              client.players[player].speed = 0.0;
            }
          }
        }
      }

      /* collisions with terrain */
      collisionowner = player;
      client.players[player].tank = collisiondetect(client.players[player].tank, TANKRADIUS, tankcollision);
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int tanklocallogic(Pointi old) {
  int pill, base, i, j, x, y;
  Pointi new;

TRY

  if (old.x < 0 || old.x >= WIDTH || old.y < 0 || old.y >= WIDTH) {
    SUCCESS
  }

  /* local tank logic */
  if (client.players[client.player].connected) {
    if (!client.players[client.player].dead) {
      Pointi tmp;

      tmp = makepoint(client.players[client.player].tank.x, client.players[client.player].tank.y);

      /* collisions with tanks */
      for (i = 0; i < MAXPLAYERS; i++) {
        if (i != client.player && client.players[i].connected && !client.players[i].dead) {
          Vec2f diff;
          float mag;

          diff = sub2f(client.players[client.player].tank, client.players[i].tank);
          mag = mag2f(diff);
          if (mag < TANKRADIUS*2.0) {
            if (mag < 0.00001) {
              client.players[client.player].tank = add2f(client.players[i].tank, mul2f(tan2f((random()%16)*(kPif/8.0)), TANKRADIUS*2.0));
            }
            else {
              client.players[client.player].tank = add2f(client.players[i].tank, mul2f(diff, (TANKRADIUS*2.0)/mag));
            }
          }
        }
      }

      new = makepoint(client.players[client.player].tank.x, client.players[client.player].tank.y);

      if (!isequalpoint(new, tmp)) {
        if (increasevis(makerect(new.x - 14, new.y - 14, 29, 29))) LOGFAIL(errno)
        if (decreasevis(makerect(tmp.x - 14, tmp.y - 14, 29, 29))) LOGFAIL(errno)
      }
    }
    else {
      new = makepoint(client.players[client.player].tank.x, client.players[client.player].tank.y);
    }

    /* show hidden mines within 1 square */
    x = w2t(client.players[client.player].tank.x) - 1;
    y = w2t(client.players[client.player].tank.y) - 1;
    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
        if (testhiddenmine(x + j, y + i)) LOGFAIL(errno)
      }
    }

    enter(new, old);

    if (new.x < 0 || new.x >= WIDTH || new.y < 0 || new.y >= WIDTH) {
      pill = -1;
    }
    else {
      pill = findpill(new.x, new.y);
    }

    if (new.x < 0 || new.x >= WIDTH || new.y < 0 || new.y >= WIDTH) {
      base = -1;
    }
    else {
      base = findbase(new.x, new.y);
    }
    
    /* refuel or drain */
    if (!client.players[client.player].dead) {
      /* drain resources on river terrain */
      if (!client.players[client.player].boat &&
          pill == -1 && base == -1 && client.terrain[new.y][new.x] == kRiverTerrain &&
          client.players[client.player].speed <= 0.5859375
        ) {
        client.draincounter++;

        if (client.playsound) {
          client.playsound(kBubblesSound);
        }

        if (client.draincounter >= DRAINTICKS) {
          client.draincounter = 0;
          client.shells--;

          if (client.shells < 0) {
            client.shells = 0;
          }

          client.mines--;

          if (client.mines < 0) {
            client.mines = 0;
          }

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
      }
      else {
        client.draincounter = 0;
      }

      if (!client.refueling) {
        if (base != -1) {
          client.refueling = 1;
          client.refuelingbase = base;
          client.refuelingcounter = 0;
        }
      }
      else if (isequalpoint(new, old)) {
        int armour;

        client.refuelingcounter++;

        if (
          client.armour < MAXARMOUR && client.bases[client.refuelingbase].armour > MINBASEARMOUR &&
          (armour = MIN(MAXARMOUR - client.armour, MIN(client.bases[client.refuelingbase].armour - MINBASEARMOUR, MINBASEARMOUR))) > 0
        ) {
          if (client.refuelingcounter >= REFUELARMOURTICKS) {
            sendclrefuel(client.refuelingbase, armour, 0, 0);
            client.bases[client.refuelingbase].armour -= armour;
            client.armour += armour;
            client.refuelingcounter = 0;

            if (client.settankstatus) {
              client.settankstatus();
            }
          }
        }
        else if (client.shells < MAXSHELLS && client.bases[client.refuelingbase].shells >= MINBASESHELLS) {
          if (client.refuelingcounter >= REFUELSHELLSTICKS) {
            if (client.shells > MAXSHELLS - MINBASESHELLS) {
              sendclrefuel(client.refuelingbase, 0, MAXSHELLS - client.shells, 0);
              client.bases[client.refuelingbase].shells -= MAXSHELLS - client.shells;
              client.shells += MAXSHELLS - client.shells;
            }
            else {
              sendclrefuel(client.refuelingbase, 0, MINBASESHELLS, 0);
              client.bases[client.refuelingbase].shells -= MINBASESHELLS;
              client.shells += MINBASESHELLS;
            }

            client.refuelingcounter = 0;

            if (client.settankstatus) {
              client.settankstatus();
            }
          }
        }
        else if (client.mines < MAXMINES && client.bases[client.refuelingbase].mines >= MINBASEMINES) {
          if (client.refuelingcounter >= REFUELMINESTICKS) {
            if (client.mines > MAXMINES - MINBASEMINES) {
              sendclrefuel(client.refuelingbase, 0, 0, MAXMINES - client.mines);
              client.bases[client.refuelingbase].mines -= MAXMINES - client.mines;
              client.mines += MAXMINES - client.mines;
            }
            else {
              sendclrefuel(client.refuelingbase, 0, 0, MINBASEMINES);
              client.bases[client.refuelingbase].mines -= MINBASEMINES;
              client.mines += MINBASEMINES;
            }

            client.refuelingcounter = 0;

            if (client.settankstatus) {
              client.settankstatus();
            }
          }
        }
      }
      else {
        client.refueling = 0;
        client.refuelingbase = -1;
        client.refuelingcounter = 0;
      }

      /* increase range */
      if ((client.players[client.player].inputflags & INCREMASK) && !(client.players[client.player].inputflags & DECREMASK)) {
        client.range += DRANGE/TICKSPERSEC;

        if (client.range > MAXRANGE) {
          client.range = MAXRANGE;
        }
      }  /* decrease range */
      else if (!(client.players[client.player].inputflags & INCREMASK) && (client.players[client.player].inputflags & DECREMASK)) {
        client.range -= DRANGE/TICKSPERSEC;

        if (client.range < MINRANGE) {
          client.range = MINRANGE;
        }
      }

      /* fire shell */
      if ((client.players[client.player].inputflags & SHOOTMASK) && (client.shellcounter > TICKSPERSEC/SHELLRATE)) {
        if (client.shells > 0) {
          struct Shell *shell;

          if ((shell = (struct Shell *)malloc(sizeof(struct Shell))) == NULL) LOGFAIL(errno)
          shell->owner = client.player;
          shell->point = add2f(client.players[client.player].tank, mul2f(dir2vec(client.players[client.player].dir), 0.5));
          shell->boat = client.players[client.player].boat;
          shell->pill = 0;
          shell->dir = client.players[client.player].dir;
          shell->range = client.range - 0.5;
          addlist(&client.players[client.player].shells, shell);
          client.shells--;

          client.shellcounter = 0;

          if (client.settankstatus) {
            client.settankstatus();
          }

          if (client.playsound) {
            client.playsound(kTankShotSound);
          }

          client.tankshotsound = 1;
        }
      }

      client.shellcounter++;
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int testhiddenmine(int x, int y) {
TRY
  if (mag2f(sub2f(make2f(x + 0.5, y + 0.5), client.players[client.player].tank)) <= 2.0) {
    switch (client.terrain[y][x]) {
    case kMinedSwampTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    case kMinedCraterTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    case kMinedRoadTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    case kMinedForestTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    case kMinedRubbleTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    case kMinedGrassTerrain:
      if (refresh(x, y)) LOGFAIL(errno)
      break;

    default:
      break;
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}


int tanktest(int x, int y) {
  int i;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected && !client.players[i].dead && (int)client.players[i].tank.x == x && (int)client.players[i].tank.y == y) {
      return 1;
    }
  }

  return 0;
}

int tankonaboattest(int x, int y) {
  int i;
  
  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);
  
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected) {
      if (!client.players[i].dead && client.players[i].boat && (int)client.players[i].tank.x == x && (int)client.players[i].tank.y == y) {
        return 1;
      }
    }
  }
  
  return 0;
}

int builderlogic(int player) {
  int i;
  Vec2f diff;
  float mag;

TRY
  if (!client.players[player].connected) {
    SUCCESS
  }

  switch (client.players[player].builderstatus) {
  case kBuilderReady:
    if (player == client.player) {
      client.buildertask = getbuildertaskforcommand(client.nextbuildercommand, client.nextbuildertarget);
      client.nextbuildercommand = BUILDERNILL;

      switch (client.buildertask) {
      case kBuilderDoNothing:
        break;

      case kBuilderGetTree:
        client.players[player].buildertarget = client.nextbuildertarget;
        client.nextbuildertarget = makepoint(0, 0);
        diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
        mag = mag2f(diff);
        client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));
        client.players[player].builderstatus = kBuilderGoto;
        client.buildermines = 0;
        client.buildertrees = 0;
        client.builderpill = NOPILL;

        if (client.settankstatus) {
          client.settankstatus();
        }

        break;

      case kBuilderBuildRoad:
        if (client.trees >= ROADTREES) {
          client.players[player].buildertarget = client.nextbuildertarget;
          client.nextbuildertarget = makepoint(0, 0);
          diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
          mag = mag2f(diff);
          client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));
          client.players[player].builderstatus = kBuilderGoto;
          client.buildermines = 0;
          client.buildertrees = ROADTREES;
          client.trees -= ROADTREES;
          client.builderpill = NOPILL;

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more trees.");
          }
        }

        break;

      case kBuilderBuildWall:
        if (client.trees >= WALLTREES) {
          client.players[player].buildertarget = client.nextbuildertarget;
          client.nextbuildertarget = makepoint(0, 0);
          diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
          mag = mag2f(diff);
          client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));            
          client.players[player].builderstatus = kBuilderGoto;
          client.buildermines = 0;
          client.buildertrees = WALLTREES;
          client.trees -= WALLTREES;
          client.builderpill = NOPILL;

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more trees.");
          }
        }

        break;

      case kBuilderBuildBoat:
        if (client.trees >= BOATTREES) {
          client.players[player].buildertarget = client.nextbuildertarget;
          client.nextbuildertarget = makepoint(0, 0);
          diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
          mag = mag2f(diff);
          client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));
          client.players[player].builderstatus = kBuilderGoto;
          client.buildermines = 0;
          client.buildertrees = BOATTREES;
          client.trees -= BOATTREES;
          client.builderpill = NOPILL;

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more trees.");
          }
        }

        break;

      case kBuilderBuildPill:
        if (client.trees >= 4) {
          for (i = 0; i < client.npills; i++) {
            if (client.pills[i].owner == player && client.pills[i].armour == ONBOARD) {
              client.players[player].buildertarget = client.nextbuildertarget;
              client.nextbuildertarget = makepoint(0, 0);
              diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
              mag = mag2f(diff);
              client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));
              client.players[player].builderstatus = kBuilderGoto;
              client.buildermines = 0;

              if (client.trees >= PILLTREES) {
                client.buildertrees = PILLTREES;
                client.trees -= PILLTREES;
              }
              else {
                client.buildertrees = client.trees;
                client.trees = 0;
              }

              client.builderpill = i;

              if (client.settankstatus) {
                client.settankstatus();
              }

              break;
            }
          }

          if (i == client.npills) {
            client.nextbuildertarget = makepoint(0, 0);

            if (client.printmessage) {
              client.printmessage(MSGGAME, "You need a pill.");
            }
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more trees.");
          }
        }

        break;

      case kBuilderRepairPill:
        if (client.trees > 0) {
          int needed;

          client.players[player].buildertarget = client.nextbuildertarget;
          client.nextbuildertarget = makepoint(0, 0);
          diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
          mag = mag2f(diff);
          client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));
          client.players[player].builderstatus = kBuilderGoto;
          client.buildermines = 0;

          switch (client.seentiles[client.players[player].buildertarget.y][client.players[player].buildertarget.x]) {
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
            needed = (kFriendlyPill15Tile - client.seentiles[client.players[player].buildertarget.y][client.players[player].buildertarget.x] + 3)/4;
            break;

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
            needed = (kHostilePill15Tile - client.seentiles[client.players[player].buildertarget.y][client.players[player].buildertarget.x] + 3)/4;
            break;

          default:
            needed = 0;
            break;
          }

          if (client.trees > needed) {
            client.buildertrees += needed;
            client.trees -= needed;
          }
          else {
            client.buildertrees += client.trees;
            client.trees = 0;
          }

          client.builderpill = NOPILL;

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more trees.");
          }
        }

        break;

      case kBuilderPlaceMine:
        if (client.mines > 0) {
          client.players[player].buildertarget = client.nextbuildertarget;
          client.nextbuildertarget = makepoint(0, 0);
          diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].tank);
          mag = mag2f(diff);
          client.players[player].builder = mag <= (TANKRADIUS - BUILDERRADIUS) ? make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5) : add2f(client.players[player].tank, mul2f(diff, (TANKRADIUS - BUILDERRADIUS)/mag));            
          client.players[player].builderstatus = kBuilderGoto;
          client.buildermines = 1;
          client.mines -= 1;
          client.builderpill = NOPILL;

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
        else {
          client.nextbuildertarget = makepoint(0, 0);

          if (client.printmessage) {
            client.printmessage(MSGGAME, "You need more mines.");
          }
        }

        break;

      default:
        break;
      }
    }

    break;

  case kBuilderGoto:
    diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].builder);

    if (mag2f(diff) < 0.00001) {
      if (player == client.player) {
        switch (client.buildertask) {
        case kBuilderGetTree:
          if (sendclgrabtrees(client.players[player].buildertarget.x, client.players[player].buildertarget.y) == -1) LOGFAIL(errno)
          client.players[player].builderstatus = kBuilderWork;
          break;

        case kBuilderBuildRoad:
          if (!tankonaboattest(client.players[player].buildertarget.x, client.players[player].buildertarget.y)) {
            if (sendclbuildroad(client.players[player].buildertarget.x, client.players[player].buildertarget.y, client.buildertrees) == -1) LOGFAIL(errno)
            client.players[player].builderstatus = kBuilderWork;
          }
          else {
            client.players[player].builderstatus = kBuilderWait;
          }

          break;

        case kBuilderBuildWall:
          if (!tanktest(client.players[player].buildertarget.x, client.players[player].buildertarget.y)) {
            if (sendclbuildwall(client.players[player].buildertarget.x, client.players[player].buildertarget.y, client.buildertrees) == -1) LOGFAIL(errno)
            client.players[player].builderstatus = kBuilderWork;
          }
          else {
            client.players[player].builderstatus = kBuilderWait;
          }

          break;

        case kBuilderBuildBoat:
          if (!tanktest(client.players[player].buildertarget.x, client.players[player].buildertarget.y)) {
            if (sendclbuildboat(client.players[player].buildertarget.x, client.players[player].buildertarget.y, client.buildertrees) == -1) LOGFAIL(errno)
            client.players[player].builderstatus = kBuilderWork;
          }
          else {
            client.players[player].builderstatus = kBuilderWait;
          }

          break;

        case kBuilderBuildPill:
          if (!tanktest(client.players[player].buildertarget.x, client.players[player].buildertarget.y)) {
            if (sendclbuildpill(client.players[player].buildertarget.x, client.players[player].buildertarget.y, client.buildertrees, client.builderpill) == -1) LOGFAIL(errno)
            client.players[player].builderstatus = kBuilderWork;
          }
          else {
            client.players[player].builderstatus = kBuilderWait;
          }

          break;

        case kBuilderRepairPill:
          if (!tanktest(client.players[player].buildertarget.x, client.players[player].buildertarget.y)) {
            if (sendclrepairpill(client.players[player].buildertarget.x, client.players[player].buildertarget.y, client.buildertrees) == -1) LOGFAIL(errno)
            client.players[player].builderstatus = kBuilderWork;
          }
          else {
            client.players[player].builderstatus = kBuilderWait;
          }

          break;

        case kBuilderPlaceMine:
          if (sendclplacemine(client.players[player].buildertarget.x, client.players[player].buildertarget.y) == -1) LOGFAIL(errno)
          client.players[player].builderstatus = kBuilderWork;
          break;

        default:
          assert(0);
          break;
        }
      }
    }
    else {
      float speed;

      if (isequalpoint(makepoint(client.players[player].builder.x, client.players[player].builder.y), client.players[player].buildertarget)) {
        speed = buildertargetspeed(client.players[player].builder.x, client.players[player].builder.y);
      }
      else {
        if (client.players[player].boat && mag2f(sub2f(client.players[player].builder, client.players[player].tank)) < TANKRADIUS + BUILDERRADIUS) {
          speed = BUILDERMAXSPEED;
        }
        else {
          speed = builderspeed(client.players[player].builder.x, client.players[player].builder.y, player);
        }
      }

      if (mag2f(diff) > speed/TICKSPERSEC) {
        diff = mul2f(diff, speed/(TICKSPERSEC*mag2f(diff)));

        target = client.players[player].buildertarget;
        buildertask = client.buildertask;
        collisionowner = player;
        diff = sub2f(collisiondetect(add2f(client.players[player].builder, diff), BUILDERRADIUS, buildercollision), client.players[player].builder);

        if (mag2f(diff) <= 0.00128*speed) {
          client.players[player].builderstatus = kBuilderReturn;
        }
        else {
          client.players[player].builder = collisiondetect(add2f(client.players[player].builder, mul2f(diff, speed/(TICKSPERSEC*mag2f(diff)))), BUILDERRADIUS, buildercollision);
        }
      }
      else {
        client.players[player].builder = make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5);
      }
    }

    break;

  case kBuilderWork:
    break;

  case kBuilderWait:
    if (client.players[player].builderwait++ > BUILDERBUILDTIME) {
      client.players[player].builderstatus = kBuilderReturn;
    }

    break;

  case kBuilderReturn:
    if (!client.players[player].dead) {
      float speed;
      int col;

      if (
        mag2f(sub2f(client.players[player].builder, client.players[player].tank)) <= 1.5*(TANKRADIUS + BUILDERRADIUS)
      ) {
        speed = BUILDERMAXSPEED;
        col = 0;
      }
      else {
        if (isequalpoint(makepoint(client.players[player].builder.x, client.players[player].builder.y), client.players[player].buildertarget)) {
          speed = buildertargetspeed(client.players[player].builder.x, client.players[player].builder.y);
        }
        else {
          speed = builderspeed(client.players[player].builder.x, client.players[player].builder.y, player);
        }
        col = 1;
      }

      diff = sub2f(client.players[player].tank, client.players[player].builder);

      /* builder enters tank */
      if (mag2f(diff) <= TANKRADIUS - BUILDERRADIUS) {
        client.players[player].builderstatus = kBuilderReady;
        client.players[player].buildertarget = makepoint(0, 0);

        if (player == client.player) {
          client.buildertask = kBuilderDoNothing;
          client.mines += client.buildermines;
          client.trees += client.buildertrees;
          client.buildermines = 0;
          client.buildertrees = 0;
          client.builderpill = NOPILL;

          if (client.mines > MAXMINES) {
            client.mines = MAXMINES;
          }

          if (client.trees > MAXTREES) {
            client.trees = MAXTREES;
          }

          if (client.settankstatus) {
            client.settankstatus();
          }
        }
      }
      else {
        diff = mul2f(diff, speed/(TICKSPERSEC*mag2f(diff)));

        if (col) {
          target = client.players[player].buildertarget;
          buildertask = client.buildertask;
          collisionowner = player;
          diff = sub2f(collisiondetect(add2f(client.players[player].builder, diff), BUILDERRADIUS, buildercollision), client.players[player].builder);

          if (mag2f(diff) >= 0.00001) {
            client.players[player].builder = collisiondetect(add2f(client.players[player].builder, mul2f(diff, speed/(TICKSPERSEC*mag2f(diff)))), BUILDERRADIUS, buildercollision);
          }
        }
        else {
          client.players[player].builder = add2f(client.players[player].builder, diff);
        }

        if (player == client.player && !circlesquare(client.players[player].builder, BUILDERRADIUS, client.players[player].buildertarget)) {
          client.buildertask = kBuilderDoNothing;
        }
      }
    }

    break;

  case kBuilderParachute:
    diff = sub2f(make2f(client.players[player].buildertarget.x + 0.5, client.players[player].buildertarget.y + 0.5), client.players[player].builder);

    if (mag2f(diff) < 0.001) {
      client.players[player].buildertarget = makepoint(0, 0);
      client.players[player].builderstatus = kBuilderReturn;
    }
    else {
      if (mag2f(diff) > PARACHUTESPEED/TICKSPERSEC) {
        diff = mul2f(diff, PARACHUTESPEED/(TICKSPERSEC*mag2f(diff)));
      }

      client.players[player].builder = add2f(client.players[player].builder, diff);
    }

    break;

  default:
    break;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int pilllogic(Vec2f old) {
TRY
  if (client.players[client.player].dead) {
    SUCCESS;
  }

    int i;

    for (i = 0; i < client.npills; i++) {
      if (
        !client.players[client.player].dead &&
        client.pills[i].armour != ONBOARD &&
        client.pills[i].armour > 0 &&
        (client.pills[i].owner == NEUTRAL || !testalliance(client.pills[i].owner, client.player))
      ) {
        Vec2f pill, diff;
        float mag;

        pill = make2f(client.pills[i].x + 0.5, client.pills[i].y + 0.5);
        diff = sub2f(client.players[client.player].tank, pill);
        mag = mag2f(diff);

        if ((mag <= 2.0 || forestvis(client.players[client.player].tank) > 0.25) && mag <= 8.0) {
          int j;

          for (j = 0; j < MAXPLAYERS; j++) {
            if (
              j != client.player &&
              client.players[j].connected &&
              !client.players[j].dead &&
              (client.pills[i].owner == NEUTRAL || !testalliance(client.pills[i].owner, j)) &&
              mag2f(sub2f(client.players[j].tank, pill)) < mag &&
              (mag2f(sub2f(client.players[j].tank, pill)) <= 2.0 || forestvis(client.players[j].tank) > 0.25)
            ) {
              break;
            }
          }

          if (!(j < MAXPLAYERS)) {
            client.pills[i].counter++;

            if (client.pills[i].counter >= client.pills[i].speed) {
              struct Shell *shell;
              Vec2f vel, compi, compj;
              int col;

              vel = mul2f(sub2f(client.players[client.player].tank, old), TICKSPERSEC);
              compi = sub2f(vel, prj2f(diff, vel));
              compj = mul2f(unit2f(diff), sqrtf(fabsf((SHELLVEL*SHELLVEL) - dot2f(compi, compi))));  /* fabsf is a cludge */

              if ((shell = (struct Shell *)malloc(sizeof(struct Shell))) == NULL) LOGFAIL(errno)
              shell->owner = client.pills[i].owner;
              shell->point = add2f(pill, mul2f(diff, 0.70711219/mag));
              shell->boat = 0;
              shell->pill = 1;
              shell->dir = vec2dir(add2f(compi, compj));

              shell->range = 8.5 - 0.70711219;

              if ((col = shellcollisiontest(shell, client.player)) == -1) LOGFAIL(errno)

              if (col) {
                free(shell);
              }
              else {
                addlist(&client.players[client.player].shells, shell);
              }

              client.pills[i].counter = 0;

              if (client.playsound) {
                client.playsound(kPillShotSound);
              }

              client.pillshotsound = 1;
            }
          }
          else {
            client.pills[i].counter = 0;
          }
        }
      }
      else {
        client.pills[i].counter = 0;
      }
    }

CLEANUP
ERRHANDLER(0, -1)
END
}

int shellcollisiontest(struct Shell *shell, int player) {
  Pointi p;
  int pill, base, ret;
  struct Explosion *explosion = NULL;

  assert(shell != NULL);
  assert(shell->point.x >= 0.0);
  assert(shell->point.x < FWIDTH);
  assert(shell->point.y >= 0.0);
  assert(shell->point.y < FWIDTH);

TRY
  p = makepoint(shell->point.x, shell->point.y);

  if ((pill = findpill(p.x, p.y)) != -1) {
    if (client.pills[pill].armour > 0) {
      if (player == client.player) {
        sendcldamage(p.x, p.y, shell->boat);
        if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
        explosion->point.x = p.x + 0.5;
        explosion->point.y = p.y + 0.5;
        explosion->counter = 0;
        addlist(&client.explosions, explosion);
        explosion = NULL;
        if (killsquarebuilder(p)) LOGFAIL(errno)
      }

      ret = 1;
    }
    else {
      ret = 0;
    }

    SUCCESS
  }

  if ((base = findbase(p.x, p.y)) != -1) {
    if (!shell->pill) {
      if (shell->boat) {
        if (player == client.player) {
          if (
            client.bases[base].owner != NEUTRAL && shell->owner != NEUTRAL && client.bases[base].armour >= MINBASEARMOUR &&
            !testalliance(client.bases[base].owner, shell->owner)
          ) {
            sendcldamage(p.x, p.y, shell->boat);
            if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
            explosion->point.x = p.x + 0.5;
            explosion->point.y = p.y + 0.5;
            explosion->counter = 0;
            addlist(&client.explosions, explosion);
            explosion = NULL;
            if (killsquarebuilder(p)) LOGFAIL(errno)
          }
          else {
            if ((explosion = (void *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
            explosion->point = shell->point;
            explosion->counter = 0;
            addlist(&client.players[client.player].explosions, explosion);
            explosion = NULL;
            if (killpointbuilder(shell->point)) LOGFAIL(errno)
          }
        }

        ret = 1;
      }
      else {
        if (
          client.bases[base].owner != NEUTRAL && shell->owner != NEUTRAL && client.bases[base].armour >= MINBASEARMOUR &&
          !testalliance(client.bases[base].owner, shell->owner)
        ) {
          if (player == client.player) {
            sendcldamage(p.x, p.y, shell->boat);
            if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
            explosion->point.x = p.x + 0.5;
            explosion->point.y = p.y + 0.5;
            explosion->counter = 0;
            addlist(&client.explosions, explosion);
            explosion = NULL;
            if (killsquarebuilder(p)) LOGFAIL(errno)
          }

          ret = 1;
        }
        else {
          ret = 0;
        }
      }
    }
    else {
      ret = 0;
    }

    SUCCESS
  }

  if (shell->boat) {
    switch (client.terrain[p.y][p.x]) {
    case kSeaTerrain:  /* sea */
    case kRiverTerrain:  /* river */
    case kMinedSeaTerrain:  /* mined sea */
    case kCraterTerrain:  /* crater */
      ret = 0;
      break;

    case kWallTerrain:  /* wall */
    case kSwampTerrain0:  /* swamp */
    case kSwampTerrain1:  /* swamp */
    case kSwampTerrain2:  /* swamp */
    case kSwampTerrain3:  /* swamp */
    case kForestTerrain:  /* forest */
    case kRubbleTerrain0:  /* rubble */
    case kRubbleTerrain1:  /* rubble */
    case kRubbleTerrain2:  /* rubble */
    case kRubbleTerrain3:  /* rubble */
    case kGrassTerrain0:  /* grass */
    case kGrassTerrain1:  /* grass */
    case kGrassTerrain2:  /* grass */
    case kGrassTerrain3:  /* grass */
    case kDamagedWallTerrain0:  /* damaged wall */
    case kDamagedWallTerrain1:  /* damaged wall */
    case kDamagedWallTerrain2:  /* damaged wall */
    case kDamagedWallTerrain3:  /* damaged wall */
    case kBoatTerrain:  /* river w/boat */
    case kMinedSwampTerrain:  /* mined swamp */
    case kMinedCraterTerrain:  /* mined crater */
    case kMinedRoadTerrain:  /* mined road */
    case kMinedForestTerrain:  /* mined forest */
    case kMinedRubbleTerrain:  /* mined rubble */
    case kMinedGrassTerrain:  /* mined grass */
      if (player == client.player) {
        sendcldamage(p.x, p.y, shell->boat);
        if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
        explosion->point.x = p.x + 0.5;
        explosion->point.y = p.y + 0.5;
        explosion->counter = 0;
        addlist(&client.explosions, explosion);
        explosion = NULL;
        if (killsquarebuilder(p)) LOGFAIL(errno)
      }

      ret = 1;
      break;

    case kRoadTerrain:  /* road */
      if (player == client.player) {
        if (
          (isWaterLikeTerrain(client.terrain[p.y][p.x - 1]) && isWaterLikeTerrain(client.terrain[p.y][p.x + 1])) ||
          (isWaterLikeTerrain(client.terrain[p.y - 1][p.x]) && isWaterLikeTerrain(client.terrain[p.y + 1][p.x]))
        ) {
          sendcldamage(p.x, p.y, shell->boat);
          if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
          explosion->point.x = p.x + 0.5;
          explosion->point.y = p.y + 0.5;
          explosion->counter = 0;
          addlist(&client.explosions, explosion);
          explosion = NULL;
          if (killsquarebuilder(p)) LOGFAIL(errno)
        }
        else {
          if ((explosion = (void *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
          explosion->point = shell->point;
          explosion->counter = 0;
          addlist(&client.players[client.player].explosions, explosion);
          explosion = NULL;
          if (killpointbuilder(shell->point)) LOGFAIL(errno)
        }
      }

      ret = 1;
      break;

    default:
      ret = 0;
      break;
    }
  }
  else {
    switch (client.terrain[p.y][p.x]) {
    case kSeaTerrain:  /* sea */
    case kRiverTerrain:  /* river */
    case kMinedSeaTerrain:  /* mined sea */
    case kSwampTerrain0:  /* swamp */
    case kSwampTerrain1:  /* swamp */
    case kSwampTerrain2:  /* swamp */
    case kSwampTerrain3:  /* swamp */
    case kCraterTerrain:  /* crater */
    case kRoadTerrain:  /* road */
    case kRubbleTerrain0:  /* rubble */
    case kRubbleTerrain1:  /* rubble */
    case kRubbleTerrain2:  /* rubble */
    case kRubbleTerrain3:  /* rubble */
    case kGrassTerrain0:  /* grass */
    case kGrassTerrain1:  /* grass */
    case kGrassTerrain2:  /* grass */
    case kGrassTerrain3:  /* grass */
    case kMinedSwampTerrain:  /* mined swamp */
    case kMinedCraterTerrain:  /* mined crater */
    case kMinedRoadTerrain:  /* mined road */
    case kMinedRubbleTerrain:  /* mined rubble */
    case kMinedGrassTerrain:  /* mined grass */
      ret = 0;
      break;

    case kWallTerrain:  /* wall */
    case kForestTerrain:  /* forest */
    case kDamagedWallTerrain0:  /* damaged wall */
    case kDamagedWallTerrain1:  /* damaged wall */
    case kDamagedWallTerrain2:  /* damaged wall */
    case kDamagedWallTerrain3:  /* damaged wall */
    case kBoatTerrain:  /* river w/boat */
    case kMinedForestTerrain:  /* mined forest */
      if (player == client.player) {
        sendcldamage(p.x, p.y, shell->boat);
        if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
        explosion->point.x = p.x + 0.5;
        explosion->point.y = p.y + 0.5;
        explosion->counter = 0;
        addlist(&client.explosions, explosion);
        explosion = NULL;
        if (killsquarebuilder(p)) LOGFAIL(errno)
      }

      ret = 1;
      break;

    default:
      ret = 0;
      break;
    }
  }

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(ret)
//    RETURN(0)  /* use with caution */

  default:
    if (explosion) {
      free(explosion);
    }

    RETERR(-1)
  }
END
}

int shelllogic(int player) {
  struct ListNode *node;
  struct Shell *shell;
  struct Explosion *explosion = NULL;

TRY
  /* move shells */
  if (client.players[player].connected) {
    node = nextlist(&client.players[player].shells);

    while (node != NULL) {
      shell = ptrlist(node);

      if (shell->range < SHELLVEL/TICKSPERSEC) {
        shell->point = add2f(shell->point, mul2f(dir2vec(shell->dir), shell->range));
        shell->range = 0.0;
      }
      else {
        shell->point = add2f(shell->point, mul2f(dir2vec(shell->dir), SHELLVEL/TICKSPERSEC));
        shell->range -= SHELLVEL/TICKSPERSEC;
      }

      node = nextlist(node);
    }

    /* test for collisions with pills, bases and terrain */
    node = nextlist(&client.players[player].shells);

    while (node != NULL) {
      int col;

      shell = ptrlist(node);

      if ((col = shellcollisiontest(shell, player)) == -1) LOGFAIL(errno)

      if (col) {
        node = removelist(node, free);
      }
      else {
        node = nextlist(node);
      }
    }

    /* test for collisions with tanks */
    if (!client.players[player].dead) {
      node = nextlist(&client.players[client.player].shells);

      while (node != NULL) {
        shell = ptrlist(node);

        if (mag2f(sub2f(shell->point, client.players[player].tank)) <= TANKRADIUS) {
          if ((explosion = (void *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
          explosion->point = shell->point;
          explosion->counter = 0;
          addlist(&client.players[client.player].explosions, explosion);
          explosion = NULL;
          if (killpointbuilder(shell->point)) LOGFAIL(errno)
          client.players[player].kickdir = shell->dir;
          client.players[player].kickspeed = KICKFORCE;

          if (player == client.player) {
            client.players[client.player].boat = 0;

            client.armour -= 5;

            if (client.armour < 0) {
              client.armour = 0;
              killtank();
            }

            if (client.settankstatus) {
              client.settankstatus();
            }
          }
          else {
            if (sendclhittank(player, shell->dir)) LOGFAIL(errno)
          }

          if (client.playsound) {
            client.playsound(kHitTankSound);
          }

          node = removelist(node, free);
        }
        else {
          node = nextlist(node);
        }
      }
    }

    /* explode shells */
    node = nextlist(&client.players[player].shells);

    while (node != NULL) {
      shell = ptrlist(node);

      if (shell->range <= 0.0) {        
        explosion = (void *)malloc(sizeof(struct Explosion));
        if (explosion == NULL) LOGFAIL(errno)
        explosion->point = shell->point;
        explosion->counter = 0;
        addlist(&client.players[player].explosions, explosion);
        explosion = NULL;
        if (killpointbuilder(shell->point)) LOGFAIL(errno)

        if (player == client.player) {
          sendcltouch(w2t(shell->point.x), w2t(shell->point.y));
        }

        node = removelist(node, free);
      }
      else {
        node = nextlist(node);
      }
    }
  }

CLEANUP
  if (explosion) {
    free(explosion);
  }

ERRHANDLER(0, -1)
END
}

static int explosionlogic(int player) {
  struct ListNode *node;

TRY
  if (player == -1) {
    node = nextlist(&client.explosions);

    while (node != NULL) {
      struct Explosion *explosion;

      explosion = ptrlist(node);
      explosion->counter++;

      if (explosion->counter > EXPLOSIONTICKS) {
        node = removelist(node, free);
      }
      else {
        node = nextlist(node);
      }
    }
  }
  else {
    if (client.players[player].connected) {
      node = nextlist(&client.players[player].explosions);

      while (node != NULL) {
        struct Explosion *explosion;

        explosion = ptrlist(node);
        explosion->counter++;

        if (explosion->counter > EXPLOSIONTICKS) {
          node = removelist(node, free);
        }
        else {
          node = nextlist(node);
        }
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int killtank() {
TRY
  if (!client.players[client.player].dead) {
    int j, pills;
    pills = 0;

    for (j = 0; j < client.npills; j++) {
      if (client.pills[j].owner == client.player && j != client.builderpill && client.pills[j].armour == ONBOARD) {
        pills |= 1 << j;
      }
    }

    if (sendcldroppills(client.players[client.player].tank.x, client.players[client.player].tank.y, pills)) LOGFAIL(errno)

    client.deaths++;
    client.players[client.player].dead = 1;
    client.players[client.player].boat = 0;
    client.respawncounter = 0;

    if (client.settankstatus) {
      client.settankstatus();
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int drown() {
TRY
  if (!client.players[client.player].dead || client.respawncounter <= EXPLODETICKS) {
    client.players[client.player].boat = 0;
    client.players[client.player].kickspeed = 0.0;
    client.respawncounter = EXPLODETICKS + 1;

    /* play sound */
    if (client.playsound) {
      client.playsound(kSinkSound);
    }

    client.sinksound = 1;
  }

  if (!client.players[client.player].dead) {
    int j, pills;
    pills = 0;

    for (j = 0; j < client.npills; j++) {
      if (client.pills[j].owner == client.player && j != client.builderpill && client.pills[j].armour == ONBOARD) {
        pills |= 1 << j;
      }
    }

    if (sendcldroppills(client.players[client.player].tank.x, client.players[client.player].tank.y, pills)) LOGFAIL(errno)

    client.deaths++;
    client.players[client.player].dead = 1;

    if (client.settankstatus) {
      client.settankstatus();
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int smallboom() {
TRY
  if (!client.players[client.player].dead || client.respawncounter <= EXPLODETICKS) {
    client.players[client.player].boat = 0;
    client.players[client.player].kickspeed = 0.0;
    client.respawncounter = EXPLODETICKS + 1;
    if (sendclsmallboom(w2t(client.players[client.player].tank.x), w2t(client.players[client.player].tank.y))) LOGFAIL(errno)
  }

  if (!client.players[client.player].dead) {
    int j, pills;
    pills = 0;

    for (j = 0; j < client.npills; j++) {
      if (client.pills[j].owner == client.player && j != client.builderpill && client.pills[j].armour == ONBOARD) {
        pills |= 1 << j;
      }
    }

    if (sendcldroppills(client.players[client.player].tank.x, client.players[client.player].tank.y, pills)) LOGFAIL(errno)
    client.deaths++;
    client.players[client.player].dead = 1;

    if (client.settankstatus) {
      client.settankstatus();
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int superboom() {
  int x, y;
  Vec2f point;
  struct Explosion *explosion = NULL;

TRY
  if (!client.players[client.player].dead || client.respawncounter <= EXPLODETICKS) {
    client.players[client.player].boat = 0;
    client.players[client.player].kickspeed = 0.0;
    client.respawncounter = EXPLODETICKS + 1;

    /* superboom! */
    x = (int)client.players[client.player].tank.x;

    if (client.players[client.player].tank.x - ((float)((int)client.players[client.player].tank.x)) < 0.5) {
      x--;
    }

    y = (int)client.players[client.player].tank.y;

    if (client.players[client.player].tank.y - ((float)((int)client.players[client.player].tank.y)) < 0.5) {
      y = ((int)client.players[client.player].tank.y) - 1;
    }

    if (sendclsuperboom(x, y)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = x + 0.5;
    explosion->point.y = y + 0.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(x, y))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = x + 1.5;
    explosion->point.y = y + 0.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(x + 1, y))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = x + 0.5;
    explosion->point.y = y + 1.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(x, y + 1))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    explosion->point.x = x + 1.5;
    explosion->point.y = y + 1.5;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killsquarebuilder(makepoint(x + 1, y + 1))) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = x + 0.25;
    point.y = y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = x + 1.0;
    point.y = y + 0.25;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = x + 1.75;
    point.y = y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = x + 1.0;
    point.y = y + 1.75;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
    point.x = x + 1.0;
    point.y = y + 1.0;
    explosion->point = point;
    explosion->counter = 0;
    if (addlist(&client.explosions, explosion)) LOGFAIL(errno)
    explosion = NULL;
    if (killpointbuilder(point)) LOGFAIL(errno)

    /* play sound */
    if (client.playsound) {
      if (client.fog[y][x] > 0) {
        client.playsound(kSuperBoomSound);
      }
      else {
        client.playsound(kFarSuperBoomSound);
      }
    }
  }

  if (!client.players[client.player].dead) {
    int j, pills;
    pills = 0;

    for (j = 0; j < client.npills; j++) {
      if (client.pills[j].owner == client.player && j != client.builderpill && client.pills[j].armour == ONBOARD) {
        pills |= 1 << j;
      }
    }

    if (sendcldroppills(client.players[client.player].tank.x, client.players[client.player].tank.y, pills)) LOGFAIL(errno)
    client.deaths++;
    client.players[client.player].dead = 1;
  }

CLEANUP
  if (explosion) {
    free(explosion);
  }

ERRHANDLER(0, -1)
END
}

int enter(Pointi new, Pointi old) {
  int pill, base;

  assert(new.x >= 0 && new.x < WIDTH && new.y >= 0 && new.y < WIDTH && old.x >= 0 && old.x < WIDTH && old.y >= 0 && old.y < WIDTH);

TRY
//  if (!(new.x >= 0 && new.x < WIDTH && new.y >= 0 && new.y < WIDTH && old.x >= 0 && old.x < WIDTH && old.y >= 0 && old.y < WIDTH)) {
//    SUCCESS
//  }

  if ((pill = findpill(new.x, new.y)) != -1) {
    if (client.pills[pill].armour > 0) {
      if (superboom()) LOGFAIL(errno)
    }
    else {
      if (!client.players[client.player].dead) {
        if (!isequalpoint(new, old)) {
          if (sendclgrabtile(new.x, new.y)) LOGFAIL(errno)
        }

        switch (client.terrain[new.y][new.x]) {
        case kSwampTerrain0:  /* swamp */
        case kSwampTerrain1:  /* swamp */
        case kSwampTerrain2:  /* swamp */
        case kSwampTerrain3:  /* swamp */
        case kCraterTerrain:  /* crater */
        case kRoadTerrain:  /* road */
        case kForestTerrain:  /* forest */
        case kRubbleTerrain0:  /* rubble */
        case kRubbleTerrain1:  /* rubble */
        case kRubbleTerrain2:  /* rubble */
        case kRubbleTerrain3:  /* rubble */
        case kGrassTerrain0:  /* grass */
        case kGrassTerrain1:  /* grass */
        case kGrassTerrain2:  /* grass */
        case kGrassTerrain3:  /* grass */
          /* drop boat */
          if (client.players[client.player].boat && !isequalpoint(new, old)) {
            client.players[client.player].boat = 0;
            sendcldropboat(old.x, old.y);
          }

          break;

        default:
          break;
        }
      }
    }

    SUCCESS
  }

  if ((base = findbase(new.x, new.y)) != -1) {
    if (!client.players[client.player].dead) {  /* if not dead */
      if (!isequalpoint(new, old)) {  /* if entered square */
        if /* if base is neutral or not in alliance */
          (
            client.bases[base].owner == NEUTRAL ||
            !testalliance(client.bases[base].owner, client.player)
          ) {
          if (sendclgrabtile(new.x, new.y)) LOGFAIL(errno)
        }

        if (client.players[client.player].boat) {  /* if have boat */
          client.players[client.player].boat = 0;
          sendcldropboat(old.x, old.y);
        }
      }
    }

    SUCCESS
  }

  switch (client.terrain[new.y][new.x]) {
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
    if (superboom()) LOGFAIL(errno)
    SUCCESS

  case kSeaTerrain:  /* sea */
    if (!client.players[client.player].boat) {
      drown();
    }

    SUCCESS

  case kRiverTerrain:  /* river */
    SUCCESS

  case kForestTerrain:  /* forest */
    if (client.players[client.player].dead && client.respawncounter < EXPLODETICKS && !isequalpoint(new, old)) {
      struct Explosion *explosion;
      if (sendcldamage(new.x, new.y, 0)) LOGFAIL(errno)
      if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
      explosion->point.x = new.x + 0.5;
      explosion->point.y = new.y + 0.5;
      explosion->counter = 0;
      addlist(&client.explosions, explosion);
      if (killsquarebuilder(new)) LOGFAIL(errno)
    }

  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRoadTerrain:  /* road */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
    /* drop boat */
    if (!client.players[client.player].dead && client.players[client.player].boat && !isequalpoint(new, old)) {
      client.players[client.player].boat = 0;
      sendcldropboat(old.x, old.y);
    }
  
    /* plant mines */
    if (!client.players[client.player].dead && client.players[client.player].inputflags & LMINEMASK && client.mines > 0 && !isequalpoint(new, old)) {
      client.mines--;
      if (sendcldropmine(new.x, new.y) == -1) LOGFAIL(errno)
    }

    SUCCESS

  case kBoatTerrain:  /* river w/boat */
    if (!isequalpoint(new, old)) {
      if (client.players[client.player].boat) {
        struct Explosion *explosion;
        if (sendcldamage(new.x, new.y, 0)) LOGFAIL(errno)
        if ((explosion = (struct Explosion *)malloc(sizeof(struct Explosion))) == NULL) LOGFAIL(errno)
        explosion->point.x = new.x + 0.5;
        explosion->point.y = new.y + 0.5;
        explosion->counter = 0;
        addlist(&client.explosions, explosion);
        if (killsquarebuilder(new)) LOGFAIL(errno)
      }
      else {
        if (sendclgrabtile(new.x, new.y)) LOGFAIL(errno)
      }
    }

    SUCCESS

  case kMinedSeaTerrain:  /* mined sea */
    if (!isequalpoint(new, old)) {
      if (sendclgrabtile(new.x, new.y)) LOGFAIL(errno)
    }

    drown();
    SUCCESS

  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRoadTerrain:  /* mined road */
  case kMinedForestTerrain:  /* mined forest */
  case kMinedRubbleTerrain:  /* mined rubble */
  case kMinedGrassTerrain:  /* mined grass */
    if (!isequalpoint(new, old)) {
      if (sendclgrabtile(new.x, new.y)) LOGFAIL(errno)
    }

    SUCCESS

  default:
    assert(0);
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int spawn() {
  int start, i, j;
  int weights[MAX_STARTS], range, index;

TRY
  for (i = 0; i < client.nstarts; i++) {
    weights[i] = 1;

    for (j = 0; j < client.nbases; j++) {
      if (client.bases[j].owner == NEUTRAL || testalliance(client.bases[j].owner, client.player)) {
        float dist = mag2f(sub2f(make2f(client.starts[i].x + 0.5, client.starts[i].y + 0.5), make2f(client.bases[j].x + 0.5, client.bases[j].y + 0.5)));

        if (dist < 8.5) {
          if (weights[i] < 3) {
            weights[i] = 3;
          }
        }
        else if (dist < 17) {
          if (weights[i] < 2) {
            weights[i] = 2;
          }
        }
      }
    }

    for (j = 0; j < client.npills; j++) {
      if (!testalliance(client.pills[j].owner, client.player)) {
        if (mag2f(sub2f(make2f(client.starts[i].x + 0.5, client.starts[i].y + 0.5), make2f(client.pills[j].x + 0.5, client.pills[j].y + 0.5))) < 8.5) {
          weights[i] = 0;
        }
      }
    }
  }

  range = 0;

  for (i = 0; i < client.nstarts; i++) {
    range += weights[i];
  }

  /* all starting locations are spiked need to reassess */
  if (range == 0) {
    for (i = 0; i < client.nstarts; i++) {
      weights[i] = 1;

      for (j = 0; j < client.nbases; j++) {
        if (client.bases[j].owner == NEUTRAL || testalliance(client.bases[j].owner, client.player)) {
          float dist = mag2f(sub2f(make2f(client.starts[i].x + 0.5, client.starts[i].y + 0.5), make2f(client.bases[j].x + 0.5, client.bases[j].y + 0.5)));

          if (dist < 8.5) {
            if (weights[i] < 3) {
              weights[i] = 3;
            }
          }
          else if (dist < 17) {
            if (weights[i] < 2) {
              weights[i] = 2;
            }
          }
        }
      }
    }

    range = 0;

    for (i = 0; i < client.nstarts; i++) {
      range += weights[i];
    }
  }

  index = random()%range;
  range = 0;

  for (i = 0; i < client.nstarts; i++) {
    range += weights[i];

    if (range > index) {
      break;
    }
  }

  start = i;

  client.players[client.player].dead = 0;
  client.players[client.player].tank.x = client.starts[start].x + 0.5;
  client.players[client.player].tank.y = client.starts[start].y + 0.5;
  client.players[client.player].dir = client.starts[start].dir*(kPif/8.0);
  client.players[client.player].speed = 0.0;
  client.players[client.player].turnspeed = 0.0;
  client.players[client.player].kickspeed = 0.0;
  client.players[client.player].kickdir = 0.0;
  client.range = MAXRANGE;
  client.players[client.player].boat = 1;

  if (client.gametype == kDominationGameType) {
    if (client.game.domination.type == kOpenGame) {
      client.shells = MAXSHELLS;
      client.mines = MAXMINES;
      client.armour = MAXARMOUR;
      client.trees = MAXTREES;
    }
    else if (client.game.domination.type == kTournamentGame) {
      client.shells = 0;

      for (i = 0; i < client.nbases; i++) {
        if (client.bases[i].owner == NEUTRAL) {
          client.shells++;
        }
      }

      client.shells *= 2;
      client.mines = 0;
      client.armour = MAXARMOUR;
      client.trees = 0;
    }
    else if (client.game.domination.type == kStrictGame) {
      client.shells = 0;
      client.mines = 0;
      client.armour = MAXARMOUR;
      client.trees = 0;
    }
    else {
      assert(0);
    }
  }
  else {
    assert(0);
  }

  client.spawned = 1;

  if (client.settankstatus) {
    client.settankstatus();
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int tilefor(int x, int y) {
  int i;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  for (i = 0; i < client.npills; i++) {
    if (client.pills[i].armour != ONBOARD && client.pills[i].x == x && client.pills[i].y == y) {
      if (client.pills[i].owner != NEUTRAL && testalliance(client.pills[i].owner, client.player)) {
        return kFriendlyPill00Tile + client.pills[i].armour;
      }
      else {
        return kHostilePill00Tile + client.pills[i].armour;
      }
    }
  }

  for (i = 0; i < client.nbases; i++) {
    if (client.bases[i].x == x && client.bases[i].y == y) {
      if (client.bases[i].owner == NEUTRAL) {
        return kNeutralBaseTile;
      }
      else if (testalliance(client.bases[i].owner, client.player)) {
        return kFriendlyBaseTile;
      }
      else {
        return kHostileBaseTile;
      }
    }
  }

  return terraintotile(client.terrain[y][x]);
  
}

int fogtilefor(int x, int y, int tile) {
  int pill, base;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  if ((pill = findpill(x, y)) != -1) {
    if (client.pills[pill].owner != NEUTRAL && testalliance(client.pills[pill].owner, client.player)) {
      return kFriendlyPill00Tile + client.pills[pill].armour;
    }
    else {
      return kHostilePill00Tile + client.pills[pill].armour;
    }
  }

  if ((base = findbase(x, y)) != -1) {
    if (client.bases[base].owner == NEUTRAL) {
      return kNeutralBaseTile;
    }
    else if (testalliance(client.bases[base].owner, client.player)) {
      return kFriendlyBaseTile;
    }
    else {
      return kHostileBaseTile;
    }
  }

  switch (client.terrain[y][x]) {
  case kSeaTerrain:  /* sea */
    return kSeaTile;

  case kMinedSeaTerrain:  /* mined sea */
    return kMinedSeaTile;

  case kWallTerrain:  /* wall */
    return kWallTile;

  case kRiverTerrain:  /* river */
    return kRiverTile;

  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
    return kSwampTile;

  case kMinedSwampTerrain:  /* mined swamp */
    if (client.hiddenmines && tile != kMinedSwampTile) {
      return kSwampTile;
    }
    else {
      return kMinedSwampTile;
    }

  case kCraterTerrain:  /* crater */
    return kCraterTile;

  case kMinedCraterTerrain:  /* mined crater */
    if (client.hiddenmines && tile != kMinedCraterTile) {
      return kCraterTile;
    }
    else {
      return kMinedCraterTile;
    }

  case kRoadTerrain:  /* road */
    return kRoadTile;

  case kMinedRoadTerrain:  /* mined road */
    if (client.hiddenmines && tile != kMinedRoadTile) {
      return kRoadTile;
    }
    else {
      return kMinedRoadTile;
    }

  case kForestTerrain:  /* forest */
    return kForestTile;

  case kMinedForestTerrain:  /* mined forest */
    if (client.hiddenmines && tile != kMinedForestTile && tile != kMinedGrassTile) {
      return kForestTile;
    }
    else {
      return kMinedForestTile;
    }

  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
    return kRubbleTile;

  case kMinedRubbleTerrain:  /* mined rubble */
    if (client.hiddenmines && tile != kMinedRubbleTile) {
      return kRubbleTile;
    }
    else {
      return kMinedRubbleTile;
    }

  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
    return kGrassTile;

  case kMinedGrassTerrain:  /* mined grass */
    if (client.hiddenmines && tile != kMinedGrassTile && tile != kMinedForestTile) {
      return kGrassTile;
    }
    else {
      return kMinedGrassTile;
    }

  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
    return kDamagedWallTile;

  case kBoatTerrain:  /* river w/boat */
    return kBoatTile;

  default:
    assert(0);
    return -1;
  }
}

int refresh(int x, int y) {
  int i, j;
  Recti rect;
  int image;
  Pointi *p;
  int seentile;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  if (client.fog[y - 1][x - 1] > 0 || client.fog[y - 1][x    ] > 0 || client.fog[y - 1][x + 1] > 0 ||
      client.fog[y    ][x - 1] > 0 || client.fog[y    ][x    ] > 0 || client.fog[y    ][x + 1] > 0 ||
      client.fog[y + 1][x - 1] > 0 || client.fog[y + 1][x    ] > 0 || client.fog[y + 1][x + 1] > 0) {
    seentile = tilefor(x, y);

    if (seentile != client.seentiles[y][x]) {
      client.seentiles[y][x] = seentile;
      rect = intersectionrect(makerect(x - 1, y - 1, 3, 3), makerect(0, 0, WIDTH, WIDTH));

      for (i = minxrect(rect); i <= maxxrect(rect); i++) {
        for (j = minyrect(rect); j <= maxyrect(rect); j++) {
          if (((image = mapimage(client.seentiles, i, j)) != client.images[j][i]) || (i == x && j == y)) {
            client.images[j][i] = image;
            if ((p = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
            p->x = i;
            p->y = j;
            if (addlist(&client.changedtiles, p) == -1) LOGFAIL(errno)
          }
        }
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

void clearchangedtiles() {
  clearlist(&client.changedtiles, free);
}

int requestalliance(uint16_t withplayers) {
  struct CLSetAlliance clsetalliance;
  int i, j;
  char *text = NULL;
  uint16_t xor;

TRY
  xor = client.players[client.player].alliance ^ (client.players[client.player].alliance | withplayers);
  client.players[client.player].alliance |= withplayers;
  clsetalliance.type = CLSETALLIANCE;
  clsetalliance.alliance = htons(client.players[client.player].alliance);
  if (writebuf(&client.sendbuf, &clsetalliance, sizeof(clsetalliance)) == -1) LOGFAIL(errno)
  if (sendbuf(&client.sendbuf, client.cntlsock) == -1) LOGFAIL(errno)

  /* their alliance bit has changed */
  for (i = 0; i < MAXPLAYERS; i++) {
    /* my alliance bit is set */
    if (client.players[i].connected && (xor & (1 << i))) {
      /* their alliance bit is set */
      if (client.players[i].alliance & (1 << client.player)) {
        if (client.printmessage) {
          if (asprintf(&text, "alliance accepted with %s", client.players[i].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
          free(text);
          text = NULL;
        }

        if (client.setplayerstatus) {
          client.setplayerstatus(i);
        }

        for (j = 0; j < client.nbases; j++) {
          if (client.bases[j].owner == i) {
            refresh(client.bases[j].x, client.bases[j].y);

            if (client.setbasestatus) {
              client.setbasestatus(j);
            }
          }
        }

        for (j = 0; j < client.npills; j++) {
          if (client.pills[j].owner == i) {
            /* fog of war */
            if (client.pills[j].armour != ONBOARD && client.pills[j].armour > 0) {
              if (increasevis(makerect(client.pills[j].x - 7, client.pills[j].y - 7, 15, 15))) LOGFAIL(errno)
            }

            refresh(client.pills[j].x, client.pills[j].y);

            if (client.setpillstatus) {
              client.setpillstatus(j);
            }
          }
        }

        if (increasevis(makerect(((int)client.players[i].tank.x) - 14, ((int)client.players[i].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
      }
      /* their alliance bit is unset */
      else {
        if (client.printmessage) {
          if (asprintf(&text, "requested alliance with %s", client.players[i].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
          free(text);
          text = NULL;
        }
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int leavealliance(uint16_t withplayers) {
  struct CLSetAlliance clsetalliance;
  int i, j;
  char *text = NULL;
  uint16_t xor;

TRY
  xor = client.players[client.player].alliance ^ (client.players[client.player].alliance & (~withplayers | (1 << client.player)));
  client.players[client.player].alliance &= ~withplayers | (1 << client.player);
  clsetalliance.type = CLSETALLIANCE;
  clsetalliance.alliance = htons(client.players[client.player].alliance);
  if (writebuf(&client.sendbuf, &clsetalliance, sizeof(clsetalliance)) == -1) LOGFAIL(errno)
  if (sendbuf(&client.sendbuf, client.cntlsock) == -1) LOGFAIL(errno)

  /* their alliance bit has changed */
  for (i = 0; i < MAXPLAYERS; i++) {
    /* my alliance bit is unset */
    if (client.players[i].connected && (xor & (1 << i))) {
      /* their alliance bit is set */
      if (client.players[i].alliance & (1 << client.player)) {
        if (client.printmessage) {
          if (asprintf(&text, "left alliance with %s", client.players[i].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
          free(text);
          text = NULL;
        }

        if (client.setplayerstatus) {
          client.setplayerstatus(i);
        }

        for (j = 0; j < client.nbases; j++) {
          if (client.bases[j].owner == i) {
            refresh(client.bases[j].x, client.bases[j].y);
            if (client.setbasestatus) {
              client.setbasestatus(j);
            }
          }
        }

        for (j = 0; j < client.npills; j++) {
          if (client.pills[j].owner == i) {
            /* fog of war */
            if (client.pills[j].armour != ONBOARD) {
              if (refresh(client.pills[j].x, client.pills[j].y)) LOGFAIL(errno)

              if (client.pills[j].armour > 0) {
                if (decreasevis(makerect(client.pills[j].x - 7, client.pills[j].y - 7, 15, 15))) LOGFAIL(errno)
              }
            }

            if (client.setpillstatus) {
              client.setpillstatus(j);
            }
          }
        }

        if (decreasevis(makerect(((int)client.players[i].tank.x) - 14, ((int)client.players[i].tank.y) - 14, 29, 29)) == -1) LOGFAIL(errno)
      }
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int keyevent(int mask, int set) {
  Pointi tanksqr;
  int gotlock = 0;

TRY
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  if (set) {
    client.players[client.player].inputflags |= mask;
  }
  else {
    client.players[client.player].inputflags &= ~mask;
  }

  tanksqr = makepoint(w2t(client.players[client.player].tank.x), w2t(client.players[client.player].tank.y));

  if (!client.players[client.player].dead) {
    int i;

    for (i = 0; i < client.npills; i++) {
      if (client.pills[i].armour != ONBOARD && client.pills[i].x == (int)client.players[client.player].tank.x && client.pills[i].y == (int)client.players[client.player].tank.y) {
        break;
      }
    }

    if (!(i < client.npills)) {
      for (i = 0; i < client.nbases; i++) {
        if (client.bases[i].x == (int)client.players[client.player].tank.x && client.bases[i].y == (int)client.players[client.player].tank.y) {
          break;
        }
      }

      if (!(i < client.nbases)) {
        switch (client.terrain[(int)client.players[client.player].tank.y][(int)client.players[client.player].tank.x]) {
        case kSwampTerrain0:
        case kSwampTerrain1:
        case kSwampTerrain2:
        case kSwampTerrain3:
        case kCraterTerrain:
        case kRoadTerrain:
        case kForestTerrain:
        case kRubbleTerrain0:
        case kRubbleTerrain1:
        case kRubbleTerrain2:
        case kRubbleTerrain3:
        case kGrassTerrain0:
        case kGrassTerrain1:
        case kGrassTerrain2:
        case kGrassTerrain3:
          /* plant first mine */
          if (set && mask == LMINEMASK && client.mines > 0) {
            client.mines--;
            if (sendcldropmine((int)client.players[client.player].tank.x, (int)client.players[client.player].tank.y) == -1) LOGFAIL(errno)
          }

        break;

        default:
          break;
        }
      }
    }
  }

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  if (gotlock) {
    unlockclient();
  }

ERRHANDLER(0, -1)
END
}

void buildercommand(int command, Pointi p) {
  if (client.players[client.player].builderstatus != kBuilderParachute && client.nextbuildercommand == BUILDERNILL && !client.players[client.player].dead) {
    client.nextbuildercommand = command;
    client.nextbuildertarget = p;
  }
}

int getbuildertaskforcommand(int command, Pointi at) {
  switch (command) {
  case BUILDERNILL:
    return kBuilderDoNothing;
    
  case BUILDERTREE:
    switch (client.seentiles[at.y][at.x]) {
    case kForestTile:
    case kMinedForestTile:
      return kBuilderGetTree;

    default:
      return kBuilderDoNothing;
    }

  case BUILDERROAD:
    switch (client.seentiles[at.y][at.x]) {
    case kForestTile:
    case kMinedForestTile:
      return kBuilderGetTree;

    case kRiverTile:
    case kSwampTile:
    case kCraterTile:
    case kRubbleTile:
    case kGrassTile:
      return kBuilderBuildRoad;

    case kMinedSwampTile:
    case kMinedCraterTile:
    case kMinedRubbleTile:
    case kMinedGrassTile:
      if (client.printmessage) {
        client.printmessage(MSGGAME, "Your builder cannot do that.  It would kill him.");
      }

      return kBuilderDoNothing;

    default:
      return kBuilderDoNothing;
    }

  case BUILDERWALL:
    switch (client.seentiles[at.y][at.x]) {
    case kForestTile:
    case kMinedForestTile:
      return kBuilderGetTree;

    case kSwampTile:
    case kCraterTile:
    case kRoadTile:
    case kRubbleTile:
    case kGrassTile:
    case kDamagedWallTile:
      return kBuilderBuildWall;

    case kRiverTile:
      return kBuilderBuildBoat;

    case kMinedSwampTile:
    case kMinedCraterTile:
    case kMinedRoadTile:
    case kMinedRubbleTile:
    case kMinedGrassTile:
      if (client.printmessage) {
        client.printmessage(MSGGAME, "Your builder cannot do that.  It would kill him.");
      }

      return kBuilderDoNothing;

    default:
      return kBuilderDoNothing;
    }

  case BUILDERPILL:
    switch (client.seentiles[at.y][at.x]) {
    case kForestTile:
    case kMinedForestTile:
      return kBuilderGetTree;

    case kSwampTile:
    case kCraterTile:
    case kRoadTile:
    case kRubbleTile:
    case kGrassTile:
      return kBuilderBuildPill;

    case kMinedSwampTile:
    case kMinedCraterTile:
    case kMinedRoadTile:
    case kMinedRubbleTile:
    case kMinedGrassTile:
      if (client.printmessage) {
        client.printmessage(MSGGAME, "Your builder cannot do that.  It would kill him.");
      }

      return kBuilderDoNothing;

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
      return kBuilderRepairPill;

    default:
      return kBuilderDoNothing;
    }

  case BUILDERMINE:
    switch (client.seentiles[at.y][at.x]) {
    case kSwampTile:
    case kCraterTile:
    case kRoadTile:
    case kForestTile:
    case kRubbleTile:
    case kGrassTile:
      return kBuilderPlaceMine;

    case kMinedSwampTile:
    case kMinedCraterTile:
    case kMinedRoadTile:
    case kMinedForestTile:
    case kMinedRubbleTile:
    case kMinedGrassTile:
      if (client.printmessage) {
        client.printmessage(MSGGAME, "Your builder cannot do that.  It would kill him.");
      }

      return kBuilderDoNothing;

    default:
      return kBuilderDoNothing;
    }

  default:
    assert(0);
  }
}

int sendmessage(const char *text, int to) {
  int gotlock = 0;
  int i;
  struct CLSendMesg clsendmesg;

TRY
  /* client lock */
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  clsendmesg.type = CLSENDMESG;
  clsendmesg.to = to;

  switch (to) {
  case MSGEVERYONE:
    clsendmesg.mask = htons(0xffff);  /* ok */
    break;

  case MSGALLIES:
    clsendmesg.mask = htons(client.players[client.player].alliance);
    break;

  case MSGNEARBY:
    clsendmesg.mask = htons(0x00);  /* ok */

    for (i = 0; i < MAXPLAYERS; i++) {
      if (mag2f(sub2f(client.players[client.player].tank, client.players[i].tank)) < 8.5) {
        clsendmesg.mask |= 1 << i;
      }
    }

    clsendmesg.mask = htons(clsendmesg.mask);
    break;

  default:
    assert(0);
    break;
  }

  if (writebuf(&client.sendbuf, &clsendmesg, sizeof(clsendmesg)) == -1) LOGFAIL(errno)
  if (writebuf(&client.sendbuf, text, strlen(text) + 1) == -1) LOGFAIL(errno)
  if (sendbuf(&client.sendbuf, client.cntlsock) == -1) LOGFAIL(errno)

  /* client unlock */
  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  if (gotlock) {
    unlockclient();
  }

ERRHANDLER(0, -1)
END
}

static int w2t(float f) {
  return f;
}

float rounddir(float dir) {
  return (kPif/8.0)*floor(dir/(kPif/8.0) + 0.5);
}

int tankcollision(Pointi square) {
  int i;

  if (square.x < 0 || square.x >= WIDTH || square.y < 0 || square.y >= WIDTH) {
    return 1;
  }

  for (i = 0; i < client.npills; i++) {
    if (client.pills[i].armour != ONBOARD && client.pills[i].x == square.x && client.pills[i].y == square.y) {
      return client.pills[i].armour > 0;
    }
  }

  for (i = 0; i < client.nbases; i++) {
    if (client.bases[i].x == square.x && client.bases[i].y == square.y) {
      return
        client.bases[i].owner != NEUTRAL &&
        !testalliance(client.bases[i].owner, collisionowner) &&
        client.bases[i].armour >= MINBASEARMOUR;
    }
  }

  switch (client.terrain[square.y][square.x]) {
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
    return 1;

  case kSeaTerrain:  /* sea */
  case kRiverTerrain:  /* river */
  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRoadTerrain:  /* road */
  case kForestTerrain:  /* forest */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kBoatTerrain:  /* river w/boat */
  case kMinedSeaTerrain:  /* mined sea */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRoadTerrain:  /* mined road */
  case kMinedForestTerrain:  /* mined forest */
  case kMinedRubbleTerrain:  /* mined rubble */
  case kMinedGrassTerrain:  /* mined grass */
    return 0;

  default:
    assert(0);
  }
}

int buildercollision(Pointi square) {
  int pill, base;

  if (square.x < 0 || square.x >= WIDTH || square.y < 0 || square.y >= WIDTH) {
    return 1;
  }

  if ((pill = findpill(square.x, square.y)) != -1) {
    return !((isequalpoint(square, target) && buildertask == kBuilderRepairPill) || (client.pills[pill].armour == 0));
  }

  if ((base = findbase(square.x, square.y)) != -1) {
    return client.bases[base].owner != NEUTRAL && !testalliance(client.bases[base].owner, collisionowner) && client.bases[base].armour > MINBASEARMOUR;
  }

  switch (client.terrain[square.y][square.x]) {
  case kWallTerrain:  /* wall */
  case kDamagedWallTerrain0:  /* damaged wall */
  case kDamagedWallTerrain1:  /* damaged wall */
  case kDamagedWallTerrain2:  /* damaged wall */
  case kDamagedWallTerrain3:  /* damaged wall */
    return !isequalpoint(square, target) || buildertask != kBuilderBuildWall;

  case kRiverTerrain:  /* river */
    return !isequalpoint(square, target) || (buildertask != kBuilderBuildBoat && buildertask != kBuilderBuildRoad);

  case kSeaTerrain:  /* sea */
  case kMinedSeaTerrain:  /* mined sea */
    return 1;

  case kSwampTerrain0:  /* swamp */
  case kSwampTerrain1:  /* swamp */
  case kSwampTerrain2:  /* swamp */
  case kSwampTerrain3:  /* swamp */
  case kCraterTerrain:  /* crater */
  case kRoadTerrain:  /* road */
  case kForestTerrain:  /* forest */
  case kRubbleTerrain0:  /* rubble */
  case kRubbleTerrain1:  /* rubble */
  case kRubbleTerrain2:  /* rubble */
  case kRubbleTerrain3:  /* rubble */
  case kGrassTerrain0:  /* grass */
  case kGrassTerrain1:  /* grass */
  case kGrassTerrain2:  /* grass */
  case kGrassTerrain3:  /* grass */
  case kBoatTerrain:  /* river w/boat */
  case kMinedSwampTerrain:  /* mined swamp */
  case kMinedCraterTerrain:  /* mined crater */
  case kMinedRoadTerrain:  /* mined road */
  case kMinedForestTerrain:  /* mined forest */
  case kMinedRubbleTerrain:  /* mined rubble */
  case kMinedGrassTerrain:  /* mined grass */
    return 0;

  default:
    assert(0);
    return 0;
  }
}

static int circlesquare(Vec2f point, float radius, Pointi square) {
  if (point.x < square.x) {
    if (point.y < square.y) {
      return mag2f(sub2f(make2f(square.x, square.y), point)) < radius;
    }
    else if (point.y > square.y + 1) {
      return mag2f(sub2f(make2f(square.x, square.y + 1), point)) < radius;
    }
    else {
      return point.x + radius > square.x;
    }
  }
  else if (point.x > square.x + 1) {
    if (point.y < square.y) {
      return mag2f(sub2f(make2f(square.x + 1, square.y), point)) < radius;
    }
    else if (point.y > square.y + 1) {
      return mag2f(sub2f(make2f(square.x + 1, square.y + 1), point)) < radius;
    }
    else {
      return point.x - radius < square.x + 1;
    }
  }
  else {
    if (point.y < square.y) {
      return point.y + radius > square.y;
    }
    else if (point.y > square.y + 1) {
      return point.y - radius < square.y + 1;
    }
    else {
      return 1;
    }
  }
}

static Vec2f collisiondetect(Vec2f p, float radius, int (*func)(Pointi square)) {
  int ix, iy;
  int lxc, hxc, lyc, hyc;
  float lx, hx, ly, hy, fx, fy;
  float sqr, sca, r2;

  ix = (int)p.x;
  iy = (int)p.y;
  fx = (float)ix;
  fy = (float)iy;
  lx = p.x - fx;
  hx = 1.0 - lx;
  ly = p.y - fy;
  hy = 1.0 - ly;
  r2 = radius*radius;

  lxc = lx < radius && func(makepoint(ix - 1, iy));
  hxc = hx < radius && func(makepoint(ix + 1, iy));
  lyc = ly < radius && func(makepoint(ix, iy - 1));
  hyc = hy < radius && func(makepoint(ix, iy + 1));

  if (lxc) {
    if (hxc) {
      p.x = fx + 0.5;
    }
    else {
      p.x = fx + radius;
    }
  }
  else if (hxc) {
    p.x = fx + (1.0 - radius);
  }

  if (lyc) {
    if (hyc) {
      p.x = fy + 0.5;
    }
    else {
      p.y = fy + radius;
    }
  }
  else if (hyc) {
    p.y = fy + (1.0 - radius);
  }

  if (!lxc && !lyc && (sqr = lx*lx + ly*ly) < r2 && func(makepoint(ix - 1, iy - 1))) {
    sca = radius/sqrtf(sqr);
    p.x = fx + sca*lx;
    p.y = fy + sca*ly;
  }

  if (!hxc && !lyc && (sqr = hx*hx + ly*ly) < r2 && func(makepoint(ix + 1, iy - 1))) {
    sca = radius/sqrtf(sqr);
    p.x = fx + (1.0 - sca*hx);
    p.y = fy + sca*ly;
  }

  if (!lxc && !hyc && (sqr = lx*lx + hy*hy) < r2 && func(makepoint(ix - 1, iy + 1))) {
    sca = radius/sqrtf(sqr);
    p.x = fx + sca*lx;
    p.y = fy + (1.0 - sca*hy);
  }

  if (!hxc && !hyc && (sqr = hx*hx + hy*hy) < r2 && func(makepoint(ix + 1, iy + 1))) {
    sca = radius/sqrtf(sqr);
    p.x = fx + (1.0 - sca*hx);
    p.y = fy + (1.0 - sca*hy);
  }

  return p;
}

int killsquarebuilder(Pointi p) {
TRY
  switch (client.players[client.player].builderstatus) {
  case kBuilderGoto:
  case kBuilderWork:
  case kBuilderWait:
  case kBuilderReturn:
    if (isequalpoint(makepoint(w2t(client.players[client.player].builder.x), w2t(client.players[client.player].builder.y)), p)) {
      killbuilder();
    }

    break;

  case kBuilderReady:
  case kBuilderParachute:
  default:
    break;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int killpointbuilder(Vec2f p) {
TRY
  switch (client.players[client.player].builderstatus) {
  case kBuilderGoto:
  case kBuilderWork:
  case kBuilderWait:
  case kBuilderReturn:
    if (mag2f(sub2f(client.players[client.player].builder, p)) < EXPLOSIONRADIUS) {
      killbuilder();
    }

    break;

  case kBuilderReady:
  case kBuilderParachute:
  default:
    break;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int killbuilder() {
  int start;

TRY
  if (client.builderpill != NOPILL) {
    /* drop pill */
    if (sendcldroppills(client.players[client.player].builder.x, client.players[client.player].builder.y, 1 << client.builderpill)) LOGFAIL(errno)
    client.builderpill = NOPILL;
  }

  start = random()%client.nstarts;
  client.players[client.player].builderstatus = kBuilderParachute;
  client.players[client.player].builder.x = client.starts[start].x + 0.5;
  client.players[client.player].builder.y = client.starts[start].y + 0.5;
  client.nextbuildercommand = BUILDERNILL;
  client.nextbuildertarget = makepoint(0, 0);
  client.buildertask = kBuilderDoNothing;
  client.buildermines = 0;
  client.buildertrees = 0;
  client.players[client.player].buildertarget = makepoint(0, 0);

  client.players[client.player].buildertarget.x = client.players[client.player].tank.x;
  client.players[client.player].buildertarget.y = client.players[client.player].tank.y;

  /* play sound */
  if (client.playsound) {
    client.playsound(kBuilderDeathSound);
  }
  if (client.printmessage) {
      char *text;
      if (asprintf(&text, "%s just lost his builder", client.players[client.player].name) == -1) LOGFAIL(errno)
          client.printmessage(MSGGAME, text);
      free(text);
  }

  client.builderdeathsound = 1;

CLEANUP
ERRHANDLER(0, -1)
END
}

int findpill(int x, int y) {
  int i;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  for (i = 0; i < client.npills; i++) {
    if (client.pills[i].armour != ONBOARD && client.pills[i].x == x && client.pills[i].y == y) {
      return i;
    }
  }

  return -1;
}

int findbase(int x, int y) {
  int i;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  for (i = 0; i < client.nbases; i++) {
    if (client.bases[i].x == x && client.bases[i].y == y) {
      return i;
    }
  }

  return -1;
}

static int testalliance(int p1, int p2) {
  return client.players[p1].used && client.players[p2].used && (client.players[p1].alliance & (1 << p2)) && (client.players[p2].alliance & (1 << p1));
}
