/*
 *  $Author: o1-mccune $
 *  $Date: 2017/10/26 04:52:44 $
 *  $Revision: 1.6 $
 *  $Log: oss.c,v $
 *  Revision 1.6  2017/10/26 04:52:44  o1-mccune
 *  final.
 *
 *  Revision 1.5  2017/10/25 03:48:28  o1-mccune
 *  Milestone 2. All queues are working.
 *
 *  Revision 1.4  2017/10/23 01:18:16  o1-mccune
 *  Milestone 1. Single queue round robin working. Foundation is laid for multiple queues.
 *
 *  Revision 1.3  2017/10/21 00:15:58  o1-mccune
 *  Added functions to manipulate a queue
 *
 *  Revision 1.2  2017/10/14 19:20:52  o1-mccune
 *  Updated execl call with correct child options.
 *
 *  Revision 1.1  2017/10/14 19:11:07  o1-mccune
 *  Initial revision
 *
 */

#define WAIT_THRESHOLD1 10
#define WAIT_THRESHOLD2 20
#define NUM_QUEUES 3

#include "scheduler.h"
#include "bitarray.h"
#include "pcb.h"
#include "simulatedclock.h"
#include "sharedmessage.h"
#include <sys/wait.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>


static int logEntries = 0;
static int logEntryCutoff = 10000;
static unsigned int maxProcessTime = 20;
static char defaultLogFilePath[] = "logfile.txt";
static char *logFilePath = NULL;
static FILE *logFile;

/*--System Report Variables--*/
static sim_clock_t averageTurnaroundTime;
static sim_clock_t averageWaitTime;
static sim_clock_t cpuIdleTime;
/*---------------------------*/

/*--Shared Memory Variables--*/
static pcb_t *pcb;
static sem_t *semaphore;
static sim_clock_t *simClock;
static dispatch_memory_t *dispatch;
static shared_message_t *message;
static key_t messageSharedMemoryKey;
static key_t dispatchSharedMemoryKey;
static key_t clockSharedMemoryKey;
static key_t pcbSharedMemoryKey;
static key_t semaphoreSharedMemoryKey;
static int semaphoreSharedMemoryId;
static int dispatchSharedMemoryId;
static int messageSharedMemoryId;
static int pcbSharedMemoryId;
static int clockSharedMemoryId;
/*---------------------------*/

static pid_t *children;
static int childCounter = 0;


static int activeQueue = 0;
//priority queue; as items are dispatched from this queue, they are moved to the secondary queue in their respective priority
//when the active queue is completely traversed; the active queue switches
static queue_item_t *queue[2][NUM_QUEUES];
static queue_item_t *blocked;  //blocked queue; processes that are waiting for an event
static queue_item_t *created;  //created queue is used to initially hold processes that have been created but not yet scheduled




/*--Functions for Manipulating Priority Queues--*/

sim_clock_t findQueueAverageWait(int priority){
  sim_clock_t average;
  average.seconds = 0;
  average.nanoseconds = 0;
  int numInQueue = 0;

  int i;
  for(i = 0; i < 2; i++){
    queue_item_t *iterator = queue[i][priority];
    while(iterator){
      numInQueue++;
      sumSimClocks(&average, &iterator->pcb->totalWaitTime);
      iterator = iterator->next;
    }
  }
  findAverage(&average, numInQueue);
  fprintf(stderr, "OSS: Queue %d has %d members --Average Wait: %d:%d\n", priority, numInQueue, average.seconds, average.nanoseconds);
  return average;
}

static void printQueue(queue_item_t *head){
  if(!head) return;
  queue_item_t *iterator = head;
  while(iterator->next != NULL){
    fprintf(stderr, "\tPID: %d\n", iterator->pcb->pid);
    iterator = iterator->next;
  }
  fprintf(stderr, "\tPID: %d\n", iterator->pcb->pid);
}

static void insertCreatedItem(pcb_t *pcb){
  queue_item_t *entry = malloc(sizeof(queue_item_t));
  entry->pcb = pcb;

  if(!created){  //created queue is empty
    created = entry;
    created->next = NULL;
    created->previous = created;
  }
  else{
    queue_item_t *tail = created->previous;
    tail->next = entry;
    entry->next = NULL;
    entry->previous = tail;
    created->previous = entry;
  }
  fprintf(stderr, "OSS: Process %d: Enters system at time %d:%d\n", pcb->pid, simClock->seconds, simClock->nanoseconds);
  if(logFile && logEntries < logEntryCutoff){
    fprintf(logFile, "OSS: Process %d: Enters system at time %d:%d\n", pcb->pid, simClock->seconds, simClock->nanoseconds);
    logEntries++;
  }
}

/*
 * All processes get initially inserted into queue[0]
 */
static void insertQueueItem(int queueType, int priority, pcb_t *pcb){
  queue_item_t *entry = malloc(sizeof(queue_item_t));
  entry->pcb = pcb;
  pcb->priority = priority;

  if(!queue[queueType][priority]){  //queue is empty
    queue[queueType][priority] = entry;
    queue[queueType][priority]->next = NULL;
    queue[queueType][priority]->previous = entry;
  }
  else{  //add to tail
    queue_item_t *tail = queue[queueType][priority]->previous;
    tail->next = entry;
    entry->next = NULL;
    entry->previous = tail;
    queue[queueType][priority]->previous = entry;
  }
  fprintf(stderr, "OSS: Process %d: Moved to queue %d at time %d:%d\n", pcb->pid, priority, simClock->seconds, simClock->nanoseconds);
  if(logFile && logEntries < logEntryCutoff){
    fprintf(logFile, "OSS: Process %d: Moved to queue %d at time %d:%d\n", pcb->pid, priority, simClock->seconds, simClock->nanoseconds);
    logEntries++;
  }
}

static void insertBlockedItem(pcb_t *pcb){
  queue_item_t *entry = malloc(sizeof(queue_item_t));
  entry->pcb = pcb;

  if(!blocked){  //created queue is empty
    blocked = entry;
    blocked->next = NULL;
    blocked->previous = blocked;
  }
  else{
    queue_item_t *tail = blocked->previous;
    tail->next = entry;
    entry->next = NULL;
    entry->previous = tail;
    blocked->previous = entry;
  }
  fprintf(stderr, "OSS: Process %d: Moved to blocked queue at time %d:%d\n", pcb->pid, simClock->seconds, simClock->nanoseconds);
  if(logFile && logEntries < logEntryCutoff){
    fprintf(logFile, "OSS: Process %d: Moved to blocked queue at time %d:%d\n", pcb->pid, simClock->seconds, simClock->nanoseconds);
    logEntries++;
  }
}

static void retrieveReadyBlockedProcesses(){
  //fprintf(stderr, "OSS: looking for ready/blocked processes\n");
  queue_item_t *iterator = blocked;
  while(iterator){
     if(compareSimClocks(simClock, &iterator->pcb->eventEnd) != -1){
    //if(iterator->pcb->state == READY){  //found blocked process that is now ready
      iterator->pcb->state = READY;
      fprintf(stderr, "OSS: Process %d: Event occured\n", iterator->pcb->pid);
      if(logFile && logEntries < logEntryCutoff){
        fprintf(logFile, "OSS: Process %d: Event occured\n", iterator->pcb->pid);
        logEntries++;
      }
      insertQueueItem(activeQueue, 0, iterator->pcb);  //insert back into active priority queue with a high priority
      if(iterator == blocked){  //its the first item
        if(iterator->previous == iterator){ //its the only item
          blocked = NULL;
        }
        else{  //not only item
          blocked = iterator->next; //move head pointer to next element
          blocked->previous = iterator->previous;  //link new head with tail
        }
        iterator = blocked;  //move iterator forward and continue searching
      }     
      else if(iterator->next == NULL){ //its the last item
        blocked->previous = iterator->previous;  //move head->previous to new tail
        iterator->previous->next = NULL;  //set new tail->next to NULL
        iterator = NULL;
      }
      else{  //its in the middle
        iterator->previous->next = iterator->next;
        iterator->next->previous = iterator->previous;
        iterator = iterator->next;
      }
    }
    else iterator = iterator->next;
  }
  //fprintf(stderr, "OSS: finished looking for ready/blocked processes\n");
}


static void popCreatedQueue(){
  //fprintf(stderr, "OSS: popCreatedQueue()\n");
  if(!created) return;
  else{
    if(created->next == NULL) created = NULL;
    else{
      created->next->previous = created->previous;
      created = created->next;
    }
  }
}

//Call this function when a process terminates and should be removed completly
static pcb_t *popActiveQueue(){
  //fprintf(stderr, "OSS: popActiveQueue()\n");
  int i;
  for(i = 0; i < NUM_QUEUES; i++){
    if(queue[activeQueue][i] == NULL) continue;
    else{
      if(queue[activeQueue][i]->next == NULL){  //only item in this queue
        pcb_t *process = queue[activeQueue][i]->pcb;
        //fprintf(stderr, "OSS: Process %d: Popped\n", queue[activeQueue][i]->pcb->pid);
        queue[activeQueue][i] = NULL;
        return process;
      }
      else{
        pcb_t *process = queue[activeQueue][i]->pcb;
        //fprintf(stderr, "OSS: Process %d: Popped\n", queue[activeQueue][i]->pcb->pid);
        queue[activeQueue][i]->next->previous = queue[activeQueue][i]->previous;
        queue[activeQueue][i] = queue[activeQueue][i]->next;
        return process;
      }
    }
  }
  return NULL;
}

static queue_item_t *getNextActive(){
  int i;
  for(i = 0; i < NUM_QUEUES; i++){
    if(queue[activeQueue][i] != NULL) return queue[activeQueue][i];
  }
  return NULL;
}

static void schedule(){
  //fprintf(stderr, "OSS: Scheduling running\n");
  if(created){  //process exists that has not been scheduled
    insertQueueItem(activeQueue, 0, created->pcb);
    created->pcb->state = READY;
    popCreatedQueue();
  }
  //if next ready process has run; deal with it
  queue_item_t *next;
  if((next = getNextActive()) != NULL){
    sim_clock_t threshold1;
    threshold1.seconds = WAIT_THRESHOLD1;
    threshold1.nanoseconds = 0;
    sim_clock_t threshold2;
    threshold2.seconds = WAIT_THRESHOLD2;
    threshold2.nanoseconds = 0;
    sim_clock_t average;
    switch(next->pcb->runMode){
      case 5:{  //has not run
        break;
      }
      case 0:  //process terminated
        sumSimClocks(&averageTurnaroundTime, &next->pcb->timeInSystem);  //add this processes time in system to total turnaround time
        popActiveQueue();
        break;
      case 1:{  //process ran quantum; needs rescheduled
        if(next->pcb->priority == 0){
          average = findQueueAverageWait(0);
          scaleSimClock(&average, ALPHA);
          if(compareSimClocks(&next->pcb->totalWaitTime, &average) == 1 && compareSimClocks(&next->pcb->totalWaitTime, &threshold1) == 1){
            insertQueueItem((activeQueue + 1) % 2, 1, next->pcb);
            popActiveQueue();
          }
          else{ 
           insertQueueItem((activeQueue + 1) % 2, 0, next->pcb);
           popActiveQueue();
          }
        }
        else if(next->pcb->priority == 1){
          average = findQueueAverageWait(1);
          scaleSimClock(&average, BETA);
          if(compareSimClocks(&next->pcb->totalWaitTime, &average) == 1 && compareSimClocks(&next->pcb->totalWaitTime, &threshold2) == 1){
            insertQueueItem((activeQueue + 1) % 2, 2, next->pcb);
            popActiveQueue();
          }
          else{ 
           insertQueueItem((activeQueue + 1) % 2, 1, next->pcb);
           popActiveQueue();
          }
        }
        else{
          insertQueueItem((activeQueue + 1) % 2, 2, next->pcb);
        }
        /*
        int nextQueue = rand() % 3;
        findQueueAverageWait(nextQueue);
        insertQueueItem((activeQueue + 1) % 2, nextQueue, next->pcb);
        popActiveQueue();
        */
        break;
      }
      case 2:{  //waiting on event; generate waiting time; put in blocked queue until that time is up
        insertBlockedItem(next->pcb);
        popActiveQueue();
        break;
      }
      case 3:{  //ran, but was preempted. needs reschedule
        if(next->pcb->priority == 0){
          average = findQueueAverageWait(0);
          scaleSimClock(&average, ALPHA);
          if(compareSimClocks(&next->pcb->totalWaitTime, &average) == 1 && compareSimClocks(&next->pcb->totalWaitTime, &threshold1) == 1){
            insertQueueItem((activeQueue + 1) % 2, 1, next->pcb);
            popActiveQueue();
          }
          else{ 
           insertQueueItem((activeQueue + 1) % 2, 0, next->pcb);
           popActiveQueue();
          }
        }
        else if(next->pcb->priority == 1){
          average = findQueueAverageWait(1);
          scaleSimClock(&average, BETA);
          if(compareSimClocks(&next->pcb->totalWaitTime, &average) == 1 && compareSimClocks(&next->pcb->totalWaitTime, &threshold2) == 1){
            insertQueueItem((activeQueue + 1) % 2, 2, next->pcb);
            popActiveQueue();
          }
          else{ 
           insertQueueItem((activeQueue + 1) % 2, 1, next->pcb);
           popActiveQueue();
          }
        }
        else{
          insertQueueItem((activeQueue + 1) % 2, 2, next->pcb);
        }
        /*
        int nextQueue = rand() % 3;
        findQueueAverageWait(nextQueue);
        insertQueueItem((activeQueue + 1) % 2, nextQueue, next->pcb);
        popActiveQueue();
        */
        break;
      }
    }
  }
  //else fprintf(stderr, "OSS: No process left to schedule\n");
  retrieveReadyBlockedProcesses();
}

static int dispatchNext(int level){
  if(isEmpty()) return -1;  //check bitarray to see if all processes have terminated
  if(level == 2) return -1000;  //no process ready to run, some are still blocked  

  int i;
  for(i = 0; i < NUM_QUEUES; i++){
    if(queue[activeQueue][i] == NULL) continue;  //active queue 'i' is empty
    else{  //dispatch next process
      dispatch->pid = queue[activeQueue][i]->pcb->pid;
      switch(i){
        case 0:
          dispatch->quantum = QUANTUM;
          break;
        case 1:
          dispatch->quantum = QUANTUM/2;
          break;
        case 2:
          dispatch->quantum = QUANTUM/4;
          break;
        default:
          dispatch->quantum = QUANTUM;
      }
      fprintf(stderr, "OSS: Process %d: Dispatched at time %d:%d\n", dispatch->pid, simClock->seconds, simClock->nanoseconds);
      if(logFile && logEntries < logEntryCutoff){
        fprintf(logFile, "OSS: Process %d: Dispatched at time %d:%d\n", dispatch->pid, simClock->seconds, simClock->nanoseconds);
        logEntries++;
      }
      setLastDispatchTime(queue[activeQueue][i]->pcb, simClock);
      return dispatch->pid;
    }
  }
  
  //if control reaches this point; 'activeQueue' is empty and should be swapped to other queue
  activeQueue = (activeQueue + 1) % 2;
  return dispatchNext(++level);  //call dispatch again
}

/*-------------------------------------------*/

static void printOptions(){
  fprintf(stderr, "OSS:  Command Help\n");
  fprintf(stderr, "\tOSS:  '-h': Prints Command Usage\n");
  fprintf(stderr, "\tOSS:  Optional '-l': Filename of log file. Default is logfile.txt\n");
  fprintf(stderr, "\tOSS:  Optional '-t': Input number of seconds before the main process terminates. Default is 20 seconds.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  while ((c = getopt (argc, argv, "ht:l:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 't':
        maxProcessTime = atoi(optarg);
        break;
      case 'l':
	logFilePath = malloc(sizeof(char) * (strlen(optarg) + 1));
        memcpy(logFilePath, optarg, strlen(optarg));
        logFilePath[strlen(optarg)] = '\0';
        break;
      case '?':
        if(optopt == 't'){
          maxProcessTime = 20;
	  break;
	}
	else if(isprint (optopt))
          fprintf(stderr, "OSS: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "OSS: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  if(!logFilePath){
    logFilePath = malloc(sizeof(char) * strlen(defaultLogFilePath) + 1);
    memcpy(logFilePath, defaultLogFilePath, strlen(defaultLogFilePath));
    logFilePath[strlen(defaultLogFilePath)] = '\0';
  }
  return 0;
}

static int initDispatchSharedMemory(){
  if((dispatchSharedMemoryKey = ftok("./oss", 5)) == -1) return -1;
  //fprintf(stderr, "OSS: Dispatch Shared Memory Key: %d\n", dispatchSharedMemoryKey);
  if((dispatchSharedMemoryId = shmget(dispatchSharedMemoryKey, sizeof(dispatch_memory_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for dispatch");
    return -1;
  }
  //fprintf(stderr, "OSS: Dispatch Shared Memory ID: %d\n", dispatchSharedMemoryId);
  return 0;
}

static int initMessageSharedMemory(){
  if((messageSharedMemoryKey = ftok("./oss", 4)) == -1) return -1;
  //fprintf(stderr, "OSS: Message Shared Memory Key: %d\n", messageSharedMemoryKey);
  if((messageSharedMemoryId = shmget(messageSharedMemoryKey, sizeof(shared_message_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for message");
    return -1;
  }
  //fprintf(stderr, "OSS: Message Shared Memory ID: %d\n", messageSharedMemoryId);
  return 0;
}

static int initSemaphoreSharedMemory(){
  if((semaphoreSharedMemoryKey = ftok("./oss", 1)) == -1) return -1;
  //fprintf(stderr, "OSS: Semaphore Shared Memory Key: %d\n", semaphoreSharedMemoryKey);
  if((semaphoreSharedMemoryId = shmget(semaphoreSharedMemoryKey, sizeof(sem_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for semaphore");
    return -1;
  }
  //fprintf(stderr, "OSS: Semaphore Shared Memory ID: %d\n", semaphoreSharedMemoryId);
  return 0;
}

static int initPCBSharedMemory(){
  if((pcbSharedMemoryKey = ftok("./oss", 2)) == -1) return -1;
  //fprintf(stderr, "OSS: PCB Shared Memory Key: %d\n", pcbSharedMemoryKey);
  if((pcbSharedMemoryId = shmget(pcbSharedMemoryKey, sizeof(pcb_t) * PCB_TABLE_SIZE, IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for PCB");
    return -1;
  }
  //fprintf(stderr, "OSS: PCB Shared Memory ID: %d\n", pcbSharedMemoryId);
  return 0;
}

static int initClockSharedMemory(){
  if((clockSharedMemoryKey = ftok("./oss", 3)) == -1) return -1;
  //fprintf(stderr, "OSS: Clock Shared Memory Key: %d\n", clockSharedMemoryKey);
  if((clockSharedMemoryId = shmget(clockSharedMemoryKey, sizeof(sim_clock_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for clock");
    return -1;
  }
  //fprintf(stderr, "OSS: Clock Shared Memory ID: %d\n", clockSharedMemoryId);
  return 0;
}

static int removeDispatchSharedMemory(){
  if(shmctl(dispatchSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove dispatch shared memory");
    return -1;
  }
  return 0;
}

static int removeMessageSharedMemory(){
  if(shmctl(messageSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove message shared memory");
    return -1;
  }
  return 0;
}

static int removeSemaphoreSharedMemory(){
  if(shmctl(semaphoreSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove semaphore shared memory");
    return -1;
  }
  return 0;
}

static int removePCBSharedMemory(){
  if(shmctl(pcbSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove PCB shared memory");
    return -1;
  }
  return 0;
}

static int removeClockSharedMemory(){
  if(shmctl(clockSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove clock shared memory");
    return -1;
  }
  return 0;
}

static int detachDispatchSharedMemory(){
  return shmdt(dispatch);
}

static int detachMessageSharedMemory(){
  return shmdt(message);
}

static int detachSemaphoreSharedMemory(){
  return shmdt(semaphore);
}

static int detachClockSharedMemory(){
  return shmdt(simClock);
}

static int detachPCBSharedMemory(){
  return shmdt(pcb);
}

static int attachDispatchSharedMemory(){
  if((dispatch = shmat(dispatchSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachMessageSharedMemory(){
  if((message = shmat(messageSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSemaphoreSharedMemory(){
  if((semaphore = shmat(semaphoreSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachPCBSharedMemory(){
  if((pcb = shmat(pcbSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachClockSharedMemory(){
  if((simClock = shmat(clockSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

/*
 * Before detaching and removing IPC shared memory, use this to destroy the semaphore.
 */
static int removeSemaphore(sem_t *semaphore){
  if(sem_destroy(semaphore) == -1){
    perror("OSS: Failed to destroy semaphore");
    return -1;
  }
  return 0;
}

/*
 * After IPC shared memory has been allocated and attached to process, use this to initialize the semaphore. 
 */
static int initSemaphore(sem_t *semaphore, int processOrThreadSharing, unsigned int value){
  if(sem_init(semaphore, processOrThreadSharing, value) == -1){
    perror("OSS: Failed to initialize semaphore");
    return -1;
  }
  return 0;
}

static void cleanUp(int signal){
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    //if(children[i] > 0){
      if(signal == 2) fprintf(stderr, "Parent sent SIGINT to Child %d\n", pcb[i].pid);
      else if(signal == 14)fprintf(stderr, "Parent sent SIGALRM to Child %d\n", pcb[i].pid);
      kill(pcb[i].pid, signal);
      waitpid(-1, NULL, 0);
    //}
  }

  if(detachDispatchSharedMemory() == -1) perror("OSS: Failed to detach dispatch memory");
  if(removeDispatchSharedMemory() == -1) perror("OSS: Failed to remove dispatch memory");
  if(detachMessageSharedMemory() == -1) perror("OSS: Failed to detach message memory");
  if(removeMessageSharedMemory() == -1) perror("OSS: Failed to remove message memory");
  if(detachPCBSharedMemory() == -1) perror("OSS: Failed to detach PCB memory");
  if(removePCBSharedMemory() == -1) perror("OSS: Failed to remove PCB memory");
  if(detachClockSharedMemory() == -1) perror("OSS: Failed to detach clock memory");
  if(removeClockSharedMemory() == -1) perror("OSS: Failed to remove clock memory");
  if(removeSemaphore(semaphore) == -1) fprintf(stderr, "OSS: Failed to remove semaphore");
  if(detachSemaphoreSharedMemory() == -1) perror("OSS: Failed to detach semaphore shared memory");
  if(removeSemaphoreSharedMemory() == -1) perror("OSS: Failed to remove seamphore shared memory");
  if(children) free(children);
  if(logFile) fclose(logFile);
  freeBitVector();
  fprintf(stderr, "OSS: Total children created :: %d\n", childCounter); 
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(signal);
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

/*
 *  Transform an integer into a string
 */
static char *itoa(int num){
  char *asString = malloc(sizeof(char)*16);
  snprintf(asString, sizeof(char)*16, "%d", num);
  return asString;
}

int main(int argc, char **argv){
  parseOptions(argc, argv);

  children = malloc(sizeof(pid_t) * PCB_TABLE_SIZE);
  logFile = fopen(logFilePath, "w");
  
  averageTurnaroundTime.seconds = 0;
  averageTurnaroundTime.nanoseconds = 0;

  averageWaitTime.seconds = 0;
  averageWaitTime.nanoseconds = 0;

  cpuIdleTime.seconds = 0;
  cpuIdleTime.nanoseconds = 0;

  if(initAlarmWatcher() == -1) perror("OSS: Failed to init SIGALRM watcher");
  if(initInterruptWatcher() == -1) perror("OSS: Failed to init SIGINT watcher");
  if(initDispatchSharedMemory() == -1) perror("OSS: Failed to init dispatch shared memory");
  if(attachDispatchSharedMemory() == -1) perror("OSS: Failed to attach dispatch shared memory");
  if(initMessageSharedMemory() == -1) perror("OSS: Failed to init message shared memory");
  if(attachMessageSharedMemory() == -1) perror("OSS: Failed to attach message shared memory");
  if(initSemaphoreSharedMemory() == -1) perror("OSS: Failed to init semaphore shared memory");
  if(attachSemaphoreSharedMemory() == -1) perror("OSS: Failed to attach semaphore shared memory");
  if(initSemaphore(semaphore, 1, 1) == -1) fprintf(stderr, "OSS: Failed to create semaphore");
  if(initPCBSharedMemory() == -1) perror("OSS: Failed to init PCB memory");
  if(attachPCBSharedMemory() == -1) perror("OSS: Failed to attach PCB memory");
  if(initClockSharedMemory() == -1) perror("OSS: Failed to init clock memory");
  if(attachClockSharedMemory() == -1) perror("OSS: Failed to attach clock memory");
  if(initBitVector(PCB_TABLE_SIZE, MIN_VALUE) == -1) fprintf(stderr, "OSS: Failed to initialized bit vector\n");
  
  resetMessage(message);
  //fprintf(stderr, "OSS: Message->Number = %d\n", message->number);
  resetSimClock(simClock);
  alarm(maxProcessTime);
  pid_t childpid;
  dispatch->pid = -1;   
  //seed random number generator
  srand(time(0) + getpid());

  //time to create next process  
  sim_clock_t createNext;
  createNext.seconds = 0;
  createNext.nanoseconds = 0;
  



  pid_t lastDispatch;
  int aliveChildren = 0;



  while(1){
    //increment clock
    randomIncrementSimClock(simClock);

    //if room for another child
    if(childCounter < PCB_TABLE_SIZE){
      //if it is past the time to create the next child  
      if(compareSimClocks(simClock, &createNext) != -1){
        //create child
        if((childpid = fork()) > 0){  //parent code
          if(setBit(childCounter) == -1) fprintf(stderr, "OSS: Failed to set bit %d of bit vector\n", childCounter);
          initPCB(&pcb[childCounter], childpid, simClock);
          insertCreatedItem(&pcb[childCounter]);
          childCounter++;
          aliveChildren++;
          createNext.seconds = rand() % 3;
        }
        else if(childpid == 0){  //child code
          execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-d", itoa(dispatchSharedMemoryId), "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageSharedMemoryId), NULL);
        }
        else perror("OSS: Failed to fork");
      }
    }


    if(sem_wait(semaphore) == 0){  //semaphore locked
      if(message->number >= 0 ){
        //process terminating      
        pcb_t *popped = popActiveQueue();
        if(popped){
          sumSimClocks(&averageTurnaroundTime, &popped->timeInSystem);  //add this processes time in system to total turnaround time
          sumSimClocks(&averageWaitTime, &popped->totalWaitTime);
        }
        fprintf(stderr, "OSS: Process %d has terminated\n", popped->pid);
        if(logFile && logEntries < logEntryCutoff){
          fprintf(logFile, "OSS: Process %d has terminated\n", popped->pid);
          logEntries++;
        }
        clearBit(message->number);
        resetMessage(message);
        aliveChildren--;
      }
      int index; 
      if((index = findPCB(pcb, lastDispatch)) != -1){  //output statistics from last running process
        //fprintf(stderr, "OSS: Process %d ran for %d:%d\n", pcb[index].pid, pcb[index].lastBurstTime.seconds, pcb[index].lastBurstTime.nanoseconds);
        switch(pcb[index].runMode){
          case 1:
            fprintf(stderr, "OSS: Process %d ran its entire quantum for %d:%d\n", pcb[index].pid, pcb[index].lastBurstTime.seconds, pcb[index].lastBurstTime.nanoseconds);
            if(logFile && logEntries < logEntryCutoff){
              fprintf(logFile, "OSS: Process %d ran its entire quantum for %d:%d\n", pcb[index].pid, pcb[index].lastBurstTime.seconds, pcb[index].lastBurstTime.nanoseconds);
              logEntries++;
            }
            break;
          case 2:
            fprintf(stderr, "OSS: Process %d started waiting for an event\n", pcb[index].pid);
            if(logFile && logEntries < logEntryCutoff){
              fprintf(logFile, "OSS: Process %d started waiting for an event\n", pcb[index].pid);
              logEntries++;
            }
            break;
          case 3:
            fprintf(stderr, "OSS: Process %d preempted; ran for %d:%d\n", pcb[index].pid, pcb[index].lastBurstTime.seconds, pcb[index].lastBurstTime.nanoseconds);
            if(logFile && logEntries < logEntryCutoff){
              fprintf(logFile, "OSS: Process %d preempted; ran for %d:%d\n", pcb[index].pid, pcb[index].lastBurstTime.seconds, pcb[index].lastBurstTime.nanoseconds);
              logEntries++;
            }
            break;
        }
        /* find out how long the cpu was idle*/
        sim_clock_t diff;
        diff = findDifference(simClock, &pcb[index].timeLastDispatch);
        diff = findDifference(&diff, &pcb[index].lastBurstTime);
        fprintf(stderr, "OSS: Time since last dispatch %d:%d\n", diff.seconds, diff.nanoseconds);
        if(logFile && logEntries < logEntryCutoff){
          fprintf(logFile, "OSS: Time since last dispatch %d:%d\n", diff.seconds, diff.nanoseconds);
          logEntries++;
        }
        sumSimClocks(&cpuIdleTime, &diff);
        /*-----------------------------------*/
      }
      if(aliveChildren == 0) break;
      schedule();
      if((lastDispatch = dispatchNext(0)) == -1000){  //no available processes, but some are in blocked queue
        fprintf(stderr, "OSS: No processes are available to run, some are still blocked at time %d:%d.\n", simClock->seconds, simClock->nanoseconds);
        if(logFile && logEntries < logEntryCutoff){
          fprintf(logFile, "OSS: No processes are available to run, some are still blocked.\n");
          logEntries++;
        }
        sem_post(semaphore);  
      }
    }
  }
  printQueue(queue[activeQueue][0]);
  findAverage(&averageTurnaroundTime, childCounter);
  findAverage(&averageWaitTime, childCounter);
  fprintf(stderr, "\n\nOSS: Average Turn Around -> %d:%d\n", averageTurnaroundTime.seconds, averageTurnaroundTime.nanoseconds);
  fprintf(stderr, "OSS: Average Wait Time -> %d:%d\n", averageWaitTime.seconds, averageWaitTime.nanoseconds);
  fprintf(stderr, "OSS: CPU Downtime -> %d:%d\n\n", cpuIdleTime.seconds, cpuIdleTime.nanoseconds);
  


  raise(SIGINT);
  //cleanUp(2);

  return 0;
}


