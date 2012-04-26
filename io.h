/*
 *  io.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __IO__
#define __IO__

#include <unistd.h>
#include <sys/socket.h>

/*
 * Readblock() reads nbytes of file descriptor d into buf.  Returns nbytes
 * on success and -1 on failure and sets errno.
 */

ssize_t readblock(int d, void *buf, size_t nbytes);

/*
 * Writeblock() writes nbytes of buf to file descriptor d.  Returns nbytes
 * on success and -1 on failure and sets errno.
 */

ssize_t writeblock(int d, const void *buf, size_t nbytes);

/*
 * Writestr() writes the string str to the file descriptor d.  Returns 0
 * on success and -1 on failure and sets errno.
 */

ssize_t writestr(int d, const char *str);

/*
 * Sendblock() sends len bytes of msg to socket s.  Returns 0
 * on success, or -1 on failure and sets errno.
 */

int sendblock(int s, const void *msg, size_t len);

/*
 * Sendstr() sends the characters in str including the terminating NUL
 * character.  Returns 0 on success, or -1 on failure and sets errno.
 */

int sendstr(int s, const char *str);

/*
 * Sendblockto() sends len bytes of msg to socket s.  Returns 0
 * on success, or -1 on failure and sets errno.  Changes len to the
 * number of bytes sent regardless of success or failure.
 */
 
//int sendblockto(int s, const void *msg, size_t *len, int flags, const struct sockaddr *to, int tolen);

/*
 * Recvblock() attempts to read len bytes to buf from socket s.  Returns number of bytes
 * received on success, or -1 on failure and sets errno.
 */

ssize_t recvblock(int s, void *buf, size_t len);

/*
 * Recvblockpeek() writes len bytes to buf from socket s but does not remove
 * them from the buffer.  Returns number of bytes received on success, or -1
 * on failure and sets errno.
 */

ssize_t recvblockpeek(int s, void *buf, size_t len);

/*
 * Recvstr() reads socket s until a terminating NUL character is found and
 * allocates a string which is what str is set to.  Returns number of bytes
 * read including terminating NUL character on success, or -1 on failure and
 * sets errno.  Caller must free() str when it no longer needs it.
 */

ssize_t recvstr(int s, char **str);

/*
 * Recvblockfrom() receives nbytes of socket s into buf.  Returns 0
 * on success and -1 on failure and sets errno.  Changes nbytes to the
 * number of bytes received regardless of success or failure.
 */

ssize_t recvblockfrom(int s, void *buf, size_t *len, int flags, struct sockaddr *from, socklen_t *fromlen);

int closesock(int *sock);

#endif  /* __IO__ */
