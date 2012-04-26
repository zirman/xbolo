/*
 *  timing.h
 *  XBolo
 *
 *  Created by Michael Ash on 9/7/09.
 *
 */

#ifndef __TIMING__
#define __TIMING__

#include <stdint.h>

struct frametimingstate {
    uint64_t starttime;
    uint64_t framecounter;
    uint64_t timeperframe;
    unsigned slop;
    unsigned resetthreshold;
};

uint64_t getcurrenttime(void);

// initialize a timing state struct
// call this before doing any timing stuff
// times are in nanoseconds
// timeperframe is the amount of time desired for each frame
// slop is how much slop to accept when sleeping (helps avoid too many calls to sleep)
// resetthreshold is how far behind the target we can get before giving up on
// the target and resetting instead of trying to catch up

void timinginitializestate(struct frametimingstate *state, uint64_t timeperframe, unsigned slop, unsigned resetthreshold);

// sleep until the next frame
void timingwaitframe(struct frametimingstate *state);

#endif /* __TIMING__ */
