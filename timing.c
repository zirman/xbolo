/*
 *  timing.c
 *  XBolo
 *
 *  Created by Michael Ash on 9/7/09.
 *
 */

#include "timing.h"

#include <sys/time.h>
#include <time.h>

#ifdef __MACH__
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
static mach_timebase_info_data_t gMachTimebase;
#else
static struct timespec timebase;
#endif

void initializegetcurrenttime(void) __attribute__ ((constructor)); // get it to be called automatically at startup

void initializegetcurrenttime(void) {
#ifdef __MACH__
  mach_timebase_info(&gMachTimebase);
#elif _POSIX_TIMERS
  clock_gettime(CLOCK_MONOTONIC, &timebase);
#else
  // do nothing
#endif
}

uint64_t getcurrenttime(void) {  // in nanoseconds from system boot
#ifdef __MACH__
  uint64_t absolute = mach_absolute_time();
  Nanoseconds nano = AbsoluteToNanoseconds(*(AbsoluteTime *)&absolute);
  return *(uint64_t *)&nano;
#elif _POSIX_TIMERS
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return ((uint64_t)time.tv_sec * 1000000000) + ((uint64_t)time.tv_nsec);
#else
  struct timeval tp;
  gettimeofday(&tp, NULL);
  return ((uint64_t)tp.tv_sec * 1000000000) + ((uint64_t)tp.tv_usec * 1000);
#endif
}

void sleepuntil(uint64_t nanoseconds, unsigned slop) {
  int64_t delta = nanoseconds - getcurrenttime();

  while(delta > slop) {
    struct timespec ts = { delta / 1000000000, delta % 1000000000 };
    nanosleep(&ts, NULL);
    delta = nanoseconds - getcurrenttime();
  }
}

void timinginitializestate(struct frametimingstate *state, uint64_t timeperframe, unsigned slop, unsigned resetthreshold) {
  state->starttime = getcurrenttime();
  state->framecounter = 0;
  state->timeperframe = timeperframe;
  state->slop = slop;
  state->resetthreshold = resetthreshold;
}

void timingwaitframe(struct frametimingstate *state) {
  state->framecounter++;
  uint64_t targettime = state->starttime + state->framecounter * state->timeperframe;

  if(targettime < getcurrenttime() - state->resetthreshold) {  // we've overshot and are running too slow, reset timing
    state->framecounter = 0;
    state->starttime = getcurrenttime();
  }
  else { // things are going fine, sleep until the target time
    sleepuntil(targettime, state->slop);
  }
}
