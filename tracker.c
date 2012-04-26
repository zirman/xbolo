#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include "tracker.h"
#include "list.h"
#include "errchk.h"
#include "io.h"

#define MAXPLAYERS             (16)
#define CLUPDATEMAXSHELLS      (255)
#define CLUPDATEMAXEXPLOSIONS  (255)
#define NUDP                   (5)  /* number of udp ping packets to send */
#define SECWAIT                (2)  /* seconds to wait for each UDP ping */
#define USECWAIT               (0)  /* miliseconds to wait for each UDP ping */

#ifndef ELAST
#define ELAST (1000)  /* a hack hopefully big enough */
#endif

#define EBADIDENT   (ELAST + 1)  /* Incompatable connection. */

pthread_mutex_t mutex;

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
    uint32_t seq[MAXPLAYERS];
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

struct ThreadInfo {
  int sock;
  in_addr_t s_addr;
} ;

void *child(struct ThreadInfo *arg);

struct ListNode gamelist;

int main(int argc, const char *argv[]) {
  int err;
  struct sockaddr_in addr;
  socklen_t addrlen;
  int listensock, sock;
  pthread_t thread;
  struct ThreadInfo *threadinfo;

  listensock = -1;

TRY
  /* ignore SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    fprintf(stderr, "Error Ignoring SIGPIPE\n");
  }

  if ((err = pthread_mutex_init(&mutex, NULL))) LOGFAIL(err)

  if (initlist(&gamelist)) LOGFAIL(errno)

  /* initialize listensock */
  if ((listensock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)

  /* initialize name to INADDR_ANY port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(TRACKERPORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bzero(addr.sin_zero, 8);

  /* bind listensock to name */
  if (bind(listensock, (void *)&addr, INET_ADDRSTRLEN)) LOGFAIL(errno)

  /* begin listening */
  if (listen(listensock, 3)) LOGFAIL(errno)

  for (;;) {
    addrlen = INET_ADDRSTRLEN;

    sock = accept(listensock, (void *)&addr, &addrlen);
    if (sock == -1) {
      if (errno != ECONNABORTED) {
        LOGFAIL(errno)
      }
      else {
        continue;
      }
    }

    threadinfo = (struct ThreadInfo *)malloc(sizeof(struct ThreadInfo));
    if (threadinfo == NULL) LOGFAIL(errno)
    threadinfo->sock = sock;
    threadinfo->s_addr = addr.sin_addr.s_addr;
    err = pthread_create(&thread, NULL, (void *)child, threadinfo);
    if (err) LOGFAIL(err)
    if ((err = pthread_detach(thread))) LOGFAIL(err)
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

void *child(struct ThreadInfo *threadinfo) {
  int err;
  int sock;
  in_addr_t s_addr;
  struct sockaddr_in addr;
  int testsock = -1;
  struct sockaddr_in testaddr;
  struct TRACKER_Preamble preamble;
  uint8_t msg;
  struct TrackerHostList *trackerhostlist = NULL;
  struct ListNode *node = NULL;
  int i;

TRY
  sock = threadinfo->sock;
  s_addr = threadinfo->s_addr;
  free(threadinfo);

  if (recvblock(sock, &preamble, sizeof(preamble)) == -1) LOGFAIL(errno)

  if (bcmp(preamble.ident, TRACKERIDENT, sizeof(preamble.ident)) != 0) LOGFAIL(EBADIDENT)

  if (preamble.version != TRACKERVERSION) {
    msg = kTrackerVersionErr;
    if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)
    if (closesock(&sock)) LOGFAIL(errno)
    SUCCESS
  }

  msg = kTrackerVersionOK;
  if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)

  for (;;) {
    if (recvblock(sock, &msg, sizeof(msg)) == -1) LOGFAIL(errno)

    if (msg == kTrackerHost) {
      trackerhostlist = (struct TrackerHostList *)malloc(sizeof(struct TrackerHostList));
      trackerhostlist->addr.s_addr = s_addr;
      if (recvblock(sock, &trackerhostlist->game, sizeof(struct TrackerHost)) == -1) LOGFAIL(errno)

      /* check ports are open */

      if ((testsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) LOGFAIL(errno)

      testaddr.sin_family = AF_INET;
      testaddr.sin_port = trackerhostlist->game.port;
      testaddr.sin_addr.s_addr = s_addr;
      bzero(addr.sin_zero, 8);

      /* connect to tcp port */
      if (connect(testsock, (struct sockaddr *)&testaddr, INET_ADDRSTRLEN)) {
        msg = kTrackerTCPPortClosed;
        if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)
        if (closesock(&sock)) LOGFAIL(errno)
        SUCCESS
      }

      /* no longer need testsock */
      if (closesock(&testsock)) LOGFAIL(errno)

      /* send tcp port ok */
      msg = kTrackerTCPPortOK;
      if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)

      if ((testsock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) LOGFAIL(errno)

      testaddr.sin_family = AF_INET;
      testaddr.sin_port = trackerhostlist->game.port;
      testaddr.sin_addr.s_addr = s_addr;
      bzero(addr.sin_zero, 8);

      /* designate the destination for the dgram socket */
      if (connect(testsock, (void *)&testaddr, INET_ADDRSTRLEN)) LOGFAIL(errno)

      for (i = 0; i < NUDP; i++) {
        fd_set readfds;
        struct timeval timeout;
        struct CLUpdate clupdate;
        int ret;

        /* send dgram packet */
        bzero(&clupdate, sizeof(clupdate));
        clupdate.hdr.player = 255;
        if (send(testsock, &clupdate, sizeof(clupdate.hdr), 0) == -1) LOGFAIL(errno)

        /* listen for return packet for 2 seconds */
        do {
          FD_ZERO(&readfds);
          FD_SET(testsock, &readfds);

          timeout.tv_sec = SECWAIT;
          timeout.tv_usec = USECWAIT;

        } while (
          (ret = select(testsock + 1, &readfds, NULL, NULL, &timeout)) == -1 &&
          errno == EINTR
        );

        if (ret == -1) {  /* an error occured */
          LOGFAIL(errno)
        }
        else if (ret == 0) {  /* time ran out */
          continue;
        }
        else if (FD_ISSET(testsock, &readfds)) {  /* received a packet */
          ssize_t r;

          do {
            r = recv(testsock, (void *)&clupdate,
                     sizeof(clupdate), MSG_DONTWAIT);
          } while (r == -1 && errno == EINTR);

          if (r == -1) {
            LOGFAIL(errno)
          }
          else if (r != sizeof(clupdate.hdr)) {
            continue;
          }
          else if (clupdate.hdr.player == 255) {
            break;
          }
        }
      }

      /* no longer need testsock */
      if (closesock(&testsock)) LOGFAIL(errno)

      /* if udp pings never came back */
      if (!(i < NUDP)) {
        msg = kTrackerUDPPortClosed;
        if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)
      }
      else {
        /* send udp port ok */
        msg = kTrackerUDPPortOK;
        if (sendblock(sock, &msg, sizeof(msg)) != sizeof(msg)) LOGFAIL(errno)

        if ((err = pthread_mutex_lock(&mutex))) LOGFAIL(err)
        if (addlist(&gamelist, trackerhostlist)) LOGFAIL(errno)
        trackerhostlist = NULL;
        node = nextlist(&gamelist);
        if ((err = pthread_mutex_unlock(&mutex))) LOGFAIL(err)

        /* receive updates */
        for (;;) {
          struct TrackerHost trackerhost;

          if (recvblock(sock, &trackerhost, sizeof(trackerhost)) == -1) LOGFAIL(errno)

          if ((err = pthread_mutex_lock(&mutex))) LOGFAIL(err)
          ((struct TrackerHostList *)ptrlist(node))->game = trackerhost;
          if ((err = pthread_mutex_unlock(&mutex))) LOGFAIL(err)
        }
      }
    }
    else if (msg == kTrackerList) {
      uint32_t i;

      if ((err = pthread_mutex_lock(&mutex))) LOGFAIL(err)

      for (i = 0, node = nextlist(&gamelist); node; node = nextlist(node)) {
        i++;
      }

      node = NULL;
      i = htonl(i);

      if (sendblock(sock, &i, sizeof(i)) != sizeof(i)) LOGFAIL(errno)

      for (node = nextlist(&gamelist); node; node = nextlist(node)) {
        if (
          sendblock(
            sock, ptrlist(node), sizeof(struct TrackerHostList)
          ) != sizeof(struct TrackerHostList)
        ) {
          node = NULL;
          LOGFAIL(errno)
        }
      }

      if ((err = pthread_mutex_unlock(&mutex))) LOGFAIL(err)
    }
  }

  if (closesock(&sock)) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    break;

  default:
    if (node != NULL) {
      pthread_mutex_lock(&mutex);
      removelist(node, free);
      pthread_mutex_unlock(&mutex);
    }

    if (sock != -1) {
      closesock(&sock);
    }

    if (testsock != -1) {
      closesock(&testsock);
    }

    PNONCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    break;
  }

  CLEARERRLOG
  pthread_exit(NULL);
END
}

