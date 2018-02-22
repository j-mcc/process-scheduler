/*
 * $Author: o1-mccune $
 * $Date: 2017/10/26 04:53:22 $
 * $Revision: 1.2 $
 * $Log: simulatedclock.c,v $
 * Revision 1.2  2017/10/26 04:53:22  o1-mccune
 * final
 *
 * Revision 1.1  2017/10/23 01:23:15  o1-mccune
 * Initial revision
 *
 */

#include "simulatedclock.h"
#include <sys/shm.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define BILLION 1000000000

static int increment = 1000;  //Default is 1000 nanoseconds

void setSimClockIncrement(int value){
  increment = value;
}

void resetSimClock(sim_clock_t *simClock){
  simClock->seconds = 0;
  simClock->nanoseconds = 0;
}

void findAverage(sim_clock_t *average, int num){
  if(num == 0) return;
  //fprintf(stderr, "\t\t\t\t%d / %d = %d\n", average->seconds, num, average->seconds / num);
  average->seconds = average->seconds / num;
  //fprintf(stderr, "\t\t\t\t%d / %d = %d\n", average->nanoseconds, num, average->nanoseconds / num);
  average->nanoseconds = average->nanoseconds / num;
  
}

void scaleSimClock(sim_clock_t *clock, int scale){
  clock->seconds = clock->seconds * scale;
}

void sumSimClocks(sim_clock_t *clock, sim_clock_t *time){
  //fprintf(stderr, "\t\t\t\t(%d:%d) + (%d:%d) = ", clock->seconds, clock->nanoseconds, time->seconds, time->nanoseconds);
  clock->seconds += time->seconds;
  if(clock->nanoseconds + time->nanoseconds >= BILLION){
    clock->seconds++;
    clock->nanoseconds = (clock->nanoseconds + time->nanoseconds) % BILLION;
  }
  else{
    clock->nanoseconds += time->nanoseconds;
  }
  //fprintf(stderr, "(%d:%d)\n", clock->seconds, clock->nanoseconds);
}

sim_clock_t findDifference(sim_clock_t *time1, sim_clock_t *time2){
  sim_clock_t difference;
  sim_clock_t t1;
  t1.seconds = time1->seconds;
  t1.nanoseconds = time1->nanoseconds;
  sim_clock_t t2;
  t2.seconds = time2->seconds;
  t2.nanoseconds = time2->nanoseconds;
  
  if(t1.nanoseconds - t2.nanoseconds < 0){  //borrow 1 second from seconds and add to nanoseconds
    t1.seconds--;
    t1.nanoseconds += BILLION;
    difference.seconds = t1.seconds - t2.seconds;
    difference.nanoseconds = t1.nanoseconds - t2.nanoseconds;
    return difference;
  }
  else{
    difference.seconds = (t1.seconds - t2.seconds);
    difference.nanoseconds = (t1.nanoseconds - t2.nanoseconds);
    return difference;
  }
  return difference;
}

void incrementSimClock(sim_clock_t *simClock){
  if(simClock->nanoseconds + increment >= BILLION){
    simClock->seconds += 1;
    simClock->nanoseconds = (simClock->nanoseconds + increment) % BILLION;
  }
  else simClock->nanoseconds += increment;
}

/*
 * Increment clock by 1.xx seconds; where xx ranges from [0, 1000] nanoseconds
 */
void randomIncrementSimClock(sim_clock_t *simClock){
  unsigned int increment = rand() % 1001;
  if(simClock->nanoseconds + increment >= BILLION){
    simClock->seconds += 2;
    simClock->nanoseconds = (simClock->nanoseconds + increment) % BILLION;
  }
  else{
    simClock->seconds += 1;
    simClock->nanoseconds += increment;
  }
}

int compareSimClocks(sim_clock_t *clock, sim_clock_t *compareTo){
  if(clock->seconds == compareTo->seconds){
    if(clock->nanoseconds == compareTo->nanoseconds) return 0;  //clock times are the same
    if(clock->nanoseconds < compareTo->nanoseconds) return -1;  //clock time is less than
    return 1;  //clock time is greater
  }
  else if(clock->seconds < compareTo->seconds){
    return -1;  //clock time is less than
  }
  else{
    return 1;  //clock time is greater than
  }
}

void addNanosecondsToSimClock(sim_clock_t *destination, sim_clock_t *source, int value){
    if(value % BILLION == 0){  //value is multiple of 1 second
      destination->seconds = source->seconds + value/BILLION;
      destination->nanoseconds = source->nanoseconds;
    }
    else if(value + source->nanoseconds < BILLION){  //can add value to nanoseconds without a seconds overflow
      destination->seconds = source->seconds;
      destination->nanoseconds = value + source->nanoseconds;
    }
    else if(value + source->nanoseconds > BILLION){  //take whole seconds from value and add to seconds, then add remaining to nanoseconds
      int seconds = floor((source->nanoseconds + value)/BILLION);  
      destination->seconds = source->seconds + seconds;
      destination->nanoseconds = (source->nanoseconds + value) - (seconds * BILLION);
    }
}

void copySimClock(sim_clock_t *source, sim_clock_t *destination){
  destination->seconds = source->seconds;
  destination->nanoseconds = source->nanoseconds;
}
