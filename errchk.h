/*
 *  errchk.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski.
 *  Copyright 2004 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __ERRCHK__
#define __ERRCHK__

#include <stdio.h>
#include <assert.h>
#include <errno.h>

/* additional errno types */

#ifndef ELAST
#define ELAST (1000)  /* a hack, hopefully big enough */
#endif

#define EHOSTNOTFOUND   (ELAST + 1)   /* No such host is known. */
#define EHOSTNORECOVERY (ELAST + 2)   /* Some unexpected server failure was encountered. */
#define EHOSTNODATA     (ELAST + 3)   /* The requested name is valid but does not have an IP address. */
#define ECORFILE        (ELAST + 4)   /* Corrupt file. */
#define EINCMPAT        (ELAST + 5)   /* Incompatable file version. */
#define EBADVERSION     (ELAST + 6)   /* Incompatable server version. */
#define ETCPCLOSED      (ELAST + 7)   /* Incompatable server version. */
#define EUDPCLOSED      (ELAST + 8)   /* Incompatable server version. */
#define EDISSALLOW      (ELAST + 9)   /* New players disallowed */
#define EBADPASS        (ELAST + 10)  /* Bad password. */
#define ESERVERFULL     (ELAST + 11)  /* Server is full. */
#define ETIMELIMIT      (ELAST + 12)  /* Time limit reached. */
#define EBANNEDPLAYER   (ELAST + 13)  /* Player banned. */
#define ESERVERERROR    (ELAST + 14)  /* Server error. */

/* beginning of a TRY block */
#define TRY {

/* call this to throw an error saving the location in the stack */
#define LOGFAIL(err) { errno = err; pushlineinfo(__FILE__, __FUNCTION__, __LINE__); goto cleanup; }

/* call this to throw an error not saving the location in the stack */
#define FAIL(err) { errno = err; goto cleanup; }

/* call this to clear the error stack, should be called when catching an error and right before a thread exits */
#define CLEARERRLOG { errchkcleanup(); }

/* call this to exit successfully */
#define SUCCESS { goto success; }

/* end of TRY block and beginning of CLEANUP block */
#define CLEANUP } { int ERROR; goto success; success: errno = 0; goto cleanup; cleanup: ERROR = errno;

/* the default error handler */
#define ERRHANDLER(success_code, failure_code) { switch (ERROR) { case 0: return success_code; default: errno = ERROR; return failure_code; } }

/* returns with no error */
#define RETURN(code) { errchkcleanup(); return code; }

/* returns with error */
#define RETERR(code) { errno = ERROR; return code; }

/* prints a noncritical error to stderr */
#define PNONCRIT(msg, err) { assert(fprintf(stderr, "non-critical error; %s (%d); %s\n", msg, err, strerror(err)) >= 0); }

/* prints a critical error to stderr */
#define PCRIT(err) { assert(fprintf(stderr, "Critical Error (%d); %s\n", err, strerror(err)) >= 0); }

/* end of CLEANUP block */
#define END }

void pushlineinfo(const char *file, const char *function, size_t line);
void errchkcleanup();
void printlineinfo();

#endif /* __ERRCHK__ */
