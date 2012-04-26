/*
 *  server.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 4/27/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "server.h"
#include "client.h"
#include "terrain.h"
#include "tiles.h"
#include "bmap_server.h"
#include "tracker.h"
#include "errchk.h"
#include "io.h"
#include "timing.h"
#include "resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <netdb.h>
#include <math.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <fcntl.h>

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

struct Server server;

//static int sendplayerbufserver(int player);

/* pthread routines */

static void *servermainthread(void *);

/* server receive routines */

static int recvclsendmesg(int player);
static int recvcldropboat(int player);
static int recvcldroppills(int player);
static int recvcldropmine(int player);
static int recvcltouch(int player);
static int recvclgrabtile(int player);
static int recvclgrabtrees(int player);
static int recvclbuildroad(int player);
static int recvclbuildwall(int player);
static int recvclbuildboat(int player);
static int recvclbuildpill(int player);
static int recvclrepairpill(int player);
static int recvclplacemine(int player);
static int recvcldamage(int player);
static int recvclsmallboom(int player);
static int recvclsuperboom(int player);
static int recvclrefuel(int player);
static int recvclhittank(int player);
static int recvclsetalliance(int player);

static int sendsrsendmesg(int player, int to, uint16_t mask, const char *text);
static int sendsrplayerjoin(int player);
static int sendsrplayerrejoin(int player);
static int sendsrplayerexit(int player);
static int sendsrplayerdisc(int player);
static int sendsrplayerkick(int player);
static int sendsrplayerban(int player);
static int sendsrdamage(int player, int x, int y);
static int sendsrgrabtrees(int x, int y);
static int sendsrbuild(int x, int y);
static int sendsrgrow(int x, int y);
static int sendsrflood(int x, int y);
static int sendsrplacemine(int player, int x, int y);
static int sendsrdropmine(int player, int x, int y);
static int sendsrdropboat(int x, int y);
static int sendsrrepairpill(int pill);
static int sendsrcoolpill(int pill);
static int sendsrcapturepill(int pill);
static int sendsrbuildpill(int pill);
static int sendsrdroppill(int pill);
static int sendsrreplenishbase(int base);
static int sendsrcapturebase(int base);
static int sendsrrefuel(int player, int base, int armour, int shells, int mines);
static int sendsrgrabboat(int player, int x, int y);
static int sendsrmineack(int player, int success);
static int sendsrbuilderack(int player, int mines, int trees, int pill);
static int sendsrsmallboom(int player, int x, int y);
static int sendsrsuperboom(int player, int x, int y);
static int sendsrhittank(int player, uint32_t dir);
static int sendsrsetalliance(int player, uint16_t alliance);
static int sendsrtimelimit(uint16_t timeremaining);
static int sendsrbasecontrol(uint16_t timeleft);
static int sendsrpause(int counter);

static int sendtoall(const void *data, size_t size);
static int sendtoallex(const void *data, size_t size, int player);
static int sendtoone(const void *data, size_t size, int player);

static void growtrees(int nplayers);

static int chain();
static int chainat(int x, int y);

static int explosionat(int player, int x, int y);
static int superboomat(int player, int x, int y);

static int floodtest(int x, int y);
static int flood();
static int floodat(int x, int y);

static int findpill(int x, int y);
static int findbase(int x, int y);
static void droppills(int player, float x, float y, uint16_t pills);

static int removeplayer(int player);
static int nplayers();
static int cleanupserver();

int initserver() {
  int err;
  int i;

TRY
  bzero(&server, sizeof(server));

  server.setup = 0;
  server.running = 0;

  /* initialize tracker data */
  server.tracker.hostname = NULL;
  server.tracker.sock = -1;
  server.tracker.callback = NULL;
  if (initbuf(&server.tracker.recvbuf)) LOGFAIL(errno)
  if (initbuf(&server.tracker.sendbuf)) LOGFAIL(errno)

  server.listensock = -1;
  server.dgramsock = -1;
  server.mainpipe[0] = -1;
  server.mainpipe[1] = -1;
  server.threadpipe[0] = -1;
  server.threadpipe[1] = -1;

  /* initialize server mutex */
  if ((err = pthread_mutex_init(&server.mutex, NULL))) LOGFAIL(err)

  /* alloc and initialize the chain detonation */
  for (i = 0; i < (CHAINTICKS + 1); i++) {
    if (initlist(server.chains + i)) LOGFAIL(errno)
  }

  /* alloc and initialize the flood fill */
  for (i = 0; i < (FLOODTICKS + 1); i++) {
    if (initlist(server.floods + i)) LOGFAIL(errno)
  }

  /* alloc and initialize joining player buffers */
  if (initbuf(&server.joiningplayer.recvbuf)) LOGFAIL(errno)
  if (initbuf(&server.joiningplayer.sendbuf)) LOGFAIL(errno)

  /* alloc and initialize player buffers */
  for (i = 0; i < MAXPLAYERS; i++) {
    server.players[i].used = 0;
    server.players[i].cntlsock = -1;
    server.players[i].seq = 0;
    if (initbuf(&server.players[i].recvbuf)) LOGFAIL(errno)
    if (initbuf(&server.players[i].sendbuf)) LOGFAIL(errno)
  }

  /* alloc and initialize banned players list */
  if (initlist(&server.bannedplayers)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int setupserver(int initiallypaused, const void *buf, size_t nbytes, uint16_t port, const char password[], int timelimit, int hiddenmines, int pauseonplayerexit, int gametype, const void *gamedata) {
  int i, j;
  struct sockaddr_in addr;
  socklen_t addrlen;
  int flags;

  server.timelimit = timelimit;
  server.hiddenmines = hiddenmines;
  server.pauseonplayerexit = pauseonplayerexit;
  server.basecontrol = 0;
  server.gametype = gametype;
  server.game.domination.type = ((struct Domination *)gamedata)->type;
  server.game.domination.basecontrol = ((struct Domination *)gamedata)->basecontrol;
  server.allowjoin = 1;
  server.pause = initiallypaused ? -1 : 0;
  server.listensock = -1;
  server.dgramsock = -1;
  server.tracker.sock = -1;
  server.joiningplayer.cntlsock = -1;
  server.joiningplayer.disconnect = 0;

TRY
  /* initialized ticks */
  server.ticks = 0;

  /* load the map into server.terrain */
  if (serverloadmap(buf, nbytes)) LOGFAIL(errno)

  for (i = Y_MIN_MINE; i <= Y_MAX_MINE; i++) {
    for (j = X_MIN_MINE; j <= X_MAX_MINE; j++) {
      switch (server.terrain[i][j]) {
      case kCraterTerrain:
      case kMinedCraterTerrain:
        if (floodtest(j - 1, i)) LOGFAIL(errno)
        if (floodtest(j + 1, i)) LOGFAIL(errno)
        if (floodtest(j, i - 1)) LOGFAIL(errno)
        if (floodtest(j, i + 1)) LOGFAIL(errno)

      default:
        break;
      }
    }
  }

  /* initialize tree growing algorithm */
  server.growx = random()%WIDTH;
  server.growy = random()%WIDTH;
  server.growbestof = 0;

  if ((server.passreq = (password != NULL))) {
    strncpy(server.pass, password, MAXPASS - 1);
  }
  else {
    server.pass[0] = '\0';
  }

  /* initialize listensock */
  int one = 1;
  if ((server.listensock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)
  if ((flags = fcntl(server.listensock, F_GETFL, 0)) == -1) LOGFAIL(errno)
  if (fcntl(server.listensock, F_SETFL, flags | O_NONBLOCK));
  setsockopt(server.listensock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  /* initialize dgramsock */
  if ((server.dgramsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) LOGFAIL(errno)
  if ((flags = fcntl(server.dgramsock, F_GETFL, 0)) == -1) LOGFAIL(errno)
  if (fcntl(server.dgramsock, F_SETFL, flags | O_NONBLOCK));

  /* initialize name to INADDR_ANY port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bzero(addr.sin_zero, 8);

  /* bind listensock to name */
  while (bind(server.listensock, (void *)&addr, INET_ADDRSTRLEN)) {
    if (errno != EAGAIN) {
      LOGFAIL(errno)
    }

    usleep(10000);
  }

  addrlen = INET_ADDRSTRLEN;
  if (getsockname(server.listensock, (void *)&addr, &addrlen)) LOGFAIL(errno)

  /* bind dgramsock to name */
  while (bind(server.dgramsock, (void *)&addr, INET_ADDRSTRLEN)) {
    if (errno != EAGAIN) {
      LOGFAIL(errno)
    }

    usleep(10000);
  }

  /* begin listening */
  if (listen(server.listensock, 3)) LOGFAIL(errno)

  if (pipe(server.mainpipe)) LOGFAIL(errno)
  if (pipe(server.threadpipe)) LOGFAIL(errno)

  server.setup = 1;

CLEANUP
ERRHANDLER(0, -1)
END
}

int startserverthread() {
  pthread_t thread;
  int err;

TRY
  if (server.tracker.hostname) {
    free(server.tracker.hostname);
    server.tracker.hostname = NULL;
  }

  if ((err = pthread_create(&thread, NULL, servermainthread, NULL))) LOGFAIL(err)
  if ((err = pthread_detach(thread))) LOGFAIL(err)

  server.running = 1;

CLEANUP
ERRHANDLER(0, -1)
END
}

int startserverthreadwithtracker(
    const char trackerhostname[], uint16_t port, const char hostplayername[],
    const char mapname[], void (*callback)(int status)
  ) {
  pthread_t thread;
  int err;

TRY
  if ((server.tracker.hostname = (char *)malloc(strlen(trackerhostname) + 1)) == NULL) LOGFAIL(errno)
  strcpy(server.tracker.hostname, trackerhostname);
  server.tracker.port = port;
  strncpy(server.tracker.hostplayername, hostplayername, MAXNAME - 1);
  strncpy(server.tracker.mapname, mapname, TRKMAPNAMELEN - 1);
  server.tracker.callback = callback;

  if ((err = pthread_create(&thread, NULL, servermainthread, NULL))) LOGFAIL(err)
  if ((err = pthread_detach(thread))) LOGFAIL(err)

  server.running = 1;

CLEANUP
ERRHANDLER(0, -1)
END
}

int stopserver() {
  char buf[10];
  ssize_t r;

  assert(server.setup);

TRY
  /* close mainpipe */
  if (closesock(server.mainpipe + 1)) LOGFAIL(errno)

  if (!server.running) {
    if (startserverthread()) LOGFAIL(errno)
  }

  /* wait for threadpipe to be closed */
  for (;;) {
    if ((r = read(server.threadpipe[0], buf, sizeof(buf))) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
    }
    else if (r == 0) {
      break;
    }
  }

  server.running = 0;

  if (closesock(server.threadpipe)) LOGFAIL(errno)

  server.setup = 0;

CLEANUP
ERRHANDLER(0, -1)
END
}

int getpauseserver() {
  return server.pause == -1;
}

void pauseserver() {
  if (!getpauseserver()) {
    server.pause = -1;
    sendsrpause(255);
  }
}

void resumeserver() {
  if (getpauseserver()) {
    server.pause = TICKSPERSEC*5;
    sendsrpause(server.pause/TICKSPERSEC);
  }
}

int pauseresumeserver() {
  int gotlock = 0;

TRY
  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  if (getpauseserver()) {
    resumeserver();
  }
  else {
    pauseserver();
  }

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    RETERR(-1)
  }
END
}

int getallowjoinserver() {
  return server.allowjoin;
}

void setallowjoinserver(int allowjoin) {
  server.allowjoin = allowjoin;
}

int togglejoinserver() {
  int gotlock = 0;

TRY
  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  server.allowjoin = !server.allowjoin;

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    RETERR(-1)
  }
END
}

int lockserver() {
  int err;

TRY
  if ((err = pthread_mutex_lock(&server.mutex))) LOGFAIL(err)

CLEANUP
ERRHANDLER(0, -1)
END
}

int unlockserver() {
  int err;

TRY
  if ((err = pthread_mutex_unlock(&server.mutex))) LOGFAIL(err)

CLEANUP
ERRHANDLER(0, -1)
END
}

int kickplayer(int player) {
  int gotlock = 0;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  if (sendsrplayerkick(player)) LOGFAIL(errno)
  if (removeplayer(player)) LOGFAIL(errno)

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    RETERR(-1)
  }
END
}

int banplayer(int player) {
  int gotlock = 0;
  struct BannedPlayer *bannedplayer = NULL;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  if (server.players[player].cntlsock != -1) {
    if ((bannedplayer = (struct BannedPlayer *)malloc(sizeof(struct BannedPlayer))) == NULL) LOGFAIL(errno)
    strncpy(bannedplayer->name, server.players[player].name, MAXNAME - 1);
    bannedplayer->sin_addr = server.players[player].addr.sin_addr;
    if (addlist(&server.bannedplayers, bannedplayer)) LOGFAIL(errno)
    bannedplayer = NULL;
    if (sendsrplayerban(player)) LOGFAIL(errno)
    if (removeplayer(player)) LOGFAIL(errno)
  }

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    if (bannedplayer) {
      free(bannedplayer);
    }

    RETERR(-1)
  }
END
}

int unbanplayer(int index) {
  struct ListNode *node;
  int i;

  int gotlock = 0;

TRY
  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  for (node = nextlist(&server.bannedplayers), i = 0; node && (i < index); node = nextlist(node), i++);

  if (node != NULL) {
    removelist(node, free);
  }

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    RETERR(-1)
  }
END
}

int removeplayer(int player) {
  int i;
  uint16_t pills;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(server.players[player].cntlsock != -1);

TRY
  if (closesock(&server.players[player].cntlsock)) LOGFAIL(errno)
  server.players[player].seq = 0;
  if (readbuf(&server.players[player].recvbuf, NULL, server.players[player].recvbuf.nbytes) == -1) LOGFAIL(errno)
  if (readbuf(&server.players[player].sendbuf, NULL, server.players[player].sendbuf.nbytes) == -1) LOGFAIL(errno)

  pills = 0;

  for (i = 0; i < server.npills; i++) {
    if (server.pills[i].owner == player && server.pills[i].armour == ONBOARD) {
      pills |= 1 << i;
    }
  }

  /* drop pills */
  droppills(player, server.players[player].tank.x, server.players[player].tank.y, pills);

CLEANUP
ERRHANDLER(0, -1)
END
}

int dgramserver() {
  ssize_t r;
  struct CLUpdate clupdate;
  struct sockaddr_in addr;
  socklen_t addrlen;
  uint32_t seq;
  int i;

TRY
  for (;;) {
    addrlen = INET_ADDRSTRLEN;

    if ((r = recvfrom(server.dgramsock, &clupdate, sizeof(clupdate), O_NONBLOCK, (struct sockaddr *)&addr, &addrlen)) == -1) {
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

    /* test packet from tracker */
    if (
        r == sizeof(clupdate.hdr) &&
        clupdate.hdr.player == 255
      ) {

      /* send response */
      if (sendto(server.dgramsock, &clupdate, sizeof(clupdate.hdr), 0, (void *)&addr, addrlen) == -1) {
        if (errno != EAGAIN) LOGFAIL(errno)
      }

      continue;
    }
    else if (  /* sanity check the size */
        r < sizeof(clupdate.hdr) ||
        r != sizeof(clupdate.hdr) + clupdate.hdr.nshells*sizeof(struct CLUpdateShell) + clupdate.hdr.nexplosions*sizeof(struct CLUpdateExplosion) ||
        clupdate.hdr.player >= MAXPLAYERS
      ) {
      continue;
    }

    /* network to host byte order */
    seq = ntohl(clupdate.hdr.seq[clupdate.hdr.player]);

    /* verify this is a valid player */
    if (
        server.players[clupdate.hdr.player].used &&
        server.players[clupdate.hdr.player].cntlsock != -1 &&
        server.players[clupdate.hdr.player].dgramaddr.sin_family == addr.sin_family &&
        server.players[clupdate.hdr.player].dgramaddr.sin_addr.s_addr == addr.sin_addr.s_addr
      ) {
      /* make sure this is not an old update */
      if ((int32_t)(seq - server.players[clupdate.hdr.player].seq) > 0) {
        server.players[clupdate.hdr.player].seq = seq;
        server.players[clupdate.hdr.player].lastupdate = server.ticks;
        *(uint32_t *)&server.players[clupdate.hdr.player].tank.x = ntohl(clupdate.hdr.tankx);
        *(uint32_t *)&server.players[clupdate.hdr.player].tank.y = ntohl(clupdate.hdr.tanky);

        if (server.players[clupdate.hdr.player].dgramaddr.sin_port != addr.sin_port) {
          server.players[clupdate.hdr.player].dgramaddr.sin_port = addr.sin_port;
        }

        /* send update to all other players */
        for (i = 0; i < MAXPLAYERS; i++) {
          if (i != clupdate.hdr.player && server.players[i].cntlsock != -1) {
            if (sendto(server.dgramsock, &clupdate, r, 0, (void *)&server.players[i].dgramaddr, INET_ADDRSTRLEN) == -1) {
              if (errno != EAGAIN) LOGFAIL(errno)
            }
          }
        }
      }
    }
  }

  if (errno != EAGAIN) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int discjoiningplayerserver() {
TRY
  if (sendbuf(&server.joiningplayer.sendbuf, server.joiningplayer.cntlsock) == -1) LOGFAIL(errno)

  if (server.joiningplayer.sendbuf.nbytes == 0) {
    if (closesock(&server.joiningplayer.cntlsock)) LOGFAIL(errno)
    server.joiningplayer.disconnect = 0;
    if (readbuf(&server.joiningplayer.recvbuf, NULL, server.joiningplayer.recvbuf.nbytes) == -1) LOGFAIL(errno)
  }  

CLEANUP
ERRHANDLER(0, -1)
END
}

int joinplayerserver() {
  int i;
  int rejoin;
  int player;
  void *data = NULL;
  ssize_t len;
  uint8_t type;
  struct JOIN_Preamble *joinpreamble;
  struct BOLO_Preamble bolopreamble;

TRY
  if (server.joiningplayer.disconnect) {
    if (discjoiningplayerserver()) LOGFAIL(errno)
    SUCCESS
  }

  if (server.joiningplayer.recvbuf.nbytes < sizeof(struct JOIN_Preamble)) SUCCESS
  joinpreamble = (struct JOIN_Preamble *)server.joiningplayer.recvbuf.ptr;

  /* check client version */
  if (joinpreamble->version != NET_GAME_VERSION) {
    type = kBadVersionJOIN;
    if (writebuf(&server.joiningplayer.sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)
    if (discjoiningplayerserver()) LOGFAIL(errno)
    SUCCESS
  }

  /* check password */
  if (server.passreq && strcmp(joinpreamble->pass, server.pass)) {
    type = kBadPasswordJOIN;
    if (writebuf(&server.joiningplayer.sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)
    if (discjoiningplayerserver()) LOGFAIL(errno)
    SUCCESS
  }

  /* not allowing new players */
  if (!server.allowjoin) {
    type = kDisallowJOIN;
    if (writebuf(&server.joiningplayer.sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)
    if (discjoiningplayerserver()) LOGFAIL(errno)
    SUCCESS
  }

  {  /* check if banned */
    struct ListNode *node;
    struct BannedPlayer *bannedplayer;

    node = nextlist(&server.bannedplayers);

    while (node != NULL) {
      bannedplayer = ptrlist(node);

      if (strncmp(bannedplayer->name, joinpreamble->name, MAXNAME) == 0 && bannedplayer->sin_addr.s_addr == server.joiningplayer.addr.sin_addr.s_addr) {
        type = kBannedPlayerJOIN;
        if (writebuf(&server.joiningplayer.sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)
        if (discjoiningplayerserver()) LOGFAIL(errno)
        SUCCESS
      }

      node = nextlist(node);
    }
  }

  /* see if this ip has already joined once */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].used && server.players[i].cntlsock == -1 && !strncmp(server.players[i].name, joinpreamble->name, MAXNAME)) {
      break;
    }
  }

  /* not a rejoin */
  if (!(i < MAXPLAYERS)) {
    /* find an open slot that has never been used */
    for (i = 0; i < MAXPLAYERS; i++) {
      if (!server.players[i].used && server.players[i].cntlsock == -1) {
        break;
      }
    }

    /* all slots have been used once or are in use, see if we can find a slot that was used but player is not connected */
    if (!(i < MAXPLAYERS)) {
      int p, age;

      /* find first disconnected player */
      for (p = 0; p < MAXPLAYERS; p++) {
        if (server.players[p].cntlsock == -1) {
          i = p;
          age = server.ticks - server.players[p].lastupdate;

          /* looking for the oldest disconnected player */
          for (p++; p < MAXPLAYERS; p++) {
            if (server.players[p].cntlsock == -1) {
              if (age < server.ticks - server.players[p].lastupdate) {
                i = p;
                age = server.ticks - server.players[p].lastupdate;
              }
            }
          }

          break;
        }
      }
    }

    /* server is full */
    if (!(i < MAXPLAYERS)) {
      type = kServerFullJOIN;
      if (writebuf(&server.joiningplayer.sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)
      if (discjoiningplayerserver()) LOGFAIL(errno)
      SUCCESS
    }

    server.players[i].alliance = 1 << i;
    strncpy(server.players[i].name, joinpreamble->name, sizeof(server.players[i].name) - 1);
    rejoin = 0;
  }
  else {
    rejoin = 1;
  }

  if (readbuf(&server.joiningplayer.recvbuf, NULL, sizeof(struct JOIN_Preamble)) == -1) LOGFAIL(errno)

  /* initialize player */
  player = i;

  freebuf(&server.players[player].recvbuf);

  server.players[player].used = 1;
  server.players[player].cntlsock = server.joiningplayer.cntlsock;
  server.players[player].addr = server.joiningplayer.addr;
  server.players[player].dgramaddr = server.joiningplayer.addr;
//  server.players[player].seq = 0;
  server.players[player].lastupdate = server.ticks;
  server.players[player].recvbuf = server.joiningplayer.recvbuf;

  server.joiningplayer.cntlsock = -1;
  if (initbuf(&server.joiningplayer.recvbuf)) LOGFAIL(errno)

  /* sending preamble */
  type = kSendingPreambleJOIN;
  if (writebuf(&server.players[player].sendbuf, &type, sizeof(type)) == -1) LOGFAIL(errno)

  /* init bolo preamble */
  bcopy(NET_GAME_IDENT, bolopreamble.ident, sizeof(NET_GAME_IDENT));
  bolopreamble.version = NET_GAME_VERSION;
  bolopreamble.player = player;
  bolopreamble.hiddenmines = server.hiddenmines;

  if (server.pause == -1) {
    bolopreamble.pause = 255;
  }
  else {
    bolopreamble.pause = server.pause/TICKSPERSEC;
  }

  bolopreamble.gametype = server.gametype;
  bolopreamble.game.domination.type = server.game.domination.type;
  bolopreamble.game.domination.basecontrol = server.game.domination.basecontrol;

  for (i = 0; i < MAXPLAYERS; i++) {
    bolopreamble.players[i].used = server.players[i].used;
    bolopreamble.players[i].connected = server.players[i].cntlsock != -1;
    bolopreamble.players[i].seq = htonl(server.players[i].seq);
    bzero(bolopreamble.players[i].name, MAXNAME);
    strncpy((char *)bolopreamble.players[i].name, server.players[i].name, MAXNAME - 1);
    bzero(bolopreamble.players[i].host, MAXHOST);
    strncpy((char *)bolopreamble.players[i].host, server.players[i].host, MAXHOST - 1);
    bolopreamble.players[i].alliance = htons(server.players[i].alliance);
  }

  /* encode map */
  if ((len = serversavemap(&data)) == -1) LOGFAIL(errno)
  bolopreamble.maplen = htonl(len);

  /* send game data */
  if (writebuf(&server.players[player].sendbuf, &bolopreamble, sizeof(bolopreamble)) == -1) LOGFAIL(errno)
  if (writebuf(&server.players[player].sendbuf, data, len) == -1) LOGFAIL(errno)
//  if (sendplayerbufserver(player)) LOGFAIL(errno)

  /* free map */
  free(data);
  data = NULL;

  /* set TCP_NODELAY on socket */
  i = 1;
  if (setsockopt(server.players[player].cntlsock, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i))) LOGFAIL(errno)

  /* player join */
  if (rejoin) {
    if (sendsrplayerrejoin(player)) LOGFAIL(errno)
  }
  else {
    if (sendsrplayerjoin(player)) LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int selectserver(fd_set *readfds, fd_set *writefds, struct timeval *timeout) {
  int i;
  int nfds;

TRY
  nfds = 0;
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  
  /* read on main pipe */
  FD_SET(server.mainpipe[0], readfds);
  nfds = MAX(nfds, server.mainpipe[0]);
  
  /* read on dgram sock */
  FD_SET(server.dgramsock, readfds);
  nfds = MAX(nfds, server.dgramsock);
  
  /* read/write player control socks */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].cntlsock != -1) {
      FD_SET(server.players[i].cntlsock, readfds);
      nfds = MAX(nfds, server.players[i].cntlsock);
      
      if (server.players[i].sendbuf.nbytes > 0) {
        FD_SET(server.players[i].cntlsock, writefds);
      }
    }
  }
  
  /* read/write on joining player sock */
  if (server.joiningplayer.cntlsock != -1) {
    FD_SET(server.joiningplayer.cntlsock, readfds);
    nfds = MAX(nfds, server.joiningplayer.cntlsock);
    
    if (server.joiningplayer.sendbuf.nbytes > 0) {
      FD_SET(server.joiningplayer.cntlsock, writefds);
    }
  }
  
  /* accept on listen sock if we don't already have a joining player */
  if (server.joiningplayer.cntlsock == -1) {
    FD_SET(server.listensock, readfds);
    nfds = MAX(nfds, server.listensock);
  }
  
  /* write on tracker sock */
  if (server.tracker.sendbuf.nbytes > 0) {
    FD_SET(server.tracker.sock, writefds);
    nfds = MAX(nfds, server.tracker.sock);
  }
  
  if ((nfds = select(nfds + 1, readfds, writefds, NULL, timeout)) == -1) {
    if (errno != EINTR) {
      LOGFAIL(errno)
    }

    nfds = 0;
  }
  
CLEANUP
  switch(ERROR) {
  case 0:
    RETURN(nfds)
  default:
    RETERR(-1)
  }
END
}

int recvplayerserver(int player) {
  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  for (;;) {
    if (server.players[player].recvbuf.nbytes < sizeof(uint8_t)) FAIL(EAGAIN)

    switch (*(uint8_t *)server.players[player].recvbuf.ptr) {
    case CLSENDMESG:
      if (recvclsendmesg(player)) LOGFAIL(errno)
      break;

    case CLDROPBOAT:
      if (recvcldropboat(player)) LOGFAIL(errno)
      break;

    case CLDROPPILLS:
      if (recvcldroppills(player)) LOGFAIL(errno)
      break;

    case CLDROPMINE:
      if (recvcldropmine(player)) LOGFAIL(errno)
      break;

    case CLTOUCH:
      if (recvcltouch(player)) LOGFAIL(errno)
      break;

    case CLGRABTILE:
      if (recvclgrabtile(player)) LOGFAIL(errno)
      break;

    case CLGRABTREES:
      if (recvclgrabtrees(player)) LOGFAIL(errno)
      break;

    case CLBUILDROAD:
      if (recvclbuildroad(player)) LOGFAIL(errno)
      break;

    case CLBUILDWALL:
      if (recvclbuildwall(player)) LOGFAIL(errno)
      break;

    case CLBUILDBOAT:
      if (recvclbuildboat(player)) LOGFAIL(errno)
      break;

    case CLBUILDPILL:
      if (recvclbuildpill(player)) LOGFAIL(errno)
      break;

    case CLREPAIRPILL:
      if (recvclrepairpill(player)) LOGFAIL(errno)
      break;

    case CLPLACEMINE:
      if (recvclplacemine(player)) LOGFAIL(errno)
      break;

    case CLDAMAGE:
      if (recvcldamage(player)) LOGFAIL(errno)
      break;

    case CLSMALLBOOM:
      if (recvclsmallboom(player)) LOGFAIL(errno)
      break;

    case CLSUPERBOOM:
      if (recvclsuperboom(player)) LOGFAIL(errno)
      break;

    case CLREFUEL:
      if (recvclrefuel(player)) LOGFAIL(errno)
      break;

    case CLHITTANK:
      if (recvclhittank(player)) LOGFAIL(errno)
      break;

    case CLSETALLIANCE:
      if (recvclsetalliance(player)) LOGFAIL(errno)
      break;

    case kHangupClientMessage:
      SUCCESS
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

int runserver() {
  int i;
  int nplayers;

TRY
  /* pause logic */
  if (server.pause) {
    if (server.pause > 0) {
      server.pause--;

      if (server.pause%TICKSPERSEC == 0) {
        sendsrpause(server.pause/TICKSPERSEC);
      }
    }

    SUCCESS
  }

  /* time limited games */
  if (server.timelimit > 0) {
    if (server.ticks == TICKSPERSEC*(server.timelimit - 300)) {  /* five minute warning */
      sendsrtimelimit(300);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 60)) {  /* one minute warning */
      sendsrtimelimit(60);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 10)) {  /* ten second warning */
      sendsrtimelimit(10);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 5)) {  /* five second warning */
      sendsrtimelimit(5);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 4)) {  /* four second warning */
      sendsrtimelimit(4);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 3)) {  /* three second warning */
      sendsrtimelimit(3);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 2)) {  /* two second warning */
      sendsrtimelimit(2);
    }
    else if (server.ticks == TICKSPERSEC*(server.timelimit - 1)) {  /* one second warning */
      sendsrtimelimit(1);
    }
    else if (server.ticks == TICKSPERSEC*server.timelimit) {  /* time limit reached */
      sendsrtimelimit(0);
      server.ticks++;
      SUCCESS
    }
    else if (server.ticks > TICKSPERSEC*server.timelimit) {  /* time limit reached */
      SUCCESS
    }
  }

  /* game type logic */
  switch (server.gametype) {
  case kDominationGameType:
    if (server.nbases > 0 && server.bases[0].armour >= MINBASEARMOUR && server.bases[0].owner != NEUTRAL) {
      for (i = 1; i < server.nbases && server.bases[i].armour >= MINBASEARMOUR && ((server.players[server.bases[i].owner].alliance & (1 << server.bases[0].owner)) && (server.players[server.bases[0].owner].alliance & (1 << server.bases[i].owner))); i++) ;

      if (i == server.nbases) {
        server.basecontrol++;

        if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 10)) {  /* ten second warning */
          sendsrbasecontrol(10);
        }
        else if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 5)) {  /* five second warning */
          sendsrbasecontrol(5);
        }
        else if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 4)) {  /* four second warning */
          sendsrbasecontrol(4);
        }
        else if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 3)) {  /* three second warning */
          sendsrbasecontrol(3);
        }
        else if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 2)) {  /* two second warning */
          sendsrbasecontrol(2);
        }
        else if (server.basecontrol == TICKSPERSEC*(server.game.domination.basecontrol - 1)) {  /* one second warning */
          sendsrbasecontrol(1);
        }
        else if (server.basecontrol == TICKSPERSEC*server.game.domination.basecontrol) {  /* time limit reached */
          sendsrbasecontrol(0);
          server.ticks++;
          SUCCESS
        }
        else if (server.basecontrol > TICKSPERSEC*server.game.domination.basecontrol) {  /* time limit reached */
          SUCCESS
        }
      }
      else{
        server.basecontrol = 0;
      }
    }

    break;

  default:
    break;
  }

  server.ticks++;

  nplayers = 0;

  /* disconnect lagged players */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].cntlsock != -1) {
      if (server.ticks - server.players[i].lastupdate >= 9*TICKSPERSEC) {
        if (removeplayer(i)) LOGFAIL(errno)
        if (sendsrplayerdisc(i)) LOGFAIL(errno)

        if (server.pauseonplayerexit) {
          server.pause = -1;
          sendsrpause(255);
        }
      }
      else {
        nplayers++;
      }
    }
  }

  /* cool pills */
  for (i = 0; i < server.npills; i++) {
    if (server.pills[i].armour != ONBOARD) {
      server.pills[i].counter++;

      if (server.pills[i].counter >= COOLPILLTICKS) {
        if (server.pills[i].speed < MAXTICKSPERSHOT) {
          server.pills[i].speed++;
          sendsrcoolpill(i);
        }

        server.pills[i].counter = 0;
      }
    }
  }

  /* replenish bases */
  for (i = 0; i < server.nbases; i++) {
    server.bases[i].counter += nplayers;

    if (server.bases[i].counter >= REPLENISHBASETICKS) {
      if (++server.bases[i].armour > MAXBASEARMOUR) {
        server.bases[i].armour = MAXBASEARMOUR;
      }

      if (++server.bases[i].mines > MAXBASEMINES) {
        server.bases[i].mines = MAXBASEMINES;
      }

      if (++server.bases[i].shells > MAXBASESHELLS) {
        server.bases[i].shells = MAXBASESHELLS;
      }

      sendsrreplenishbase(i);

      server.bases[i].counter = 0;
    }
  }

  /* grow trees */
  growtrees(nplayers);

  /* chain detonation */
  if (chain()) LOGFAIL(errno)

  /* flood fill */
  if (flood()) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int registerserver() {
  int lookup = -1;
  int ret = 1;

TRY
  if (server.tracker.hostname) {
    struct TRACKER_Preamble preamble;
    struct TrackerHost trackerhost;
    int flags;
    int ret;
    uint8_t msg;

    if (server.tracker.callback) {
      server.tracker.callback(kRegisterRESOLVING);
    }

    /* lookup address */
    if ((lookup = nslookup(server.tracker.hostname)) == -1) LOGFAIL(errno)

    /* wait for exit or dns reply */
    if ((ret = selectreadread(server.mainpipe[0], lookup)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      if (closesock(&lookup)) LOGFAIL(errno)
      SUCCESS
    }

    /* get nslookup result */
    if (nslookupresult(lookup, &server.tracker.addr.sin_addr)) LOGFAIL(errno)
    if (closesock(&lookup)) LOGFAIL(errno)

    server.tracker.addr.sin_family = AF_INET;
    server.tracker.addr.sin_port = htons(TRACKERPORT);
    bzero(server.tracker.addr.sin_zero, 8);

    if (server.tracker.callback) {
      server.tracker.callback(kRegisterCONNECTING);
    }

    /* initialize trackersock */
    if ((server.tracker.sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)
    if ((flags = fcntl(server.tracker.sock, F_GETFL, 0)) == -1) LOGFAIL(errno)
    if (fcntl(server.tracker.sock, F_SETFL, flags | O_NONBLOCK));

    if ((connect(server.tracker.sock, (struct sockaddr *)&server.tracker.addr, INET_ADDRSTRLEN))) {
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

        FD_SET(server.mainpipe[0], &readfds);
        nfds = MAX(nfds, server.mainpipe[0]);

        FD_SET(server.tracker.sock, &writefds);
        nfds = MAX(nfds, server.tracker.sock);

        if ((nfds = select(nfds + 1, &readfds, &writefds, NULL, NULL)) == -1) {
          if (errno != EINTR) LOGFAIL(errno)
        }
        else {
          break;
        }
      }

      if (FD_ISSET(server.mainpipe[0], &readfds)) {
        SUCCESS
      }
      else if (FD_ISSET(server.tracker.sock, &writefds)) {
        if (connect(server.tracker.sock, (struct sockaddr *)&server.tracker.addr, INET_ADDRSTRLEN) && errno != EISCONN) LOGFAIL(errno)
        break;
      }
    }

    if ((ret = cntlsend(server.mainpipe[0], server.tracker.sock, &server.tracker.sendbuf)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      SUCCESS
    }

    if (server.tracker.callback) {
      server.tracker.callback(kRegisterSENDINGDATA);
    }

    /* send preamble */
    bcopy(TRACKERIDENT, preamble.ident, sizeof(preamble.ident));
    preamble.version = TRACKERVERSION;
    if (writebuf(&server.tracker.sendbuf, &preamble, sizeof(preamble)) == -1) LOGFAIL(errno)

    if ((ret = cntlsend(server.mainpipe[0], server.tracker.sock, &server.tracker.sendbuf)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      SUCCESS
    }

    if ((ret = cntlrecv(server.mainpipe[0], server.tracker.sock, &server.tracker.recvbuf, sizeof(uint8_t))) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      SUCCESS
    }

    if (readbuf(&server.tracker.recvbuf, &msg, sizeof(uint8_t)) == -1) LOGFAIL(errno)

    /* receive ok */
    if (msg != kTrackerVersionOK) LOGFAIL(EBADVERSION)

    /* send game data */
    msg = kTrackerHost;
    strncpy((char *)trackerhost.playername, server.tracker.hostplayername, TRKPLYRNAMELEN - 1);
    strncpy((char *)trackerhost.mapname, server.tracker.mapname, TRKMAPNAMELEN - 1);
    trackerhost.port = htons(server.tracker.port);
    trackerhost.timelimit = htonl(server.timelimit);
    trackerhost.passreq = server.passreq;
    trackerhost.nplayers = nplayers();
    trackerhost.allowjoin = server.allowjoin;
    trackerhost.pause = server.pause == -1;
    trackerhost.gametype = server.gametype;
    if (writebuf(&server.tracker.sendbuf, &msg, sizeof(msg)) == -1) LOGFAIL(errno)
    if (writebuf(&server.tracker.sendbuf, &trackerhost, sizeof(trackerhost)) == -1) LOGFAIL(errno)

    if ((ret = cntlsend(server.mainpipe[0], server.tracker.sock, &server.tracker.sendbuf)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      SUCCESS
    }

    if (server.tracker.callback) {
      server.tracker.callback(kRegisterTESTINGTCP);
    }

    /* receive tcp ok */
    if ((ret = cntlrecv(server.mainpipe[0], server.tracker.sock, &server.tracker.recvbuf, sizeof(msg))) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      SUCCESS
    }

    if (readbuf(&server.tracker.recvbuf, &msg, sizeof(uint8_t)) == -1) LOGFAIL(errno)
    if (msg != kTrackerTCPPortOK) LOGFAIL(ETCPCLOSED)

    if (server.tracker.callback) {
      server.tracker.callback(kRegisterTESTINGUDP);
    }

    /* receive udp ok */
    for (;;) {
      int nfds;
      fd_set readfds;

      /* select until we have success */
      for (;;) {
        nfds = 0;
        FD_ZERO(&readfds);
        FD_SET(server.mainpipe[0], &readfds);
        nfds = MAX(nfds, server.mainpipe[0]);
        FD_SET(server.dgramsock, &readfds);
        nfds = MAX(nfds, server.dgramsock);
        FD_SET(server.tracker.sock, &readfds);
        nfds = MAX(nfds, server.tracker.sock);

        if ((nfds = select(nfds + 1, &readfds, NULL, NULL, NULL)) == -1) {
          if (errno != EINTR) LOGFAIL(errno)
        }
        else {
          break;
        }
      }

      if (FD_ISSET(server.mainpipe[0], &readfds)) {
        SUCCESS
      }
      else if (FD_ISSET(server.dgramsock, &readfds)) {
        struct sockaddr_in addr;
        socklen_t addrlen;
        struct CLUpdate clupdate;
        ssize_t r;

        for (;;) {
          addrlen = INET_ADDRSTRLEN;

          if ((r = recvfrom(server.dgramsock, &clupdate, sizeof(clupdate), O_NONBLOCK, (void *)&addr, &addrlen)) == -1) {
            if (errno == EAGAIN) {
              break;
            }
            else if (errno != EINTR) {
              LOGFAIL(errno)
            }
            else {
              continue;
            }
          }

          /* test packet from tracker */
          if (
              r == sizeof(clupdate.hdr) &&
              clupdate.hdr.player == 255
            ) {

            bzero(&clupdate, sizeof(clupdate));
            clupdate.hdr.player = 255;

            /* send response */
            if (sendto(server.dgramsock, &clupdate, sizeof(clupdate.hdr), 0, (void *)&addr, addrlen) == -1) {
              if (errno != EAGAIN) LOGFAIL(errno)
            }
          }
        }
      }
      else if (FD_ISSET(server.tracker.sock, &readfds)) {
        if (recvbuf(&server.tracker.recvbuf, server.tracker.sock) == -1) LOGFAIL(errno)

        if (server.tracker.recvbuf.nbytes >= sizeof(msg)) {
          break;
        }
      }
    }

    if (readbuf(&server.tracker.recvbuf, &msg, sizeof(uint8_t)) == -1) LOGFAIL(errno)
    if (msg != kTrackerUDPPortOK) LOGFAIL(EUDPCLOSED)

    /* successfully registered */
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterSUCCESS);
    }
  }

  ret = 0;

CLEANUP
  if (lookup != -1) {
    closesock(&lookup);
  }

ERRHANDLER(ret, -1)
END
}

int cleanupserver() {
  int i;

TRY
  if (closesock(&server.listensock)) LOGFAIL(errno)

  if (server.tracker.hostname) {
    if (server.tracker.sock != -1) {
      if (closesock(&server.tracker.sock)) LOGFAIL(errno)
    }

    free(server.tracker.hostname);
    server.tracker.hostname = NULL;
  }

  if (closesock(&server.dgramsock)) LOGFAIL(errno)

  /* close joining player sock and clear buffers */
  if (server.joiningplayer.cntlsock != -1) {
    if (closesock(&server.joiningplayer.cntlsock)) LOGFAIL(errno)
  }

  server.joiningplayer.disconnect = 0;
  if (readbuf(&server.joiningplayer.recvbuf, NULL, server.joiningplayer.recvbuf.nbytes) == -1) LOGFAIL(errno)
  if (readbuf(&server.joiningplayer.sendbuf, NULL, server.joiningplayer.sendbuf.nbytes) == -1) LOGFAIL(errno)

  /* close player socks and clear buffers */
  for (i = 0; i < MAXPLAYERS; i++) {
    server.players[i].used = 0;
    server.players[i].seq = 0;

    bzero(server.players[i].name, MAXNAME);

    if (server.players[i].cntlsock != -1) {
      if (closesock(&server.players[i].cntlsock)) LOGFAIL(errno)
    }

    if (readbuf(&server.players[i].recvbuf, NULL, server.players[i].recvbuf.nbytes) == -1) LOGFAIL(errno)
    if (readbuf(&server.players[i].sendbuf, NULL, server.players[i].sendbuf.nbytes) == -1) LOGFAIL(errno)
  }

  /* cleanup chain detonation */
  for (i = 0; i < (CHAINTICKS + 1); i++) {
    clearlist(server.chains + i, free);
  }

  /* flood fill detonation */
  for (i = 0; i < (FLOODTICKS + 1); i++) {
    clearlist(server.floods + i, free);
  }

  clearlist(&server.bannedplayers, free);

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendtrackerupdate() {
  struct TrackerHost trackerhost;

TRY
  strncpy((char *)trackerhost.playername, server.tracker.hostplayername, TRKPLYRNAMELEN - 1);
  strncpy((char *)trackerhost.mapname, server.tracker.mapname, TRKMAPNAMELEN - 1);
  trackerhost.port = htons(server.tracker.port);
  trackerhost.gametype = server.gametype;
  trackerhost.timelimit = server.timelimit;
  trackerhost.passreq = server.passreq;
  trackerhost.nplayers = nplayers();
  trackerhost.allowjoin = getallowjoinserver();
  trackerhost.pause = getpauseserver();
  if (writebuf(&server.tracker.sendbuf, &trackerhost, sizeof(trackerhost)) == -1) LOGFAIL(errno)
  if (sendbuf(&server.tracker.sendbuf, server.tracker.sock) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

void *servermainthread(void *arg) {
  int i;
  uint64_t currenttime;
  uint64_t nexttick;
  uint64_t nexttrackerupdate;
  int gotlock = 0;
  
TRY

  if (lockserver()) LOGFAIL(errno)
  gotlock = 1;

  /* register with tracker */
  if ((i = registerserver()) == -1) {
    LOGFAIL(errno)
  }
  else if (i == 1) {  /* closed by main thread */
    SUCCESS
  }

  if (unlockserver()) LOGFAIL(errno)
  gotlock = 0;

  /* initialize times */
  nexttick = getcurrenttime();
  nexttrackerupdate = nexttick + TRACKERUPDATESECONDS*1000000000ull;

  /* main loop */
  for (;;) {
    currenttime = getcurrenttime();

    /* send update to tracker */
    if (server.tracker.hostname && server.tracker.sendbuf.nbytes == 0 && currenttime >= nexttrackerupdate) {
      if (sendtrackerupdate() == -1) LOGFAIL(errno)

      nexttrackerupdate = nexttrackerupdate + (TRACKERUPDATESECONDS*1000000000ull);

      if ((currenttime = getcurrenttime()) >= nexttrackerupdate) {
        nexttrackerupdate = currenttime + (TRACKERUPDATESECONDS*1000000000ull);
      }
    }

    if ((currenttime = getcurrenttime()) < nexttick) {
      int nfds;
      fd_set readfds;
      fd_set writefds;
      struct timeval timeout;

      timeout.tv_sec = 0;
      timeout.tv_usec = (nexttick - currenttime)/1000;

      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if ((nfds = selectserver(&readfds, &writefds, &timeout)) == -1) LOGFAIL(errno)

      /* time to quit */
      if (FD_ISSET(server.mainpipe[0], &readfds)) {
        for (i = 0; i < MAXPLAYERS; i++) {
          if (server.players[i].cntlsock != -1) {
            if (removeplayer(i)) LOGFAIL(errno)
          }
        }

        break;
      }

      /* we have i/o */
      if (nfds > 0) {
        /* read dgram sock */
        if (FD_ISSET(server.dgramsock, &readfds)) {
          if (dgramserver()) LOGFAIL(errno)
        }

        /* read player socks */
        for (i = 0; i < MAXPLAYERS; i++) {
          if (server.players[i].cntlsock != -1) {
            if (FD_ISSET(server.players[i].cntlsock, &readfds)) {
              ssize_t r;

              if ((r = recvbuf(&server.players[i].recvbuf, server.players[i].cntlsock)) == -1) {
                if (errno == ECONNRESET) {
                  CLEARERRLOG

                  /* parse received data */
                  if (recvplayerserver(i) == -1) {
                    /* player exited abnomrally */
                    if (removeplayer(i)) LOGFAIL(errno)
                    if (sendsrplayerdisc(i)) LOGFAIL(errno)
                  }
                  else {
                    /* player exited normally */
                    if (removeplayer(i)) LOGFAIL(errno)
                    if (sendsrplayerexit(i)) LOGFAIL(errno)
                  }

                  if (server.pauseonplayerexit) {
                    server.pause = -1;
                    sendsrpause(255);
                  }
                }
                else {
                  LOGFAIL(errno)
                }
              }
              else if (r == 0) {  /* socket closed, disconnect player */
                /* parse received data */
                if (recvplayerserver(i) == -1) {
                  /* player exited abnomrally */
                  if (removeplayer(i)) LOGFAIL(errno)
                  if (sendsrplayerdisc(i)) LOGFAIL(errno)
                }
                else {
                  /* player exited normally */
                  if (removeplayer(i)) LOGFAIL(errno)
                  if (sendsrplayerexit(i)) LOGFAIL(errno)
                }

                if (server.pauseonplayerexit) {
                  server.pause = -1;
                  sendsrpause(255);
                }
              }
              else {
                /* parse data */
                if (recvplayerserver(i) == -1) {
                  /* error occured */
                  if (errno != EAGAIN) {  /* a unrecoverable error occured */
                    if (removeplayer(i)) LOGFAIL(errno)
                    if (sendsrplayerdisc(i)) LOGFAIL(errno)

                    if (server.pauseonplayerexit) {
                      server.pause = -1;
                      sendsrpause(255);
                    }

                    CLEARERRLOG
                  }
                }
                else {
                  /* player exited normally */
                  if (removeplayer(i)) LOGFAIL(errno)
                  if (sendsrplayerexit(i)) LOGFAIL(errno)

                  if (server.pauseonplayerexit) {
                    server.pause = -1;
                    sendsrpause(255);
                  }
                }
              }
            }
          }
        }

        /* write player socks */
        for (i = 0; i < MAXPLAYERS; i++) {
          if (server.players[i].cntlsock != -1 && server.players[i].sendbuf.nbytes > 0) {
            if (sendbuf(&server.players[i].sendbuf, server.players[i].cntlsock) == -1) {
              if (errno != EPIPE) {  /* receving data code will disconnect player next time through the loop */
                LOGFAIL(errno)
              }
            }
          }
        }

        /* read joining player sock */
        if (server.joiningplayer.cntlsock != -1 && FD_ISSET(server.joiningplayer.cntlsock, &readfds)) {
          ssize_t r;

          if ((r = recvbuf(&server.joiningplayer.recvbuf, server.joiningplayer.cntlsock)) == -1) {
            if (errno == ECONNRESET) {
              CLEARERRLOG
              if (closesock(&server.joiningplayer.cntlsock)) LOGFAIL(errno)
            }
            else {
              LOGFAIL(errno)
            }
          }
          else if (r == 0) {
            if (closesock(&server.joiningplayer.cntlsock)) LOGFAIL(errno)
          }
          else {
            if (joinplayerserver()) LOGFAIL(errno)
          }
        }

        /* write joining player sock */
        if (server.joiningplayer.cntlsock != -1 && FD_ISSET(server.joiningplayer.cntlsock, &writefds)) {
          if (sendbuf(&server.joiningplayer.sendbuf, server.joiningplayer.cntlsock) == -1) LOGFAIL(errno)
          if (joinplayerserver()) LOGFAIL(errno)
        }

        /* accept new connections */
        if (FD_ISSET(server.listensock, &readfds)) {
          socklen_t addrlen;
          addrlen = INET_ADDRSTRLEN;
          if ((server.joiningplayer.cntlsock = accept(server.listensock, (void *)&server.joiningplayer.addr, &addrlen)) == -1) LOGFAIL(errno)
        }

        /* send buf to tracker sock */
        if (server.tracker.sock != -1 && FD_ISSET(server.tracker.sock, &writefds)) {
          if (sendbuf(&server.tracker.sendbuf, server.tracker.sock) == -1) LOGFAIL(errno)
        }
      }

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
    }
    else {
      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if (runserver()) LOGFAIL(errno)

      if (unlockserver()) LOGFAIL(errno)
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
    lockserver();
  }

  cleanupserver();
  unlockserver();

  switch (ERROR) {
  case 0:
    break;

  case EHOSTNOTFOUND:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEHOSTNOTFOUND);
    }

    break;

  case EHOSTNORECOVERY:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEHOSTNORECOVERY);
    }

    break;

  case EHOSTNODATA:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEHOSTNODATA);
    }

    break;

  case ETIMEDOUT:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterETIMEOUT);
    }

    break;

  case EBADVERSION:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEBADVERSION);
    }

    break;

  case ECONNREFUSED:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterECONNREFUSED);
    }

    break;

  case EHOSTDOWN:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEHOSTDOWN);
    }

    break;

  case EHOSTUNREACH:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEHOSTUNREACH);
    }

    break;

  case ETCPCLOSED:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterETCPCLOSED);
    }

    break;

  case EUDPCLOSED:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterEUDPCLOSED);
    }

    break;

  case ECONNRESET:
    if (server.tracker.callback) {
      server.tracker.callback(kRegisterECONNRESET);
    }

    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    break;
  }

  /* close thread pipe */
  closesock(server.mainpipe);
  closesock(server.threadpipe + 1);
  CLEARERRLOG
  pthread_exit(NULL);
END
}

int clearterrain(int x, int y) {
  if (x < X_MIN_MINE || x > X_MAX_MINE || y < Y_MIN_MINE || y > Y_MAX_MINE) {
    return 0;
  }

  switch (server.terrain[y][x]) {
  case kSeaTerrain:
  case kBoatTerrain:
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
  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    return findpill(x, y) == -1 && findbase(x, y) == -1;

  case kWallTerrain:
  case kDamagedWallTerrain0:
  case kDamagedWallTerrain1:
  case kDamagedWallTerrain2:
  case kDamagedWallTerrain3:
    return 0;

  default:
    assert(0);
    return 0;
  }
}

int dr(int x, int y, int i, uint16_t pills) {
  if (clearterrain(x, y) && findpill(x, y) == -1) {
    while (i < server.npills) {
      if ((1 << i) & pills) {
        server.pills[i].armour = 0;
        server.pills[i].x = x;
        server.pills[i].y = y;
        sendsrdroppill(i);
        i++;
        break;
      }

      i++;
    }
  }

  return i;
}

void droppills(int player, float x, float y, uint16_t pills) {
  int i, j;
  float lx, hx, ly, hy;
  int minx, miny, maxx, maxy;

  assert(player >= 0 && player < MAXPLAYERS);

  if (x < 0.0) {
    x = 0.0;
  }
  else if (x >= FWIDTH) {
    x = FWIDTH - 0.00001;
  }
  else if (isnan(x)) {
    x = FWIDTH/2.0;
  }

  if (y < 0.0) {
    y = 0.0;
  }
  else if (y >= FWIDTH) {
    y = FWIDTH - 0.00001;
  }
  else if (isnan(x)) {
    y = FWIDTH/2.0;
  }

  minx = (int)x;
  maxx = minx + 1;
  miny = (int)y;
  maxy = miny + 1;

  i = dr(minx, miny, 0, pills);

  while (i < server.npills) {
    /* find closest edge */
    lx = x - minx;
    hx = maxx - x;
    ly = y - miny;
    hy = maxy - y;

    if (lx <= hx && lx <= ly && lx <= hy) {
      minx--;

      for (j = 0; miny + j < maxy; j++) {
        i = dr(minx, miny + j, i, pills);
      }
    }
    else if (hx <= lx && hx <= ly && hx <= hy) {
      for (j = 0; miny + j < maxy; j++) {
        i = dr(maxx, miny + j, i, pills);
      }

      maxx++;
    }
    else if (ly <= lx && ly <= hx && ly <= hy) {
      miny--;

      for (j = 0; minx + j < maxx; j++) {
        i = dr(minx + j, miny, i, pills);
      }
    }
    else if (hy <= lx && hy <= hx && hy <= ly) {
      for (j = 0; minx + j < maxx; j++) {
        i = dr(minx + j, maxy, i, pills);
      }

      maxy++;
    }
    else {
      assert(0);
    }
  }
}

int recvclsendmesg(int player) {
  int i;
  struct CLSendMesg *clsendmesg;
  char *text;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  /* check for whole struct */
  if (server.players[player].recvbuf.nbytes < sizeof(clsendmesg)) FAIL(EAGAIN)

  /* check for complete string following struct */
  for (i = sizeof(struct CLSendMesg) ; i < server.players[player].recvbuf.nbytes; i++) {
    if (((char *)server.players[player].recvbuf.ptr)[i] == '\0') {
      break;
    }
  }

  /* buf does not have the whole string */
  if (!(i < server.players[player].recvbuf.nbytes)) FAIL(EAGAIN)

  /* get pointers to data */
  clsendmesg = (struct CLSendMesg *)server.players[player].recvbuf.ptr;
  /* get pointer to text */
  text = (char *)(clsendmesg + 1);

  /* convert byte order */
  clsendmesg->mask = ntohs(clsendmesg->mask);

  /* send message to all players */
  if (sendsrsendmesg(player, clsendmesg->to, clsendmesg->mask, text)) LOGFAIL(errno)

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, i + 1) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvcldropboat(int player) {
  struct CLDropBoat *cldropboat;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  /* receive message */
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLDropBoat)) FAIL(EAGAIN)

  cldropboat = (struct CLDropBoat *)server.players[player].recvbuf.ptr;

  if (ispointinrect(kSeaRect, makepoint(cldropboat->x, cldropboat->y))) {
    if (server.terrain[cldropboat->y][cldropboat->x] == kRiverTerrain) {
      server.terrain[cldropboat->y][cldropboat->x] = kBoatTerrain;
      sendsrdropboat(cldropboat->x, cldropboat->y);
    }

    /* clear buffer of read data */
    if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLDropBoat)) == -1) LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvcldroppills(int player) {
  struct CLDropPills *cldroppills;
  int i;
  float x, y;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLDropPills)) FAIL(EAGAIN)

  cldroppills = (struct CLDropPills *)server.players[player].recvbuf.ptr;

  cldroppills->pills = ntohs(cldroppills->pills);
  cldroppills->x = ntohl(cldroppills->x);
  cldroppills->y = ntohl(cldroppills->y);
  x = *((float *)&cldroppills->x);
  y = *((float *)&cldroppills->y);

  for (i = 0; i < server.npills; i++) {
    if ((cldroppills->pills & (1 << i)) && (server.pills[i].owner != player || server.pills[i].armour != ONBOARD)) {
      break;
    }
  }

  if (!(i < server.npills) && x > 0.0 && x < FWIDTH && y > 0.0 && y < FWIDTH) {
    droppills(player, x, y, cldroppills->pills);
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLDropPills)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvcldropmine(int player) {
  struct CLDropMine *cldropmine;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLDropMine)) FAIL(EAGAIN)

  cldropmine = (struct CLDropMine *)server.players[player].recvbuf.ptr;

  if (ispointinrect(kSeaRect, makepoint(cldropmine->x, cldropmine->y))) {
    switch (server.terrain[cldropmine->y][cldropmine->x]) {
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedSwampTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    case kCraterTerrain:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedCraterTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    case kRoadTerrain:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedRoadTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    case kForestTerrain:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedForestTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedRubbleTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
      server.terrain[cldropmine->y][cldropmine->x] = kMinedGrassTerrain;
      sendsrdropmine(player, cldropmine->x, cldropmine->y);
      sendsrmineack(player, 1);
      break;

    default:
      sendsrmineack(player, 0);
      break;
    }

    /* clear buffer of read data */
    if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLDropMine)) == -1) LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvcltouch(int player) {
  struct CLTouch *cltouch;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLTouch)) FAIL(EAGAIN)

  cltouch = (struct CLTouch *)server.players[player].recvbuf.ptr;

  /* interact with terrain */
  switch (server.terrain[cltouch->y][cltouch->x]) {
  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, cltouch->x, cltouch->y)) LOGFAIL(errno)
    break;

  default:
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLTouch)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclgrabtile(int player) {
  struct CLGrabTile *clgrabtile;
  int pill, base;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLGrabTile)) FAIL(EAGAIN)
  clgrabtile = (struct CLGrabTile *)server.players[player].recvbuf.ptr;

  /* grab pills */
  if ((pill = findpill(clgrabtile->x, clgrabtile->y)) != -1) {
    server.pills[pill].owner = player;
    server.pills[pill].armour = ONBOARD;
    server.pills[pill].speed = MAXTICKSPERSHOT;
    sendsrcapturepill(pill);
  }

  /* grab bases */
  if ((base = findbase(clgrabtile->x, clgrabtile->y)) != -1) {
    if (server.bases[base].owner == NEUTRAL) {
      server.bases[base].owner = player;
      server.bases[base].armour = MAXBASEARMOUR;
      server.bases[base].shells = MAXBASESHELLS;
      server.bases[base].mines = MAXBASEMINES;
      sendsrcapturebase(base);
    }
    else if (
      server.bases[base].owner != NEUTRAL &&
      (
        (server.players[server.bases[base].owner].alliance & (1 << player)) &&
        (server.players[player].alliance & (1 << server.bases[base].owner))
      )
    ) {
      server.bases[base].owner = player;
      sendsrcapturebase(base);
    }
    else {
      server.bases[base].owner = player;
      server.bases[base].armour = 0;
      server.bases[base].shells = 0;
      server.bases[base].mines = 0;
      sendsrcapturebase(base);
    }
  }

  /* interact with terrain */
  switch (server.terrain[clgrabtile->y][clgrabtile->x]) {
  case kBoatTerrain:
    server.terrain[clgrabtile->y][clgrabtile->x] = kRiverTerrain;
    sendsrgrabboat(player, clgrabtile->x, clgrabtile->y);
    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, clgrabtile->x, clgrabtile->y)) LOGFAIL(errno)
    break;

  default:
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLGrabTile)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclgrabtrees(int player) {
  struct CLGrabTrees *clgrabtrees;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLGrabTrees)) FAIL(EAGAIN)
  clgrabtrees = (struct CLGrabTrees *)server.players[player].recvbuf.ptr;

  switch (server.terrain[clgrabtrees->y][clgrabtrees->x]) {
  case kForestTerrain:
    server.terrain[clgrabtrees->y][clgrabtrees->x] = kGrassTerrain3;
    sendsrgrabtrees(clgrabtrees->x, clgrabtrees->y);
    sendsrbuilderack(player, 0, FORRESTTREES, NOPILL);
    break;

  case kMinedForestTerrain:
    server.terrain[clgrabtrees->y][clgrabtrees->x] = kMinedGrassTerrain;
    sendsrgrabtrees(clgrabtrees->x, clgrabtrees->y);
    sendsrbuilderack(player, 0, FORRESTTREES, NOPILL);
    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, clgrabtrees->x, clgrabtrees->y)) LOGFAIL(errno)
    sendsrbuilderack(player, 0, 0, NOPILL);
    break;

  default:
    sendsrbuilderack(player, 0, 0, NOPILL);
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLGrabTrees)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclbuildroad(int player) {
  struct CLBuildRoad *clbuildroad;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLBuildRoad)) FAIL(EAGAIN)
  clbuildroad = (struct CLBuildRoad *)server.players[player].recvbuf.ptr;

  switch (server.terrain[clbuildroad->y][clbuildroad->x]) {
  case kRiverTerrain:
  case kSwampTerrain0:
  case kSwampTerrain1:
  case kSwampTerrain2:
  case kSwampTerrain3:
  case kCraterTerrain:
  case kRubbleTerrain0:
  case kRubbleTerrain1:
  case kRubbleTerrain2:
  case kRubbleTerrain3:
  case kGrassTerrain0:
  case kGrassTerrain1:
  case kGrassTerrain2:
  case kGrassTerrain3:
    if (clbuildroad->trees >= clbuildroad->trees) {
      server.terrain[clbuildroad->y][clbuildroad->x] = kRoadTerrain;
      sendsrbuild(clbuildroad->x, clbuildroad->y);
      sendsrbuilderack(player, 0, clbuildroad->trees - ROADTREES, NOPILL);
    }
    else {
      sendsrbuilderack(player, 0, clbuildroad->trees, NOPILL);
    }

    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
  if (explosionat(player, clbuildroad->x, clbuildroad->y)) LOGFAIL(errno)
    sendsrbuilderack(player, 0, clbuildroad->trees, NOPILL);
    break;

  default:
    sendsrbuilderack(player, 0, clbuildroad->trees, NOPILL);
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLBuildRoad)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclbuildwall(int player) {
  struct CLBuildWall *clbuildwall;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLBuildWall)) FAIL(EAGAIN)
  clbuildwall = (struct CLBuildWall *)server.players[player].recvbuf.ptr;

  switch (server.terrain[clbuildwall->y][clbuildwall->x]) {
  case kSwampTerrain0:
  case kSwampTerrain1:
  case kSwampTerrain2:
  case kSwampTerrain3:
  case kCraterTerrain:
  case kRoadTerrain:
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
    if (clbuildwall->trees >= WALLTREES) {
      server.terrain[clbuildwall->y][clbuildwall->x] = kWallTerrain;
      sendsrbuild(clbuildwall->x, clbuildwall->y);
      sendsrbuilderack(player, 0, clbuildwall->trees - WALLTREES, NOPILL);
    }
    else {
      sendsrbuilderack(player, 0, clbuildwall->trees, NOPILL);
    }

    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, clbuildwall->x, clbuildwall->y)) LOGFAIL(errno)
    sendsrbuilderack(player, 0, clbuildwall->trees, NOPILL);
    break;

  default:
    sendsrbuilderack(player, 0, clbuildwall->trees, NOPILL);
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLBuildWall)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclbuildboat(int player) {
  struct CLBuildBoat *clbuildboat;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLBuildBoat)) FAIL(EAGAIN)
  clbuildboat = (struct CLBuildBoat *)server.players[player].recvbuf.ptr;

  switch (server.terrain[clbuildboat->y][clbuildboat->x]) {
  case kRiverTerrain:
    server.terrain[clbuildboat->y][clbuildboat->x] = kBoatTerrain;
    sendsrbuild(clbuildboat->x, clbuildboat->y);
    sendsrbuilderack(player, 0, clbuildboat->trees - BOATTREES, NOPILL);
    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, clbuildboat->x, clbuildboat->y)) LOGFAIL(errno)
    sendsrbuilderack(player, 0, clbuildboat->trees, NOPILL);
    break;

  default:
    sendsrbuilderack(player, 0, clbuildboat->trees, NOPILL);
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLBuildBoat)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclbuildpill(player) {
  struct CLBuildPill *clbuildpill;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLBuildPill)) FAIL(EAGAIN)
  clbuildpill = (struct CLBuildPill *)server.players[player].recvbuf.ptr;

  if (findpill(clbuildpill->x, clbuildpill->y) == -1 && findbase(clbuildpill->x, clbuildpill->y) == -1) {
    switch (server.terrain[clbuildpill->y][clbuildpill->x]) {
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kCraterTerrain:
    case kRoadTerrain:
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
      server.pills[clbuildpill->pill].x = clbuildpill->x;
      server.pills[clbuildpill->pill].y = clbuildpill->y;
      server.pills[clbuildpill->pill].owner = player;
      server.pills[clbuildpill->pill].armour = clbuildpill->trees*4;

      if (server.pills[clbuildpill->pill].armour > MAXPILLARMOUR) {
        clbuildpill->trees = (server.pills[clbuildpill->pill].armour - MAXPILLARMOUR)/4;
        server.pills[clbuildpill->pill].armour = MAXPILLARMOUR;
      }
      else {
        clbuildpill->trees = 0;
      }

      sendsrbuildpill(clbuildpill->pill);
      sendsrbuilderack(player, 0, clbuildpill->trees, NOPILL);
      break;

    case kMinedSeaTerrain:
    case kMinedSwampTerrain:
    case kMinedCraterTerrain:
    case kMinedRoadTerrain:
    case kMinedForestTerrain:
    case kMinedRubbleTerrain:
    case kMinedGrassTerrain:
      if (explosionat(player, clbuildpill->x, clbuildpill->y)) LOGFAIL(errno)
      sendsrbuilderack(player, 0, clbuildpill->trees, NOPILL);
      break;

    default:
      sendsrbuilderack(player, 0, clbuildpill->trees, NOPILL);
      break;
    }
  }
  else {
    sendsrbuilderack(player, 0, clbuildpill->trees, NOPILL);
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLBuildPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclrepairpill(int player) {
  struct CLRepairPill *clrepairpill;
  int pill;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLRepairPill)) FAIL(EAGAIN)
  clrepairpill = (struct CLRepairPill *)server.players[player].recvbuf.ptr;

  if ((pill = findpill(clrepairpill->x, clrepairpill->y)) != -1 && findbase(clrepairpill->x, clrepairpill->y) == -1) {
    switch (server.terrain[clrepairpill->y][clrepairpill->x]) {
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
    case kCraterTerrain:
    case kRoadTerrain:
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
      server.pills[pill].armour += clrepairpill->trees*4;

      if (server.pills[pill].armour > MAXPILLARMOUR) {
        clrepairpill->trees = (server.pills[pill].armour - MAXPILLARMOUR)/4;
        server.pills[pill].armour = MAXPILLARMOUR;
      }
      else {
        clrepairpill->trees = 0;
      }

      sendsrrepairpill(pill);
      sendsrbuilderack(player, 0, clrepairpill->trees, NOPILL);
      break;

    case kMinedSeaTerrain:
    case kMinedSwampTerrain:
    case kMinedCraterTerrain:
    case kMinedRoadTerrain:
    case kMinedForestTerrain:
    case kMinedRubbleTerrain:
    case kMinedGrassTerrain:
      if (explosionat(player, clrepairpill->x, clrepairpill->y)) LOGFAIL(errno)
      sendsrbuilderack(player, 0, clrepairpill->trees, NOPILL);
      break;

    default:
      sendsrbuilderack(player, 0, clrepairpill->trees, NOPILL);
      break;
    }
  }
  else {
    sendsrbuilderack(player, 0, clrepairpill->trees, NOPILL);
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLRepairPill)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclplacemine(int player) {
  struct CLPlaceMine *clplacemine;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLPlaceMine)) FAIL(EAGAIN)
  clplacemine = (struct CLPlaceMine *)server.players[player].recvbuf.ptr;

  switch (server.terrain[clplacemine->y][clplacemine->x]) {
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
    switch (server.terrain[clplacemine->y][clplacemine->x]) {
    case kSwampTerrain0:
    case kSwampTerrain1:
    case kSwampTerrain2:
    case kSwampTerrain3:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedSwampTerrain;

      break;

    case kCraterTerrain:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedCraterTerrain;

      break;

    case kRoadTerrain:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedRoadTerrain;

      break;

    case kForestTerrain:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedForestTerrain;

      break;

    case kRubbleTerrain0:
    case kRubbleTerrain1:
    case kRubbleTerrain2:
    case kRubbleTerrain3:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedRubbleTerrain;

      break;

    case kGrassTerrain0:
    case kGrassTerrain1:
    case kGrassTerrain2:
    case kGrassTerrain3:
      server.terrain[clplacemine->y][clplacemine->x] = kMinedGrassTerrain;

      break;

    default:
      break;
    }

    sendsrplacemine(player, clplacemine->x, clplacemine->y);
    sendsrbuilderack(player, 0, 0, NOPILL);
    break;

  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(player, clplacemine->x, clplacemine->y)) LOGFAIL(errno)
    sendsrbuilderack(player, 0, 0, NOPILL);
    break;

  default:
    sendsrbuilderack(player, 0, 0, NOPILL);
    break;
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLPlaceMine)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvcldamage(int player) {
  struct CLDamage *cldamage;
  int pill, base, i;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLDamage)) FAIL(EAGAIN)
  cldamage = (struct CLDamage *)server.players[player].recvbuf.ptr;

  if ((pill = findpill(cldamage->x, cldamage->y)) != -1) {
    if (server.pills[pill].armour > 0) {
      /* heat pill */
      server.pills[pill].armour--;
      server.pills[pill].speed /= 2;
      server.pills[pill].speed = MAX(server.pills[pill].speed, MINTICKSPERSHOT);
      server.pills[pill].counter = 0;
    }

    sendsrdamage(player, cldamage->x, cldamage->y);
  }
  else {
    if ((base = findbase(cldamage->x, cldamage->y)) != -1) {
      if (server.bases[base].armour >= MINBASEARMOUR) {
        server.bases[base].armour -= MINBASEARMOUR;
        server.bases[base].counter = 0;

        /* heat pills */
        for (i = 0; i < server.npills; i++) {
          if (
            mag2f(sub2f(make2f(server.pills[i].x + 0.5, server.pills[i].y + 0.5), make2f(server.bases[base].x + 0.5, server.bases[base].y + 0.5))) <= 8.0 &&
            server.pills[i].owner != NEUTRAL && server.bases[base].owner != NEUTRAL &&
            (
              (server.players[server.pills[i].owner].alliance & (1 << server.bases[base].owner)) &&
              (server.players[server.bases[base].owner].alliance & (1 << server.pills[i].owner))
            )
          ) {
            server.pills[i].speed /= 2;
            server.pills[pill].speed = MAX(server.pills[pill].speed, MINTICKSPERSHOT);
            server.pills[i].counter = 0;
          }
        }
      }

      sendsrdamage(player, cldamage->x, cldamage->y);
    }
    else {
      if (cldamage->boat) {
        switch (server.terrain[cldamage->y][cldamage->x]) {
        case kBoatTerrain:
          server.terrain[cldamage->y][cldamage->x] = kRiverTerrain;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kWallTerrain:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kSwampTerrain0:
          server.terrain[cldamage->y][cldamage->x] = kRiverTerrain;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kSwampTerrain1:
          server.terrain[cldamage->y][cldamage->x] = kSwampTerrain0;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kSwampTerrain2:
          server.terrain[cldamage->y][cldamage->x] = kSwampTerrain1;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kSwampTerrain3:
          server.terrain[cldamage->y][cldamage->x] = kSwampTerrain2;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kRoadTerrain:
          if (
            (isWaterLikeTerrain(server.terrain[cldamage->y][cldamage->x - 1]) && isWaterLikeTerrain(server.terrain[cldamage->y][cldamage->x + 1])) ||
            (isWaterLikeTerrain(server.terrain[cldamage->y - 1][cldamage->x]) && isWaterLikeTerrain(server.terrain[cldamage->y + 1][cldamage->x]))
          ) {
            server.terrain[cldamage->y][cldamage->x] = kRiverTerrain;
          }
          sendsrdamage(player, cldamage->x, cldamage->y);

          break;

        case kForestTerrain:
          server.terrain[cldamage->y][cldamage->x] = kGrassTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kRubbleTerrain0:
          server.terrain[cldamage->y][cldamage->x] = kRiverTerrain;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kRubbleTerrain1:
          server.terrain[cldamage->y][cldamage->x] = kRubbleTerrain0;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kRubbleTerrain2:
          server.terrain[cldamage->y][cldamage->x] = kRubbleTerrain1;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kRubbleTerrain3:
          server.terrain[cldamage->y][cldamage->x] = kRubbleTerrain2;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kGrassTerrain0:
          server.terrain[cldamage->y][cldamage->x] = kSwampTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kGrassTerrain1:
          server.terrain[cldamage->y][cldamage->x] = kGrassTerrain0;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kGrassTerrain2:
          server.terrain[cldamage->y][cldamage->x] = kGrassTerrain1;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kGrassTerrain3:
          server.terrain[cldamage->y][cldamage->x] = kGrassTerrain2;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain0:
          server.terrain[cldamage->y][cldamage->x] = kRubbleTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain1:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain0;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain2:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain1;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain3:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain2;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kMinedSeaTerrain:
        case kMinedSwampTerrain:
        case kMinedCraterTerrain:
        case kMinedRoadTerrain:
        case kMinedForestTerrain:
        case kMinedRubbleTerrain:
        case kMinedGrassTerrain:
          if (explosionat(player, cldamage->x, cldamage->y)) LOGFAIL(errno)
          break;

        default:
          break;
        }
      }
      else {
        switch (server.terrain[cldamage->y][cldamage->x]) {
        case kBoatTerrain:
          server.terrain[cldamage->y][cldamage->x] = kRiverTerrain;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kWallTerrain:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kForestTerrain:
          server.terrain[cldamage->y][cldamage->x] = kGrassTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain0:
          server.terrain[cldamage->y][cldamage->x] = kRubbleTerrain3;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain1:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain0;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain2:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain1;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kDamagedWallTerrain3:
          server.terrain[cldamage->y][cldamage->x] = kDamagedWallTerrain2;
          sendsrdamage(player, cldamage->x, cldamage->y);
          break;

        case kMinedSeaTerrain:
        case kMinedSwampTerrain:
        case kMinedCraterTerrain:
        case kMinedRoadTerrain:
        case kMinedForestTerrain:
        case kMinedRubbleTerrain:
        case kMinedGrassTerrain:
          if (explosionat(player, cldamage->x, cldamage->y)) LOGFAIL(errno)
          break;

        default:
          break;
        }
      }
    }
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLDamage)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclsmallboom(int player) {
  struct CLSmallBoom *clsmallboom;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLSmallBoom)) FAIL(EAGAIN)
  clsmallboom = (struct CLSmallBoom *)server.players[player].recvbuf.ptr;

  if (explosionat(player, clsmallboom->x, clsmallboom->y)) LOGFAIL(errno)

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLSmallBoom)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclsuperboom(int player) {
  struct CLSuperBoom *clsuperboom;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLSuperBoom)) FAIL(EAGAIN)
  clsuperboom = (struct CLSuperBoom *)server.players[player].recvbuf.ptr;

  if (superboomat(player, clsuperboom->x, clsuperboom->y)) LOGFAIL(errno)

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLSuperBoom)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclrefuel(int player) {
  struct CLRefuel *clrefuel;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLRefuel)) FAIL(EAGAIN)
  clrefuel = (struct CLRefuel *)server.players[player].recvbuf.ptr;

  if (clrefuel->base < server.nbases) {
    server.bases[clrefuel->base].armour -= clrefuel->armour;
    server.bases[clrefuel->base].shells -= clrefuel->shells;
    server.bases[clrefuel->base].mines -= clrefuel->mines;
    sendsrrefuel(player, clrefuel->base, clrefuel->armour, clrefuel->shells, clrefuel->mines);
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLRefuel)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclhittank(int player) {
  struct CLHitTank *clhittank;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLHitTank)) FAIL(EAGAIN)
  clhittank = (struct CLHitTank *)server.players[player].recvbuf.ptr;

  if (clhittank->player < MAXPLAYERS) {
    sendsrhittank(clhittank->player, clhittank->dir);
  }

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLHitTank)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int recvclsetalliance(int player) {
  struct CLSetAlliance *clsettalliance;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].recvbuf.nbytes < sizeof(struct CLSetAlliance)) FAIL(EAGAIN)
  clsettalliance = (struct CLSetAlliance *)server.players[player].recvbuf.ptr;

  /* convert byte order*/
  clsettalliance->alliance = ntohs(clsettalliance->alliance);

  server.players[player].alliance = clsettalliance->alliance;
  sendsrsetalliance(player, clsettalliance->alliance);

  /* clear buffer of read data */
  if (readbuf(&server.players[player].recvbuf, NULL, sizeof(struct CLSetAlliance)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrsendmesg(int player, int to, uint16_t mask, const char *text) {
  int i;
  struct SRSendMesg srsendmesg;
  size_t len;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(text != NULL);

TRY
  srsendmesg.type = SRSENDMESG;
  srsendmesg.player = player;
  srsendmesg.to = to;
  len = strlen(text) + 1;

  for (i = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].cntlsock != -1 && (mask & (1 << i))) {
      if (writebuf(&server.players[i].sendbuf, &srsendmesg, sizeof(srsendmesg)) == -1) LOGFAIL(errno)
      if (writebuf(&server.players[i].sendbuf, text, len) == -1) LOGFAIL(errno)
//      if (sendplayerbufserver(i)) LOGFAIL(errno)
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrdamage(int player, int x, int y) {
  struct SRDamage srdamage;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srdamage.type = SRDAMAGE;
  srdamage.player = player;
  srdamage.x = x;
  srdamage.y = y;
  srdamage.terrain = server.terrain[y][x];

  if (sendtoall(&srdamage, sizeof(srdamage))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrgrabtrees(int x, int y) {
  struct SRGrabTrees srgrabtrees;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srgrabtrees.type = SRGRABTREES;
  srgrabtrees.x = x;
  srgrabtrees.y = y;

  if (sendtoall(&srgrabtrees, sizeof(srgrabtrees))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrbuild(int x, int y) {
  struct SRBuild srbuild;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srbuild.type = SRBUILD;
  srbuild.x = x;
  srbuild.y = y;

  srbuild.terrain = server.terrain[y][x];

  if (sendtoall(&srbuild, sizeof(srbuild))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrgrow(int x, int y) {
  struct SRGrow srgrow;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srgrow.type = SRGROW;
  srgrow.x = x;
  srgrow.y = y;

  if (sendtoall(&srgrow, sizeof(srgrow))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrflood(int x, int y) {
  struct SRFlood srflood;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srflood.type = SRFLOOD;
  srflood.x = x;
  srflood.y = y;

  if (sendtoall(&srflood, sizeof(srflood))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplacemine(int player, int x, int y) {
  struct SRPlaceMine srplacemine;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srplacemine.type = SRPLACEMINE;
  srplacemine.player = player;
  srplacemine.x = x;
  srplacemine.y = y;

  if (sendtoall(&srplacemine, sizeof(srplacemine))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrdropmine(int player, int x, int y) {
  struct SRDropMine srdropmine;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srdropmine.type = SRDROPMINE;
  srdropmine.player = player;
  srdropmine.x = x;
  srdropmine.y = y;

  if (sendtoall(&srdropmine, sizeof(srdropmine))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrdropboat(int x, int y) {
  struct SRDropBoat srdropboat;

  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srdropboat.type = SRDROPBOAT;
  srdropboat.x = x;
  srdropboat.y = y;

  if (sendtoall(&srdropboat, sizeof(srdropboat))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerjoin(int player) {
  struct SRPlayerJoin srplayerjoin;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerjoin.type = SRPLAYERJOIN;
  srplayerjoin.player = player;
  bzero(srplayerjoin.name, sizeof(srplayerjoin.name));
  strncpy(srplayerjoin.name, server.players[player].name, sizeof(srplayerjoin.name) - 1);
  bzero(srplayerjoin.host, sizeof(srplayerjoin.host));
  strncpy(srplayerjoin.host, server.players[player].host, sizeof(srplayerjoin.host) - 1);

  if (sendtoall(&srplayerjoin, sizeof(srplayerjoin))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerrejoin(int player) {
  struct SRPlayerRejoin srplayerrejoin;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerrejoin.type = SRPLAYERREJOIN;
  srplayerrejoin.player = player;
  bzero(srplayerrejoin.host, sizeof(srplayerrejoin.host));
  strncpy(srplayerrejoin.host, server.players[player].host, sizeof(srplayerrejoin.host) - 1);

  if (sendtoall(&srplayerrejoin, sizeof(srplayerrejoin))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerexit(int player) {
  struct SRPlayerExit srplayerexit;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerexit.type = SRPLAYEREXIT;
  srplayerexit.player = player;

  if (sendtoone(&srplayerexit, sizeof(srplayerexit), player)) {
    if (errno != EPIPE) {
      LOGFAIL(errno)
    }
    else {
      CLEARERRLOG
    }
  }

  if (sendtoallex(&srplayerexit, sizeof(srplayerexit), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerdisc(int player) {
  struct SRPlayerDisc srplayerdisc;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerdisc.type = SRPLAYERDISC;
  srplayerdisc.player = player;

  if (sendtoallex(&srplayerdisc, sizeof(srplayerdisc), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerkick(int player) {
  struct SRPlayerKick srplayerkick;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerkick.type = SRPLAYERKICK;
  srplayerkick.player = player;

  if (sendtoall(&srplayerkick, sizeof(srplayerkick))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrplayerban(int player) {
  struct SRPlayerBan srplayerban;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srplayerban.type = SRPLAYERBAN;
  srplayerban.player = player;

  if (sendtoall(&srplayerban, sizeof(srplayerban))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrpause(int pause) {
  struct SRPause srpause;

  assert(pause >= 0);
  assert(pause < 256);

TRY
  srpause.type = SRPAUSE;
  srpause.pause = pause;

  if (sendtoall(&srpause, sizeof(srpause))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrrepairpill(int pill) {
  struct SRRepairPill srrepairpill;

  assert(pill >= 0);
  assert(pill < server.npills);
  assert(server.pills[pill].armour != ONBOARD);

TRY
  srrepairpill.type = SRREPAIRPILL;
  srrepairpill.pill = pill;
  srrepairpill.armour = server.pills[pill].armour;

  if (sendtoall(&srrepairpill, sizeof(srrepairpill))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrcoolpill(int pill) {
  struct SRCoolPill srcoolpill;

  assert(pill >= 0);
  assert(pill < server.npills);
  assert(server.pills[pill].armour != ONBOARD);

TRY
  srcoolpill.type = SRCOOLPILL;
  srcoolpill.pill = pill;

  if (sendtoall(&srcoolpill, sizeof(srcoolpill))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrcapturepill(int pill) {
  struct SRCapturePill srcapturepill;

  assert(pill >= 0);
  assert(pill < server.npills);
  assert(server.pills[pill].armour == ONBOARD);

TRY
  srcapturepill.type = SRCAPTUREPILL;
  srcapturepill.pill = pill;
  srcapturepill.owner = server.pills[pill].owner;

  if (sendtoall(&srcapturepill, sizeof(srcapturepill))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrbuildpill(int pill) {
  struct SRBuildPill srbuildpill;

  assert(pill >= 0);
  assert(pill < server.npills);
  assert(server.pills[pill].armour != ONBOARD);

TRY
  srbuildpill.type = SRBUILDPILL;
  srbuildpill.pill = pill;
  srbuildpill.x = server.pills[pill].x;
  srbuildpill.y = server.pills[pill].y;
  srbuildpill.armour = server.pills[pill].armour;

  if (sendtoall(&srbuildpill, sizeof(srbuildpill))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrdroppill(int pill) {
  struct SRDropPill srdroppill;

  assert(pill >= 0);
  assert(pill < server.npills);
  assert(server.pills[pill].armour != ONBOARD);

TRY
  srdroppill.type = SRDROPPILL;
  srdroppill.pill = pill;
  srdroppill.x = server.pills[pill].x;
  srdroppill.y = server.pills[pill].y;

  if (sendtoall(&srdroppill, sizeof(srdroppill))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrreplenishbase(int base) {
  struct SRReplenishBase srreplenishbase;

  assert(base >= 0);
  assert(base < server.nbases);

TRY
  srreplenishbase.type = SRREPLENISHBASE;
  srreplenishbase.base = base;

  if (sendtoall(&srreplenishbase, sizeof(srreplenishbase))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrcapturebase(int base) {
  struct SRCaptureBase srcaputurebase;

  assert(base >= 0);
  assert(base < server.nbases);

TRY
  srcaputurebase.type = SRCAPTUREBASE;
  srcaputurebase.base = base;
  srcaputurebase.owner = server.bases[base].owner;

  if (sendtoall(&srcaputurebase, sizeof(srcaputurebase))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrrefuel(int player, int base, int armour, int shells, int mines) {
  struct SRRefuel srrefuel;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(server.players[player].cntlsock != -1);
  assert(base >= 0);
  assert(base < server.nbases);
  assert(armour >= 0);
  assert(armour < 256);
  assert(shells >= 0);
  assert(shells < 256);
  assert(mines >= 0);
  assert(mines < 256);

TRY
  srrefuel.type = SRREFUEL;
  srrefuel.base = base;
  srrefuel.armour = armour;
  srrefuel.shells = shells;
  srrefuel.mines = mines;

  if (sendtoallex(&srrefuel, sizeof(srrefuel), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrgrabboat(int player, int x, int y) {
  struct SRGrabBoat srgrabboat;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

TRY
  srgrabboat.type = SRGRABBOAT;
  srgrabboat.player = player;
  srgrabboat.x = x;
  srgrabboat.y = y;

  if (sendtoall(&srgrabboat, sizeof(srgrabboat))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrmineack(int player, int success) {
  struct SRMineAck srmineack;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srmineack.type = SRMINEACK;
  srmineack.success = success != 0;

  if (sendtoone(&srmineack, sizeof(srmineack), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrbuilderack(int player, int mines, int trees, int pill) {
  struct SRBuilderAck srbuilderack;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(mines >= 0);
  assert(mines < 256);
  assert(trees >= 0);
  assert(trees < 256);
  assert(mines >= 0);
  assert(pill >= 0);
  assert(pill < server.npills || pill == 255);

TRY
  srbuilderack.type = SRBUILDERACK;
  srbuilderack.mines = mines;
  srbuilderack.trees = trees;
  srbuilderack.pill = pill;

  if (sendtoone(&srbuilderack, sizeof(srbuilderack), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrsmallboom(int player, int x, int y) {
  struct SRSmallBoom srsmallboom;

  assert(player >= 0);
  assert(player < MAXPLAYERS || player == NEUTRAL);
  assert(x >= 0);
  assert(x < 256);
  assert(y >= 0);
  assert(y < 256);

TRY
  srsmallboom.type = SRSMALLBOOM;
  srsmallboom.player = player;
  srsmallboom.x = x;
  srsmallboom.y = y;

  if (sendtoall(&srsmallboom, sizeof(srsmallboom))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrsuperboom(int player, int x, int y) {
  struct SRSuperBoom srsuperboom;

  assert(player >= 0);
  assert(player < MAXPLAYERS);
  assert(x >= 0);
  assert(x < 256);
  assert(y >= 0);
  assert(y < 256);

TRY
  srsuperboom.type = SRSUPERBOOM;
  srsuperboom.player = player;
  srsuperboom.x = x;
  srsuperboom.y = y;

  if (sendtoall(&srsuperboom, sizeof(srsuperboom))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrhittank(int player, uint32_t dir) {
  struct SRHitTank srhittank;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srhittank.type = SRHITTANK;
  srhittank.player = player;
  srhittank.dir = dir;

  if (sendtoone(&srhittank, sizeof(srhittank), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrsetalliance(int player, uint16_t alliance) {
  struct SRSetAlliance srsetalliance;

  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  srsetalliance.type = SRSETALLIANCE;
  srsetalliance.player = player;
  srsetalliance.alliance = htons(alliance);

  if (sendtoallex(&srsetalliance, sizeof(srsetalliance), player)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrtimelimit(uint16_t timeremaining) {
  struct SRTimeLimit srtimelimit;

TRY
  srtimelimit.type = SRTIMELIMIT;
  srtimelimit.timeremaining = htons(timeremaining);
  
  if (sendtoall(&srtimelimit, sizeof(srtimelimit))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendsrbasecontrol(uint16_t timeleft) {
  struct SRBaseControl srbasecontrol;

TRY
  srbasecontrol.type = SRBASECONTROL;
  srbasecontrol.timeleft = htons(timeleft);
  
  if (sendtoall(&srbasecontrol, sizeof(srbasecontrol))) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendtoall(const void *data, size_t nbytes) {
  int i;

  assert(data != NULL);

TRY
  for (i = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].cntlsock != -1) {
      if (writebuf(&server.players[i].sendbuf, data, nbytes) == -1) LOGFAIL(errno)
//      if (sendplayerbufserver(i)) LOGFAIL(errno)
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendtoallex(const void *data, size_t nbytes, int player) {
  int i;

  assert(data != NULL);
  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  for (i = 0; i < MAXPLAYERS; i++) {
    if (player != i && server.players[i].cntlsock != -1) {
      if (writebuf(&server.players[i].sendbuf, data, nbytes) == -1) LOGFAIL(errno)
//      if (sendplayerbufserver(i)) LOGFAIL(errno)
    }
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendtoone(const void *data, size_t nbytes, int player) {
  assert(data != NULL);
  assert(player >= 0);
  assert(player < MAXPLAYERS);

TRY
  if (server.players[player].cntlsock != -1) {
    if (writebuf(&server.players[player].sendbuf, data, nbytes) == -1) LOGFAIL(errno)
//    if (sendplayerbufserver(player)) LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int basescore(int x, int y) {
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  if (findpill(x, y) != -1 || findbase(x, y) != -1) {
    return 0;
  }

  switch (server.terrain[y][x]) {
  case kGrassTerrain0:
  case kGrassTerrain1:
  case kGrassTerrain2:
  case kGrassTerrain3:
  case kMinedGrassTerrain:
    return 5;

  case kSwampTerrain0:
  case kSwampTerrain1:
  case kSwampTerrain2:
  case kSwampTerrain3:
  case kMinedSwampTerrain:
    return 4;

  case kCraterTerrain:
  case kMinedCraterTerrain:
    return 3;

  case kRubbleTerrain0:
  case kRubbleTerrain1:
  case kRubbleTerrain2:
  case kRubbleTerrain3:
  case kMinedRubbleTerrain:
    return 2;

  case kRoadTerrain:
  case kMinedRoadTerrain:
    return 1;

  default:
    return 0;
  }
}

int adjacentscore(int x, int y) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
    return 0;
  }
  else {
    switch (server.terrain[y][x]) {
    case kForestTerrain:
    case kMinedForestTerrain:
      return 1;

    default:
      return 0;
    }
  }
}

int treescore(int x, int y) {
  assert(x >= 0);
  assert(x < WIDTH);
  assert(y >= 0);
  assert(y < WIDTH);

  return
      basescore(x, y)*(
        2*(adjacentscore(x + 1, y) + adjacentscore(x - 1, y) + adjacentscore(x, y + 1) + adjacentscore(x, y - 1)) +
        adjacentscore(x - 1, y - 1) + adjacentscore(x + 1, y - 1) + adjacentscore(x - 1, y + 1) + adjacentscore(x + 1, y + 1)
      );
}

void growtrees(int nplayers) {
  int i;
  int x, y;
  
  for (i = 0; i < nplayers*(TREESBESTOF/(TREESPLANTRATE*TICKSPERSEC)); i++) {
    x = random()%(WIDTH*WIDTH);
    y = x/WIDTH;
    x %= WIDTH;

    if (treescore(server.growx, server.growy) < treescore(x, y)) {
      server.growx = x;
      server.growy = y;
    }

    server.growbestof++;

    if (server.growbestof >= TREESBESTOF) {
      server.growbestof = 0;

      /* if clear grow trees */
      if (findpill(x, y) == -1 && findbase(x, y) == -1) {
        switch (server.terrain[server.growy][server.growx]) {
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
          if (findpill(server.growx, server.growy) == -1 && findbase(server.growx, server.growy) == -1) {
            server.terrain[server.growy][server.growx] = kForestTerrain;
            sendsrgrow(server.growx, server.growy);
          }

          break;

        case kMinedGrassTerrain:
        case kMinedRubbleTerrain:
        case kMinedCraterTerrain:
        case kMinedSwampTerrain:
        case kMinedRoadTerrain:
          if (findpill(server.growx, server.growy) == -1 && findbase(server.growx, server.growy) == -1) {
            server.terrain[server.growy][server.growx] = kMinedForestTerrain;
            sendsrgrow(server.growx, server.growy);
          }

          break;

        default:
          break;
        }
      }

      /* begin new search */
      server.growx = random()%(WIDTH*WIDTH);
      server.growy = server.growx/WIDTH;
      server.growx %= WIDTH;
    }
  }
}

int chainat(int x, int y) {
  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  switch (server.terrain[y][x]) {
  case kMinedSeaTerrain:
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(NEUTRAL, x, y)) LOGFAIL(errno)
    break;

  default:
    break;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

int floodat(int x, int y) {
  Pointi *flood = NULL;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  switch (server.terrain[y][x]) {
  case kMinedSwampTerrain:
  case kMinedCraterTerrain:
  case kMinedRoadTerrain:
  case kMinedForestTerrain:
  case kMinedRubbleTerrain:
  case kMinedGrassTerrain:
    if (explosionat(NEUTRAL, x, y)) LOGFAIL(errno)
    break;

  case kCraterTerrain:
    server.terrain[y][x] = kRiverTerrain;
    sendsrflood(x, y);
    if ((flood = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
    flood->x = x;
    flood->y = y;
    if (addlist(server.floods + (server.ticks - 1)%(FLOODTICKS + 1), flood)) LOGFAIL(errno)
    flood = NULL;
    break;

  default:
    break;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (flood != NULL) {
      free(flood);
    }

    RETERR(-1)
  }
END
}

int floodtest(int x, int y) {
  Pointi *flood;

  flood = NULL;
  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  switch (server.terrain[y][x]) {
  case kRiverTerrain:
  case kSeaTerrain:
  case kMinedSeaTerrain:
  case kBoatTerrain:
    if ((flood = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
    flood->x = x;
    flood->y = y;
    if (addlist(server.floods + (server.ticks - 1)%(FLOODTICKS + 1), flood)) LOGFAIL(errno)
    flood = NULL;
    break;

  default:
    break;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (flood != NULL) {
      free(flood);
    }

    RETERR(-1);
  }
END
}

static int explosionat(int player, int x, int y) {
  Pointi *chain = NULL;

  assert(((player >= 0 && player < MAXPLAYERS) || player == NEUTRAL) && x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  switch (server.terrain[y][x]) {
  case kBoatTerrain:
  case kWallTerrain:
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
    server.terrain[y][x] = kCraterTerrain;
    if (floodtest(x, y - 1)) LOGFAIL(errno)
    if (floodtest(x - 1, y)) LOGFAIL(errno)
    if (floodtest(x + 1, y)) LOGFAIL(errno)
    if (floodtest(x, y + 1)) LOGFAIL(errno)
    if ((chain = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
    chain->x = x;
    chain->y = y;
    if (addlist(server.chains + (server.ticks - 1)%(CHAINTICKS + 1), chain)) LOGFAIL(errno)
    chain = NULL;
    sendsrsmallboom(NEUTRAL, x, y);
    break;

  case kMinedSeaTerrain:
    sendsrsmallboom(NEUTRAL, x, y);
    break;

  default:
    break;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (chain != NULL) {
      free(chain);
    }

    RETERR(-1)
  }
END
}

static int superboomat(int player, int x, int y) {
  Pointi *chain = NULL;

  assert(player >= 0 && player < MAXPLAYERS && x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

TRY
  /* turn terrain to crater */
  if (server.terrain[y][x] != kSeaTerrain && server.terrain[y][x] != kMinedSeaTerrain) {
    server.terrain[y][x] = kCraterTerrain;
  }
  if (server.terrain[y][x + 1] != kSeaTerrain && server.terrain[y][x + 1] != kMinedSeaTerrain) {
    server.terrain[y][x + 1] = kCraterTerrain;
  }
  if (server.terrain[y + 1][x] != kSeaTerrain && server.terrain[y + 1][x] != kMinedSeaTerrain) {
    server.terrain[y + 1][x] = kCraterTerrain;
  }
  if (server.terrain[y + 1][x + 1] != kSeaTerrain && server.terrain[y + 1][x + 1] != kMinedSeaTerrain) {
    server.terrain[y + 1][x + 1] = kCraterTerrain;
  }

  /* begin flood test */
  if (floodtest(x, y - 1)) LOGFAIL(errno)
  if (floodtest(x + 1, y - 1)) LOGFAIL(errno)
  if (floodtest(x - 1, y)) LOGFAIL(errno)
  if (floodtest(x - 1, y + 1)) LOGFAIL(errno)
  if (floodtest(x + 2, y)) LOGFAIL(errno)
  if (floodtest(x + 2, y + 1)) LOGFAIL(errno)
  if (floodtest(x, y + 2)) LOGFAIL(errno)
  if (floodtest(x + 1, y + 2)) LOGFAIL(errno)

  /* begin chain explosions */
  if ((chain = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
  chain->x = x;
  chain->y = y;
  if (addlist(server.chains + (server.ticks - 1)%(CHAINTICKS + 1), chain)) LOGFAIL(errno)
  if ((chain = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
  chain->x = x + 1;
  chain->y = y;
  if (addlist(server.chains + (server.ticks - 1)%(CHAINTICKS + 1), chain)) LOGFAIL(errno)
  if ((chain = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
  chain->x = x;
  chain->y = y + 1;
  if (addlist(server.chains + (server.ticks - 1)%(CHAINTICKS + 1), chain)) LOGFAIL(errno)
  if ((chain = (Pointi *)malloc(sizeof(Pointi))) == NULL) LOGFAIL(errno)
  chain->x = x + 1;
  chain->y = y + 1;
  if (addlist(server.chains + (server.ticks - 1)%(CHAINTICKS + 1), chain)) LOGFAIL(errno)
  chain = NULL;

  /* send superboom to clients */
  sendsrsuperboom(player, x, y);

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (chain) {
      free(chain);
    }

    RETERR(-1)
  }
END
}

int chain() {
  struct ListNode *node;

TRY
  for (node = nextlist(server.chains + server.ticks%(CHAINTICKS + 1)); node != NULL; node = nextlist(node)) {
    Pointi *chain;

    chain = ptrlist(node);
    if (chainat(chain->x, chain->y - 1)) LOGFAIL(errno)
    if (chainat(chain->x - 1, chain->y)) LOGFAIL(errno)
    if (chainat(chain->x + 1, chain->y)) LOGFAIL(errno)
    if (chainat(chain->x, chain->y + 1)) LOGFAIL(errno)
  }

  clearlist(server.chains + server.ticks%(CHAINTICKS + 1), free);

CLEANUP
ERRHANDLER(0, -1)
END
}

int flood() {
  struct ListNode *node;

TRY
  for (node = nextlist(server.floods + server.ticks%(FLOODTICKS + 1)); node != NULL; node = nextlist(node)) {
    Pointi *flood;

    flood = ptrlist(node);
    if (floodat(flood->x, flood->y - 1)) LOGFAIL(errno)
    if (floodat(flood->x - 1, flood->y)) LOGFAIL(errno)
    if (floodat(flood->x + 1, flood->y)) LOGFAIL(errno)
    if (floodat(flood->x, flood->y + 1)) LOGFAIL(errno)
  }

  clearlist(server.floods + server.ticks%(FLOODTICKS + 1), free);

CLEANUP
ERRHANDLER(0, -1)
END
}

int tiletoterrain(int tile) {
  switch (tile) {
  case kWallTile:  /* wall */
    return kWallTerrain;

  case kRiverTile:  /* river */
    return kRiverTerrain;

  case kSwampTile:  /* swamp */
    return kSwampTerrain3;

  case kCraterTile:  /* crater */
    return kCraterTerrain;

  case kRoadTile:  /* road */
    return kRoadTerrain;

  case kForestTile:  /* forest */
    return kForestTerrain;

  case kRubbleTile:  /* rubble */
    return kRubbleTerrain3;

  case kGrassTile:  /* grass */
    return kGrassTerrain3;

  case kDamagedWallTile:  /* damaged wall */
    return kDamagedWallTerrain3;

  case kBoatTile:  /* river w/boat */
    return kBoatTerrain;

  case kMinedSwampTile:  /* mined swamp */
    return kMinedSwampTerrain;

  case kMinedCraterTile:  /* mined crater */
    return kMinedCraterTerrain;

  case kMinedRoadTile:  /* mined road */
    return kMinedRoadTerrain;

  case kMinedForestTile:  /* mined forest */
    return kMinedForestTerrain;

  case kMinedRubbleTile:  /* mined rubble */
    return kMinedRubbleTerrain;

  case kMinedGrassTile:  /* mined grass */
    return kMinedGrassTerrain;

  case kSeaTile:  /* sea */
    return kSeaTerrain;

  case kMinedSeaTile:  /* mined sea */
    return kMinedSeaTerrain;

  case kUnknownTile:  /* unknown */
    return kMinedSeaTerrain;

  default:
    assert(0);
    return -1;
  }
}

int findpill(int x, int y) {
  int i;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  for (i = 0; i < server.npills; i++) {
    if (server.pills[i].armour != ONBOARD && server.pills[i].x == x && server.pills[i].y == y) {
      return i;
    }
  }

  return -1;
}

int findbase(int x, int y) {
  int i;

  assert(x >= 0 && x < WIDTH && y >= 0 && y < WIDTH);

  for (i = 0; i < server.nbases; i++) {
    if (server.bases[i].x == x && server.bases[i].y == y) {
      return i;
    }
  }

  return -1;
}

int getservertcpport() {
  struct sockaddr_in addr;
  socklen_t addrlen;

  addrlen = INET_ADDRSTRLEN;
  getsockname(server.listensock, (void *)&addr, &addrlen);
  return ntohs(addr.sin_port);
}

int getserverudpport() {
  struct sockaddr_in addr;
  socklen_t addrlen;

  addrlen = INET_ADDRSTRLEN;
  getsockname(server.dgramsock, (void *)&addr, &addrlen);
  return ntohs(addr.sin_port);
}

int nplayers() {
  int i, nplayers;

  for (i = 0, nplayers = 0; i < MAXPLAYERS; i++) {
    if (server.players[i].cntlsock != -1) {
      nplayers++;
    }
  }

  return nplayers;
}
