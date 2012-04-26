/*
 *  resolver.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "resolver.h"
#include "io.h"
#include "errchk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <netdb.h>
#include <math.h>
#include <netinet/tcp.h>

#include <fcntl.h>

#include <pthread.h>
#include <unistd.h>
#include <netdb.h>

struct GetHostByName {
  char *hostname;
  int pipe;
} ;

struct GetHostByNameResult {
  int err;
  struct in_addr addr;
} ;

static void *gethostbynamethread(struct GetHostByName *info);

/* returns a file descriptor that will return struct GetHostByNameResult */
int nslookup(const char *hostname) {
  struct GetHostByName *info = NULL;
  int fildes[2] = { -1, -1 };
  pthread_t thread;
  int err;

  assert(hostname != NULL);

TRY
  if ((info = (struct GetHostByName *)malloc(sizeof(struct GetHostByName))) == NULL) LOGFAIL(errno)
  if ((info->hostname = (char *)malloc(strlen(hostname) + 1)) == NULL) LOGFAIL(errno)
  strcpy(info->hostname, hostname);

  if (pipe(fildes)) {
    fildes[0] = -1;
    fildes[1] = -1;
    LOGFAIL(errno)
  }

  info->pipe = fildes[1];

  if ((err = pthread_create(&thread, NULL, (void *)gethostbynamethread, info))) LOGFAIL(err)
  fildes[1] = -1;
  info = NULL;

  if ((err = pthread_detach(thread))) LOGFAIL(err)

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(fildes[0])

  default:
    if (fildes[1] != -1) {
      closesock(fildes + 1);
    }

    if (fildes[0] != -1) {
      closesock(fildes);
    }

    if (info) {
      if (info->hostname) {
        free(info->hostname);
      }

      free(info);
    }

    RETERR(-1)
  }
END
}

int nslookupresult(int pipe, struct in_addr *addr) {
  struct GetHostByNameResult result;

  assert(pipe != -1);
  assert(addr != NULL);

TRY
  if (readblock(pipe, &result, sizeof(result)) == -1) LOGFAIL(errno)
  if (result.err) FAIL(result.err)
  *addr = result.addr;

CLEANUP
ERRHANDLER(0, -1)
END
}

int nslookupcancel(int pipe) {
TRY
  if (closesock(&pipe)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

void *gethostbynamethread(struct GetHostByName *info) {
  struct hostent *hostent;
  struct GetHostByNameResult result;

  assert(info != NULL);

TRY
  bzero(&result, sizeof(result));

  /* use udp */
  sethostent(0);

  for (;;) {
    hostent = gethostbyname(info->hostname);

    if (hostent == NULL) {
      if (h_errno == TRY_AGAIN) {
        continue;
      }
      else if (h_errno == HOST_NOT_FOUND) {
        FAIL(EHOSTNOTFOUND)
      }
      else if (h_errno == NO_RECOVERY) {
        FAIL(EHOSTNORECOVERY)
      }
      else if (h_errno == NO_DATA) {
        FAIL(EHOSTNODATA)
      }
      else {
        assert(0);
      }

      break;
    }
    else {
      result.addr.s_addr = *(uint32_t *)(hostent->h_addr_list[0]);
      break;
    }
  }

  endhostent();

CLEANUP
  result.err = ERROR;

  writeblock(info->pipe, &result, sizeof(result));

  if (info->pipe != -1) {
    if (closesock(&info->pipe)) LOGFAIL(errno)
  }

  if (info->hostname) {
    free(info->hostname);
  }

  if (info) {
    free(info);
  }

  pthread_exit(NULL);
END
}
