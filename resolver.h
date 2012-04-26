/*
 *  resolver.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 11/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <netdb.h>
#include <math.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <fcntl.h>

int nslookup(const char *hostname);
int nslookupresult(int pipe, struct in_addr *sin_addr);
int nslookupcancel(int pipe);
