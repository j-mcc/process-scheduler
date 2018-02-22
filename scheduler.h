/*
 * $Author: o1-mccune $
 * $Date: 2017/10/26 04:53:08 $
 * $Revision: 1.4 $
 * $Log: scheduler.h,v $
 * Revision 1.4  2017/10/26 04:53:08  o1-mccune
 * final
 *
 * Revision 1.3  2017/10/21 00:16:52  o1-mccune
 * Removed function prototypes because functions were moved to static functions in oss.c
 * changed queue_item_t struct
 *
 * Revision 1.2  2017/10/19 03:44:31  o1-mccune
 * created queue_item_t for storing processes in a queue
 *
 * Revision 1.1  2017/10/18 16:57:01  o1-mccune
 * Initial revision
 *
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include <sys/types.h>

#define QUANTUM 20000000  //nanoseconds
#define ALPHA 5
#define BETA 5


typedef struct queue_item_t{
  pcb_t *pcb;
  struct queue_item_t *previous;
  struct queue_item_t *next;
}queue_item_t;

typedef struct{
  pid_t pid;
  unsigned int quantum; 
}dispatch_memory_t;

/*
void insert(pcb_t *pcb);

void removeQueueItem(pcb_t *pcb);

void printQueue();

void dispatch();

void schedule();
*/


#endif
