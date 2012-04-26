/*
 *  io.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#include "io.h"
#include "errchk.h"

#include <stdlib.h>
#include <string.h>

ssize_t readblock(int d, void *buf, size_t nbytes) {
  size_t n = 0;

TRY
  while (n < nbytes) {
    ssize_t r;

    if ((r = read(d, buf + n, nbytes - n)) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
      continue;
    }

    if (r == 0) {  /* incomplete bytes */
      FAIL(EPIPE)
    }

    n += r;
  }

CLEANUP
ERRHANDLER(nbytes, -1)
END
}

ssize_t writeblock(int d, const void *buf, size_t nbytes) {
  size_t n = 0;

TRY
  while (n < nbytes) {
    ssize_t r;

    if ((r = write(d, buf + n, nbytes - n)) == -1) {
      if (errno != ENOBUFS) LOGFAIL(errno)
      usleep(100);
      continue;
    } 

    n += r;
  }

CLEANUP
ERRHANDLER(nbytes, -1)
END
}


ssize_t writestr(int d, const char *str) {
  size_t nbytes;
TRY
  nbytes = strlen(str);

  if (writeblock(d, str, nbytes) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(0, -1)
END
}

int sendblock(int s, const void *msg, size_t len) {
  size_t b = 0;

TRY
  while (b < len) {
    ssize_t r;

    if ((r = send(s, msg + b, len - b, 0)) == -1) {
      if (errno != ENOBUFS) {
        LOGFAIL(errno)
      }

      usleep(100);
      continue;
    }

    b += r;
  }

CLEANUP
ERRHANDLER(b, -1)
END
}

int sendstr(int s, const char *str) {
  ssize_t bytes;

TRY
  if ((bytes = sendblock(s, str, strlen(str) + 1)) == -1) LOGFAIL(errno)

CLEANUP
ERRHANDLER(bytes, -1)
END
}

int sendblockto(int s, const void *msg, size_t *len, int flags, const struct sockaddr *to, int tolen) {
  size_t b = 0;

TRY
  while (b < *len) {
    ssize_t r;

    if ((r = sendto(s, msg + b, (*len) - b, flags, to, tolen)) == -1) {
      if (errno != ENOBUFS) LOGFAIL(errno)
      usleep(100);
      continue;
    }

    b += r;
  }

CLEANUP
  *len = b;

ERRHANDLER(0, -1)
END
}

ssize_t recvblock(int s, void *buf, size_t len) {
  size_t n = 0;

TRY
  while (n < len) {
    ssize_t r;

    if ((r = recv(s, buf + n, len - n, MSG_WAITALL)) == -1) {
      if (errno != EINTR) {
        LOGFAIL(errno)
      }
      else {
        continue;
      }
    }

    /* Connection closed. EPIPE is normally returned for when send()ing
       on a closed pipe but it seems apropriate here since the socket
       was closed before the expected number of bytes could be recevied. */
    if (r == 0) FAIL(EPIPE)

    n += r;
  }

CLEANUP
ERRHANDLER(n, -1)
END
}

ssize_t recvblockpeek(int s, void *buf, size_t len) {
  ssize_t r;

TRY
  do {
    if ((r = recv(s, buf, len, MSG_PEEK | MSG_WAITALL)) == -1) {
      if (errno != EINTR) {
        LOGFAIL(errno)
      }
      else {
        continue;
      }
    }

    /* Connection closed. EPIPE is normally returned for when send()ing
       on a closed pipe but it seems apropriate here since the socket
       was closed before the expected number of bytes could be recevied. */
    if (r == 0) FAIL(EPIPE)
  } while (r < len);

CLEANUP
ERRHANDLER(r, -1)
END
}

ssize_t recvstr(int s, char **str) {
  char *buf = NULL;
  size_t bufsize = 8;
  size_t len = 0;

TRY
  if ((buf = (char *)malloc(bufsize)) == NULL) LOGFAIL(errno)

  do {
    /* allocate more memory if needed */
    if (len + 1 >= bufsize) {
      char *newbuf;
      bufsize *= 2;
      if ((newbuf = realloc(buf, bufsize)) == NULL) LOGFAIL(errno)
      buf = newbuf;
    }

    /* receive one byte at a time */
    for (;;) {
      if (recv(s, buf + len, 1, MSG_WAITALL) == -1) {
        if (errno != EINTR) {
          LOGFAIL(errno)
        }
        else {
          continue;
        }
      }
      else {
        break;
      }
    }

    len++;
  } while (buf[len - 1] != '\0');

CLEANUP
  switch (ERROR) {
  case 0:
    *str = buf;
    RETURN(len)

  default:
    if (buf != NULL) {
      free(buf);
    }

    *str = NULL;
    RETERR(-1)
  }
END
}

ssize_t recvblockfrom(int s, void *buf, size_t *len, int flags, struct sockaddr *from, socklen_t *fromlen) {
  ssize_t n = 0;

TRY
  while (n < *len) {
    ssize_t r;

    if ((r = recvfrom(s, buf + n, *len - n, flags, from, (socklen_t *)fromlen)) == -1) {
      if (errno != EINTR) LOGFAIL(errno)
      continue;
    }

    if (r == 0) {
      break;
    }

    n += r;
  }

CLEANUP
  *len = n;

ERRHANDLER(0, -1)
END
}

int closesock(int *sock) {
  assert(sock);
  assert(*sock != -1);

TRY
  while (close(*sock)) {
    if (errno != EINTR) {
      LOGFAIL(errno)
    }
  }

  *sock = -1;

CLEANUP
ERRHANDLER(0, -1)
END
}
