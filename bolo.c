/*
 *  bolo.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/1/04.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "bolo.h"
#include "server.h"
#include "client.h"
#include "terrain.h"
#include "tracker.h"
#include "bmap.h"
#include "io.h"
#include "rect.h"
#include "list.h"
#include "resolver.h"
#include "errchk.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>
#include <assert.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#define ISFOG(x, y)     (((x) < 0 || (x) >= WIDTH || (y) < 0 || (y) >= WIDTH) ? 1 : (client.fog[(y)][(x)] == 0))

struct TrackerThreadInfo {
  char *hostname;
  struct ListNode *node;
  void (*trackerstatus)(int);
  int fd;
} ;

static struct {
  pthread_mutex_t mutex;
  int sock;
} tracker;

static void *trackerthread(struct TrackerThreadInfo *trackerthreadinfo);

int allowjoinserver() {
  int ret;
  int gotlock = 0;

TRY
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  ret = server.allowjoin;

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
ERRHANDLER(ret, -1)
END
}
  
int initbolo(void (*setplayerstatusfunc)(int player), void (*setpillstatusfunc)(int pill), void (*setbasestatusfunc)(int base), void (*settankstatusfunc)(), void (*playsound)(int sound), void (*printmessagefunc)(int type, const char *text), void (*joinprogress)(int statuscode, float scale), void (*clientloopupdate)(void)) {
  int err;

TRY
  /* ignore SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    fprintf(stderr, "Error Ignoring SIGPIPE\n");
  }

  /* init tracker */
  if ((err = pthread_mutex_init(&tracker.mutex, NULL))) LOGFAIL(err)

  /* initialize the random number generator */
#if __APPLE__
  srandomdev();
#else
  srandom(time(0));
#endif

  /* init server */
  if (initserver()) LOGFAIL(errno)

  /* init client */
  if (initclient(setplayerstatusfunc, setpillstatusfunc, setbasestatusfunc, settankstatusfunc, playsound, printmessagefunc, joinprogress, clientloopupdate)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, errno)
END
}

void pauseresumegame() {
  pauseresumeserver();
}

void togglejoingame() {
  togglejoinserver();
}

float fogvis(Vec2f v) {
  float fogvis;
  int x, y;
  float fx, cx;
  float fy, cy;

  if (v.x < 0.0 || v.x >= FWIDTH || v.y < 0.0 || v.y >= FWIDTH) {
    return 0.0;
  }

  x = ((int)v.x)%WIDTH;
  y = ((int)v.y)%WIDTH;
  fx = v.x - floorf(v.x);
  cx = 1.0 - fx;
  fy = v.y - floorf(v.y);
  cy = 1.0 - fy;

  /* calculate visibility */
  if (ISFOG(x, y)) {
    fogvis =
      MAX(
        MAX(
          MAX(ISFOG(x - 1, y) ? 0.0 : cx, ISFOG(x + 1, y) ? 0.0 : fx),
          MAX(ISFOG(x, y - 1) ? 0.0 : cy, ISFOG(x, y + 1) ? 0.0 : fy)
        ),
        MAX(
          MAX(
            ISFOG(x - 1, y - 1) ? 0.0 : 1.0 - sqrtf(fx*fx + fy*fy),
            ISFOG(x - 1, y + 1) ? 0.0 : 1.0 - sqrtf(fx*fx + cy*cy)
          ),
          MAX(
            ISFOG(x + 1, y - 1) ? 0.0 : 1.0 - sqrtf(cx*cx + fy*fy),
            ISFOG(x + 1, y + 1) ? 0.0 : 1.0 - sqrtf(cx*cx + cy*cy)
          )
        )
      );
  }
  else {
    fogvis = 1.0;
  }

  return fogvis;
}

int isforest(int x, int y) {
  int i;

  if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
    return 0;
  }

  for (i = 0; i < client.npills; i++) {
    if (client.pills[i].armour != ONBOARD && client.pills[i].x == x && client.pills[i].y == y) {
      return 0;
    }
  }

  for (i = 0; i < client.nbases; i++) {
    if (client.bases[i].x == x && client.bases[i].y == y) {
      return 0;
    }
  }

  return client.terrain[y][x] == kForestTerrain || client.terrain[y][x] == kMinedForestTerrain;
}

float forestvis(Vec2f v) {
  int x, y;
  float fx, cx;
  float fy, cy;

  if (v.x < 0.0 || v.x >= FWIDTH || v.y < 0.0 || v.y >= FWIDTH) {
    return 0.0;
  }

  x = ((int)v.x)%WIDTH;
  y = ((int)v.y)%WIDTH;
  fx = v.x - floorf(v.x);
  cx = 1.0 - fx;
  fy = v.y - floorf(v.y);
  cy = 1.0 - fy;

  if (isforest(x, y)) {
    return
      MAX(
        MAX(
          MAX(isforest(x - 1, y) ? 0.0 : cx, isforest(x + 1, y) ? 0.0 : fx),
          MAX(isforest(x, y - 1) ? 0.0 : cy, isforest(x, y + 1) ? 0.0 : fy)
        ),
        MAX(
          MAX(
            isforest(x - 1, y - 1) ? 0.0 : 1.0 - sqrtf(fx*fx + fy*fy),
            isforest(x - 1, y + 1) ? 0.0 : 1.0 - sqrtf(fx*fx + cy*cy)
          ),
          MAX(
            isforest(x + 1, y - 1) ? 0.0 : 1.0 - sqrtf(cx*cx + fy*fy),
            isforest(x + 1, y + 1) ? 0.0 : 1.0 - sqrtf(cx*cx + cy*cy)
          )
        )
      );
  }
  else {
    return 1.0;
  }
}

float calcvis(Vec2f v) {
  int i;
  int x, y;
  float fogvis, forestvis;
  float fx, cx;
  float fy, cy;
  float dist, dist2, vis;

  if (v.x < 0.0 || v.x >= FWIDTH || v.y < 0.0 || v.y >= FWIDTH) {
    return 0.0;
  }

  x = ((int)v.x)%WIDTH;
  y = ((int)v.y)%WIDTH;
  fx = v.x - floorf(v.x);
  cx = 1.0 - fx;
  fy = v.y - floorf(v.y);
  cy = 1.0 - fy;

  /* calculate visibility */
  if (ISFOG(x, y)) {
    fogvis =
      MAX(
        MAX(
          MAX(ISFOG(x - 1, y) ? 0.0 : cx, ISFOG(x + 1, y) ? 0.0 : fx),
          MAX(ISFOG(x, y - 1) ? 0.0 : cy, ISFOG(x, y + 1) ? 0.0 : fy)
        ),
        MAX(
          MAX(
            ISFOG(x - 1, y - 1) ? 0.0 : 1.0 - sqrtf(fx*fx + fy*fy),
            ISFOG(x - 1, y + 1) ? 0.0 : 1.0 - sqrtf(fx*fx + cy*cy)
          ),
          MAX(
            ISFOG(x + 1, y - 1) ? 0.0 : 1.0 - sqrtf(cx*cx + fy*fy),
            ISFOG(x + 1, y + 1) ? 0.0 : 1.0 - sqrtf(cx*cx + cy*cy)
          )
        )
      );
  }
  else {
    fogvis = 1.0;
  }

  if (isforest(x, y)) {
    forestvis =
      MAX(
        MAX(
          MAX(isforest(x - 1, y) ? 0.0 : cx, isforest(x + 1, y) ? 0.0 : fx),
          MAX(isforest(x, y - 1) ? 0.0 : cy, isforest(x, y + 1) ? 0.0 : fy)
        ),
        MAX(
          MAX(
            isforest(x - 1, y - 1) ? 0.0 : 1.0 - sqrtf(fx*fx + fy*fy),
            isforest(x - 1, y + 1) ? 0.0 : 1.0 - sqrtf(fx*fx + cy*cy)
          ),
          MAX(
            isforest(x + 1, y - 1) ? 0.0 : 1.0 - sqrtf(cx*cx + fy*fy),
            isforest(x + 1, y + 1) ? 0.0 : 1.0 - sqrtf(cx*cx + cy*cy)
          )
        )
      );
  }
  else {
    forestvis = 1.0;
  }

  dist = 3.0;

  /* find distance to player tank */
  dist2 = mag2f(sub2f(client.players[client.player].tank, v));

  if (dist2 < dist) {
    dist = dist2;
  }

  /* find shortest distance to live pill owned by player */
  for (i = 0; i < client.npills; i++) {
    if (
      client.pills[i].owner == client.player &&
      client.pills[i].armour != ONBOARD &&
      client.pills[i].armour != 0
    ) {
      dist2 = mag2f(sub2f(v, make2f(client.pills[i].x + 0.5, client.pills[i].y + 0.5)));

      if (dist2 < dist) {
        dist = dist2;
      }
    }
  }

  if (dist <= 2.0) {
    if (forestvis*fogvis < 0.5) {
      vis = 0.5;
    }
    else {
      vis = forestvis*fogvis;
    }
  }
  else if (dist < 3.0) {
    if (forestvis*fogvis < 0.5) {
      vis = ((3.0 - dist)*(0.5 - forestvis*fogvis)) + forestvis*fogvis;
    }
    else {
      vis = forestvis*fogvis;
    }
  }
  else {
    vis = forestvis*fogvis;
  }

  return vis;
}

float vec2dir(Vec2f v) {
  v.y = -v.y;
  return fmodf(_atan2f(v) + k2Pif, k2Pif);
}

Vec2f dir2vec(float dir) {
  Vec2f r;
  r = tan2f(dir);
  r.y = -r.y;
  return r;
}

struct GetHostByNameThreadInfo {
  char *hostname;
  struct in_addr sin_addr;
  int err;
  pthread_mutex_t parentmutex, childmutex;
} ;

static void *trackerthread(struct TrackerThreadInfo *trackerthreadinfo) {
//  struct GetHostByNameThreadInfo *gethostbynamethreadinfo = NULL;
//  pthread_t thread;
  struct sockaddr_in addr;
  uint8_t msg;
  struct TrackerHostList *trackerlist;
  struct TRACKER_Preamble preamble;
  uint32_t i, n;
  int lookup;
//  int err;

TRY
/*
  if ((gethostbynamethreadinfo = (struct GetHostByNameThreadInfo *)malloc(sizeof(struct GetHostByNameThreadInfo))) == NULL) LOGFAIL(errno)
  if ((gethostbynamethreadinfo->hostname = (char *)malloc(strlen(trackerthreadinfo->hostname) + 1)) == NULL) LOGFAIL(errno);
  strncpy(gethostbynamethreadinfo->hostname, trackerthreadinfo->hostname, strlen(trackerthreadinfo->hostname) + 1);
  bzero(&gethostbynamethreadinfo->sin_addr, sizeof(gethostbynamethreadinfo->sin_addr));
  if ((err = pthread_mutex_init(&gethostbynamethreadinfo->parentmutex, NULL))) LOGFAIL(err)
  if ((err = pthread_mutex_init(&gethostbynamethreadinfo->childmutex, NULL))) LOGFAIL(err)
  if ((err = pthread_mutex_lock(&gethostbynamethreadinfo->parentmutex))) LOGFAIL(err)
  if ((err = pthread_mutex_lock(&gethostbynamethreadinfo->childmutex))) LOGFAIL(err)

  if (trackerthreadinfo->trackerstatus) {
    trackerthreadinfo->trackerstatus(kGetListTrackerRESOLVING);
  }

  if ((err = pthread_create(&thread, NULL, (void *)gethostbynamethread, gethostbynamethreadinfo))) LOGFAIL(err)
  if ((err = pthread_detach(thread))) LOGFAIL(err)

  for (;;) {
    usleep(100);

    if ((err = pthread_mutex_lock(&tracker.mutex))) LOGFAIL(err)

    if (tracker.sock == -1) {
      if ((err = pthread_mutex_unlock(&tracker.mutex))) LOGFAIL(err)
      if ((err = pthread_mutex_unlock(&gethostbynamethreadinfo->parentmutex))) LOGFAIL(err)
      LOGFAIL(ECONNABORTED)
    }

    if ((err = pthread_mutex_unlock(&tracker.mutex))) LOGFAIL(err)

    if ((err = pthread_mutex_trylock(&gethostbynamethreadinfo->childmutex))) {
      if (err != EBUSY) {
        LOGFAIL(err)
      }
      else {
        continue;
      }
    }
    else {
      if (gethostbynamethreadinfo->err) {
        LOGFAIL(gethostbynamethreadinfo->err)
      }
      else {
        break;
      }
    }
  }
  */
  /* lookup address */
  if ((lookup = nslookup(trackerthreadinfo->hostname)) == -1) LOGFAIL(errno)

  /* get nslookup result */
  if (nslookupresult(lookup, &addr.sin_addr)) LOGFAIL(errno)
  if (closesock(&lookup)) LOGFAIL(errno)

  addr.sin_family = AF_INET;
  addr.sin_port = htons(TRACKERPORT);
  bzero(addr.sin_zero, 8);
//  if ((err = pthread_mutex_unlock(&gethostbynamethreadinfo->parentmutex))) LOGFAIL(err)

  /* connect */
  trackerthreadinfo->trackerstatus(kGetListTrackerCONNECTING);
  if ((connect(tracker.sock, (void *)&addr, INET_ADDRSTRLEN))) LOGFAIL(errno)

  /* send preamble */
  bcopy(TRACKERIDENT, preamble.ident, sizeof(preamble.ident));
  preamble.version = TRACKERVERSION;
  if (sendblock(tracker.sock, &preamble, sizeof(preamble)) != sizeof(preamble)) LOGFAIL(errno)

  /* receive ok */
  if (recvblock(tracker.sock, &msg, sizeof(msg)) == -1) LOGFAIL(errno)
  if (msg != kTrackerVersionOK) LOGFAIL(EBADVERSION)

  /* send list request */
  trackerthreadinfo->trackerstatus(kGetListTrackerGETTINGLIST);
  msg = kTrackerList;
  if (sendblock(tracker.sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)

  /* receive number of games */
  if (recvblock(tracker.sock, &n, sizeof(n)) == -1) LOGFAIL(errno)
  n = ntohl(n);

  /* receive games list */
  for (i = 0; i < n; i++) {
    if ((trackerlist = (void *)malloc(sizeof(struct TrackerHostList))) == NULL) LOGFAIL(errno)

    if (recvblock(tracker.sock, trackerlist, sizeof(struct TrackerHostList)) == -1) LOGFAIL(errno)

    trackerlist->game.port = ntohs(trackerlist->game.port);
    trackerlist->game.timelimit = ntohl(trackerlist->game.timelimit);

    if (addlist(trackerthreadinfo->node, trackerlist)) LOGFAIL(errno)
  }

CLEANUP
  switch (ERROR) {
  case 0:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerSUCCESS);
    }

    break;

  case EHOSTNOTFOUND:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEHOSTNOTFOUND);
    }

    CLEARERRLOG
    break;

  case EHOSTNORECOVERY:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEHOSTNORECOVERY);
    }

    CLEARERRLOG
    break;

  case EHOSTNODATA:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEHOSTNODATA);
    }

    CLEARERRLOG
    break;

  case EBADVERSION:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEBADVERSION);
    }

    CLEARERRLOG
    break;

  case ECONNABORTED:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerECONNABORTED);
    }

    CLEARERRLOG
    break;

  case EPIPE:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEPIPE);
    }

    CLEARERRLOG
    break;

  case ETIMEDOUT:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerETIMEOUT);
    }

    CLEARERRLOG
    break;

  case ECONNREFUSED:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerECONNREFUSED);
    }

    CLEARERRLOG
    break;

  case EHOSTDOWN:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEHOSTDOWN);
    }

    CLEARERRLOG
    break;

  case EHOSTUNREACH:
    if (trackerthreadinfo->trackerstatus) {
      trackerthreadinfo->trackerstatus(kGetListTrackerEHOSTUNREACH);
    }

    CLEARERRLOG
    break;

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
    break;
  }

  while (close(tracker.sock) && errno == EINTR);
  tracker.sock = -1;
  free(trackerthreadinfo->hostname);
  free(trackerthreadinfo);
  pthread_mutex_unlock(&tracker.mutex);
  CLEARERRLOG
  pthread_exit(NULL);
END
}

int listtracker(const char trackerhostname[], struct ListNode *node, void(*trackerstatus)(int status)) {
  int err;
  pthread_t thread;
  struct TrackerThreadInfo *trackerthreadinfo;

  assert(trackerhostname && node && trackerstatus);

TRY
  if ((trackerthreadinfo = (struct TrackerThreadInfo *)malloc(sizeof(struct TrackerThreadInfo))) == NULL) LOGFAIL(errno)
  if ((trackerthreadinfo->hostname = (char *)malloc(strlen(trackerhostname) + 1)) == NULL) LOGFAIL(errno)
  strcpy(trackerthreadinfo->hostname, trackerhostname);
  trackerthreadinfo->node = node;
  trackerthreadinfo->trackerstatus = trackerstatus;

  if ((err = pthread_mutex_lock(&tracker.mutex))) LOGFAIL(err)
  if ((tracker.sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)
  if ((err = pthread_create(&thread, NULL, (void *)trackerthread, trackerthreadinfo))) LOGFAIL(err)
  if ((err = pthread_detach(thread))) LOGFAIL(err)
  if ((err = pthread_mutex_unlock(&tracker.mutex))) LOGFAIL(err)

CLEANUP
ERRHANDLER(0, -1)
END
}

void stoptracker() {
  pthread_mutex_lock(&tracker.mutex);

  if (tracker.sock != -1) {
    while (close(tracker.sock) && errno == EINTR);
    tracker.sock = -1;
  }

  pthread_mutex_unlock(&tracker.mutex);
}
