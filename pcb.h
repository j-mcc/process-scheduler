/*
 * $Author: o1-mccune $
 * $Date: 2017/10/26 04:52:59 $
 * $Revision: 1.5 $
 * $Log: pcb.h,v $
 * Revision 1.5  2017/10/26 04:52:59  o1-mccune
 * final.
 *
 * Revision 1.4  2017/10/25 03:48:48  o1-mccune
 * Milestone 2. All queues are working.
 *
 * Revision 1.3  2017/10/23 01:22:02  o1-mccune
 * Milestone 1.
 *
 * Revision 1.2  2017/10/14 17:15:26  o1-mccune
 * Added pid field to pcb struct
 *
 * Revision 1.1  2017/10/14 16:22:18  o1-mccune
 * Initial revision
 *
 */

#ifndef PCB_H
#define PCB_H

#include <sys/types.h>
#include "simulatedclock.h"

#define PCB_TABLE_SIZE 18

typedef struct{
  pid_t pid;                        //process id
  int priority;                     //process priority
  enum {CREATED, READY, WAIT, EXIT} state;   //ready/wait/exit
  unsigned char runMode;           //run mode
  sim_clock_t eventEnd;             //time event will end
  sim_clock_t totalWaitTime;
  sim_clock_t timeCreated;          //time when process entered system
  sim_clock_t timeLastDispatch;     //time when last dispatched
  sim_clock_t timeInSystem;         //elapsed time since process entered system
  sim_clock_t totalCPUTime;         //total time using cpu
  sim_clock_t lastBurstTime;        //duration of last cpu use
  sim_clock_t lastWaitTime;         //duration of last wait
} pcb_t;

void initPCB(pcb_t *pcb, pid_t pid, sim_clock_t *time);

void addToTimeInSystem(pcb_t *pcb, sim_clock_t *time);

void addToTotalCPUTime(pcb_t *pcb, sim_clock_t *time);

void setLastBurstTime(pcb_t *pcb, sim_clock_t *time);

void setLastDispatchTime(pcb_t *pcb, sim_clock_t *time);

void changePriority(pcb_t *pcb, unsigned int priority);

pid_t getPID(pcb_t *pcb);

void setLastWaitTime(pcb_t *pcb, sim_clock_t *time);

int findPCB(pcb_t *pcb, pid_t pid);

#endif
