/*
 *  buf.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/15/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "buf.h"
#include "errchk.h"

#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFBLOCKSIZE (16)

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static int resizebuf(struct Buf *buf, size_t nbytes);

int initbuf(struct Buf *buf) {
  assert(buf != NULL);

TRY
  if ((buf->ptr = malloc(BUFBLOCKSIZE)) == NULL) LOGFAIL(errno)
  buf->nbytes = 0;
  buf->size = BUFBLOCKSIZE;

CLEANUP
ERRHANDLER(0, -1)
END
}

void freebuf(struct Buf *buf) {
  assert(buf != NULL);
  assert(buf->ptr != NULL);
  free(buf->ptr);
  buf->ptr = NULL;
}

int resizebuf(struct Buf *buf, size_t nbytes) {
  void *ptr;
  size_t size;

  assert(buf != NULL);
  assert(buf->ptr != NULL);

TRY
  if ((size = ((nbytes + ((BUFBLOCKSIZE * 2) - 1)) / BUFBLOCKSIZE) * BUFBLOCKSIZE) != buf->size) {
    if ((ptr = realloc(buf->ptr, size)) == NULL) LOGFAIL(errno)
    buf->ptr = ptr;
    buf->size = size;
  }

CLEANUP
ERRHANDLER(0, -1)
END
}

ssize_t writebuf(struct Buf *buf, const void *data, size_t nbytes) {
  assert(buf != NULL);
  assert(buf->ptr != NULL);
  assert(data != NULL);

TRY
  /* increase the size of the buffer if needed */
  if (resizebuf(buf, buf->nbytes + nbytes)) LOGFAIL(errno)
  bcopy(data, buf->ptr + buf->nbytes, nbytes);
  buf->nbytes += nbytes;

CLEANUP
ERRHANDLER(nbytes, -1)
END
}

ssize_t readbuf(struct Buf *buf, void *data, size_t nbytes) {
  assert(buf != NULL);
  assert(buf->ptr != NULL);
  assert(nbytes <= buf->nbytes);

TRY
  if (data) {
    bcopy(buf->ptr, data, nbytes);
  }

  bcopy(buf->ptr + nbytes, buf->ptr, buf->nbytes -= nbytes);
  if (resizebuf(buf, buf->nbytes)) LOGFAIL(errno)

CLEANUP
ERRHANDLER(buf->nbytes, -1)
END
}

ssize_t sendbuf(struct Buf *buf, int d) {
  ssize_t nbytes;

  assert(buf != NULL);
  assert(buf->ptr != NULL);

TRY
  if ((nbytes = send(d, buf->ptr, buf->nbytes, 0)) == -1) {
    if (errno != EAGAIN) LOGFAIL(errno)
    nbytes = 0;
  }
  else {
    if (readbuf(buf, NULL, nbytes) == -1) LOGFAIL(errno)
  }

CLEANUP
ERRHANDLER(nbytes, -1)
END
}

ssize_t recvbuf(struct Buf *buf, int d) {
  ssize_t nbytes;
  ssize_t totalnbytes;

  assert(buf != NULL);
  assert(buf->ptr != NULL);

TRY
  totalnbytes = 0;

  do {
    if (resizebuf(buf, buf->nbytes)) LOGFAIL(errno)

    if ((nbytes = recv(d, buf->ptr + buf->nbytes, buf->size - buf->nbytes, MSG_DONTWAIT)) == -1) {
      if (errno != EAGAIN) LOGFAIL(errno)
      break;
    }

    buf->nbytes += nbytes;
    totalnbytes += nbytes;
  } while (buf->nbytes == buf->size);

CLEANUP
ERRHANDLER(totalnbytes, -1)
END
}

int selectreadwrite(int readsock, int writesock) {
  int nfds;
  fd_set readfds;
  fd_set writefds;
  int ret;

TRY
  /* select until we have success */
  for (;;) {
    nfds = 0;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(readsock, &readfds);
    nfds = MAX(nfds, readsock);
    FD_SET(writesock, &writefds);
    nfds = MAX(nfds, writesock);

    if ((nfds = select(nfds + 1, &readfds, &writefds, NULL, NULL)) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
    }
    else {
      break;
    }
  }

  if (FD_ISSET(readsock, &readfds)) {
    ret = 1;
  }
  else if (FD_ISSET(writesock, &writefds)) {
    ret = 0;
  }
  else {
    assert(0);
  }

CLEANUP
ERRHANDLER(ret, -1)
END
}

int selectreadread(int readsock1, int readsock2) {
  int nfds;
  fd_set readfds;
  int ret;

TRY
  /* select until we have success */
  for (;;) {
    nfds = 0;
    FD_ZERO(&readfds);
    FD_SET(readsock1, &readfds);
    nfds = MAX(nfds, readsock1);
    FD_SET(readsock2, &readfds);
    nfds = MAX(nfds, readsock2);

    if ((nfds = select(nfds + 1, &readfds, NULL, NULL, NULL)) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
    }
    else {
      break;
    }
  }

  if (FD_ISSET(readsock1, &readfds)) {
    ret = 1;
  }
  else if (FD_ISSET(readsock2, &readfds)) {
    ret = 0;
  }
  else {
    assert(0);
  }

CLEANUP
ERRHANDLER(ret, -1)
END
}

int cntlsend(int cntlsock, int sock, struct Buf *buf) {
  int ret;

TRY
  for (;;) {
    if ((ret = selectreadwrite(cntlsock, sock)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      break;
    }
    else if (ret == 0) {
      if (sendbuf(buf, sock) == -1) LOGFAIL(errno)

      if (buf->nbytes == 0) {
        break;
      }
    }
  }

CLEANUP
ERRHANDLER(ret, -1)
END
}

int cntlrecv(int cntlsock, int sock, struct Buf *buf, size_t nbytes) {
  int ret;

  assert(cntlsock != -1);
  assert(sock != -1);
  assert(buf != NULL);
  assert(buf->ptr != NULL);

TRY
  while (buf->nbytes < nbytes) {
    if ((ret = selectreadread(cntlsock, sock)) == -1) {
      LOGFAIL(errno)
    }
    else if (ret == 1) {
      ret = 1;
      break;
    }
    else if (ret == 0) {
      ssize_t r;

      if ((r = recvbuf(buf, sock)) == -1) {
        LOGFAIL(errno)
      }
      else if (r == 0) {
        FAIL(EPIPE)
      }
    }
  }

CLEANUP
ERRHANDLER(ret, -1)
END
}
