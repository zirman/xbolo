/*
 *  bmap_server.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __BMAP_SERVER__
#define __BMAP_SERVER__

#include "bmap.h"

/* server functions */
int serverloadmap(const void *buf, size_t nbytes);
ssize_t serversavemap(void **data);

#endif /* __BMAP_SERVER__ */
