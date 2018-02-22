/*
 * $Author: o1-mccune $
 * $Date: 2017/10/26 04:53:30 $
 * $Revision: 1.2 $
 * $Log: simulatedclock.h,v $
 * Revision 1.2  2017/10/26 04:53:30  o1-mccune
 * final.
 *
 * Revision 1.1  2017/10/23 01:23:29  o1-mccune
 * Initial revision
 *
 */

#ifndef SIMULATEDCLOCK_H
#define SIMULATEDCLOCK_H

typedef struct{
  int seconds;
  int nanoseconds;
}sim_clock_t;

void setSimClockIncrement(int value);

void resetSimClock(sim_clock_t *simClock);

void incrementSimClock(sim_clock_t *simClock);

void randomIncrementSimClock(sim_clock_t *simClock);

int compareSimClocks(sim_clock_t *clock, sim_clock_t *compareTo);

void addNanosecondsToSimClock(sim_clock_t *destination, sim_clock_t *source, int value);

void copySimClock(sim_clock_t *source, sim_clock_t *destination);

void sumSimClocks(sim_clock_t *clock, sim_clock_t *time);

sim_clock_t findDifference(sim_clock_t *time1, sim_clock_t *time2);

void findAverage(sim_clock_t *average, int num);

void scaleSimClock(sim_clock_t *clock, int scale);

#endif
