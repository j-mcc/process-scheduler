/*
 * $Author: o1-mccune $
 * $Date: 2017/10/26 04:52:22 $
 * $Revision: 1.4 $
 * $Log: child.c,v $
 * Revision 1.4  2017/10/26 04:52:22  o1-mccune
 * Final.
 *
 * Revision 1.3  2017/10/25 03:48:14  o1-mccune
 * Milestone 2. All queues are working.
 *
 * Revision 1.2  2017/10/23 01:20:19  o1-mccune
 * Milestone 1. Runmode decision should be moved to OSS scheduler.
 *
 * Revision 1.1  2017/10/14 19:18:57  o1-mccune
 * Initial revision
 *
 */


#include "scheduler.h"
#include "pcb.h"
#include "simulatedclock.h"
#include "sharedmessage.h"
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/shm.h>

static int messageId;
static int sharedSemaphoreId;
static int pcbId;
static int sharedClockId;
static int dispatchId;
static dispatch_memory_t *dispatch; 
static shared_message_t *message;
static sem_t *semaphore;
static sim_clock_t *simClock;
static pcb_t *pcb;

static void printOptions(){
  fprintf(stderr, "CHILD:  Command Help\n");
  fprintf(stderr, "\tCHILD:  Optional '-h': Prints Command Usage\n");
  fprintf(stderr, "\tCHILD:  '-d': Shared memory ID for dispatch.\n");
  fprintf(stderr, "\tCHILD:  '-p': Shared memory ID for PCB Table.\n");
  fprintf(stderr, "\tCHILD:  '-m': Shared memory ID for message.\n");
  fprintf(stderr, "\tCHILD:  '-s': Shared memory ID for semaphore.\n");
  fprintf(stderr, "\tCHILD:  '-c': Shared memory ID for clock.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  int nCP = 0;
  while ((c = getopt (argc, argv, "hd:p:s:c:m:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 'c':
        sharedClockId = atoi(optarg);
        break;
      case 'd':
        dispatchId = atoi(optarg);
        break;
      case 's':
        sharedSemaphoreId = atoi(optarg);
        break;
      case 'm':
        messageId = atoi(optarg);
        break;
      case 'p':
	pcbId = atoi(optarg);
        break;
      case '?':
	if(isprint (optopt))
          fprintf(stderr, "CHILD: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "CHILD: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  return 0;
}

static int detachDispatch(){
  return shmdt(dispatch);
}

static int detachSharedMessage(){
  return shmdt(message);
}

static int detachSharedSemaphore(){
  return shmdt(semaphore);
}

static int detachPCB(){
  return shmdt(pcb);
}

static int detachSharedClock(){
  return shmdt(simClock);
}

static int attachDispatch(){
  if((dispatch = shmat(dispatchId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedMessage(){
  if((message = shmat(messageId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedSemaphore(){
  if((semaphore = shmat(sharedSemaphoreId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachPCB(){
  if((pcb = shmat(pcbId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedClock(){
  if((simClock = shmat(sharedClockId, NULL, SHM_RDONLY)) == (void *)-1) return -1;
  return 0;
}

static void cleanUp(int signal){
  if(detachSharedSemaphore() == -1) perror("CHILD: Failed to detach shared semaphore");
  if(detachPCB() == -1) perror("CHILD: Failed to detach PCB table");
  if(detachSharedClock() == -1) perror("CHILD: Failed to detach shared clock");
  if(detachSharedMessage() == -1) perror("CHILD: Failed to detach shared message");
  if(detachDispatch() == -1) perror("CHILD: Failed to detach dispatch");
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(0);
}

static int initAlarmWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL));
}

static int initInterruptWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGINT, &action, NULL));
}


int main(int argc, char **argv){
  parseOptions(argc, argv);

  if(initAlarmWatcher() == -1){
    perror("CHILD: Failed to init SIGALRM watcher");
    exit(1);
  }
  if(initInterruptWatcher() == -1){
    perror("CHILD: Failed to init SIGINT watcher");
    exit(2);
  }
  if(attachSharedSemaphore() == -1){
    perror("CHILD: Failed to attach semaphore");
    exit(3);
  }
  if(attachPCB() == -1){
    perror("CHILD: Failed to attach PCB table");
    exit(4);
  }
  if(attachSharedClock() == -1){
    perror("CHILD: Failed to attach clock");
    exit(5);
  }
  if(attachSharedMessage() == -1){
    perror("CHILD: Failed to attach message");
    exit(6);
  }
  if(attachDispatch() == -1){
    perror("CHILD: Failed to attach dispatch");
    exit(7);
  }

  srand(time(0) + getpid());  //seed random number
  int i = findPCB(pcb, getpid()); //find pcb index  

  sim_clock_t eventDuration;

  
  while(1){
/*
    if(pcb[i].state == WAIT){
      if(compareSimClocks(simClock, &eventDuration) != -1){ //event has passed
        pcb[i].state = READY;
      }
    }
*/
    if(dispatch->pid == getpid()){ 
       
      sim_clock_t time;
      time.seconds = 0;
      

      pcb[i].runMode = rand() % 4;

      fprintf(stderr, "\tCHILD %d: Priority -> %d\n", getpid(), pcb[i].priority);
      fprintf(stderr, "\tCHILD %d: Run Mode -> %d\n", getpid(), pcb[i].runMode);
      switch(pcb[i].runMode){
        case 0:  //terminate now
          time.nanoseconds = 0;
          break;
        case 1:  //run full time quantum
          time.nanoseconds = dispatch->quantum;
          break;
        case 2:{  //start waiting for event
          time.nanoseconds = 0;  //no execution time
          pcb[i].state = WAIT;
          eventDuration.seconds = rand() % 6;
          eventDuration.nanoseconds = rand() % 1001;
          sumSimClocks(&eventDuration, &pcb[i].timeLastDispatch);
          pcb[i].eventEnd.seconds = eventDuration.seconds;
          pcb[i].eventEnd.nanoseconds = eventDuration.nanoseconds;
          fprintf(stderr, "\tCHILD %d: Event ends at %d:%d\n", getpid(), eventDuration.seconds, eventDuration.nanoseconds);
        } 
        case 3:{  //get preempted after % of quantum
          int percent, dur;
          percent = ((rand() % 99) + 1);
          dur = ((percent/100.) * dispatch->quantum);
          time.nanoseconds = dur;
          break;
        }
      }
      
      fprintf(stderr, "\tCHILD %d: Last Wait Time %d:%d\n", getpid(), pcb[i].lastWaitTime.seconds, pcb[i].lastWaitTime.nanoseconds);
      sumSimClocks(&pcb[i].totalWaitTime, &pcb[i].lastWaitTime);
      fprintf(stderr, "\tCHILD %d: Total Wait Time %d:%d\n", getpid(), pcb[i].totalWaitTime.seconds, pcb[i].totalWaitTime.nanoseconds);
      addToTotalCPUTime(&pcb[i], &time);   
      fprintf(stderr, "\tCHILD %d: Total CPU Used %d:%d\n", getpid(), pcb[i].totalCPUTime.seconds, pcb[i].totalCPUTime.nanoseconds);
      
      fprintf(stderr, "\tCHILD %d: Last Dispatched at %d:%d\n", getpid(), pcb[i].timeLastDispatch.seconds, pcb[i].timeLastDispatch.nanoseconds);
      
      setLastBurstTime(&pcb[i], &time);
      fprintf(stderr, "\tCHILD %d: Last Burst Used %d:%d\n", getpid(), pcb[i].lastBurstTime.seconds, pcb[i].lastBurstTime.nanoseconds);
      
      
      time.seconds = 0;
      time.nanoseconds = 0;

      if(pcb[i].totalCPUTime.nanoseconds >= 50000000 || pcb[i].runMode == 0){ 
        if(rand() % 2 == 1 || pcb[i].runMode == 0){
          setMessage(message, i);
          pcb[i].state = EXIT;
          dispatch->pid = -1;
          pcb[i].timeInSystem = findDifference(&pcb[i].timeLastDispatch, &pcb[i].timeCreated);
          sumSimClocks(&pcb[i].timeInSystem, &pcb[i].lastBurstTime);
          fprintf(stderr, "\tCHILD %d: Total Time in System %d:%d\n", getpid(), pcb[i].timeInSystem.seconds, pcb[i].timeInSystem.nanoseconds);
          if(sem_post(semaphore) == -1) fprintf(stderr, "CHILD %d: Failed to unlock semaphore\n", getpid()); 
          break;
        }
      }
      dispatch->pid = -1;
      if(sem_post(semaphore) == -1) fprintf(stderr, "CHILD %d: Failed to unlock semaphore\n", getpid()); 
  
    }
  }

  cleanUp(2);

  return 0;
}

