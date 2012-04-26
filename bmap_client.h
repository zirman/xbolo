/*
 *  bmap_client.h
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/11/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#ifndef __BMAP_CLIENT__
#define __BMAP_CLIENT__

#include "bmap.h"

/* loads map on client */
int clientloadmap(const void *buf, size_t len);

#endif /* __BMAP_CLIENT__ */
