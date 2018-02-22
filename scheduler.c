/*
 * $Author: o1-mccune $
 * $Date: 2017/10/23 01:22:13 $
 * $Revision: 1.1 $
 * $Log: scheduler.c,v $
 * Revision 1.1  2017/10/23 01:22:13  o1-mccune
 * Initial revision
 *
 */

#include "pcb.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


static FILE *outFile = NULL;


static queue_item_t *head0;
static queue_item_t *head1;
static queue_item_t *head2;

static queue_item_t *current0;
static queue_item_t *current1;
static queue_item_t *current2;

int initPriorityScheduler(FILE *logFile){
  if(!logFile) outFile = stderr;
  return 1;
}

void insert(pcb_t *pcb){
  queue_item_t *entry = malloc(sizeof(queue_item_t));
  entry->pcb = pcb;

  if(!head0){  //list is empty
    head0 = entry;
    head0->next = entry;
    head0->previous = entry;
  }
  else{  //add to tail
    queue_item_t *tail = head0->previous;
    tail->next = entry;
    entry->next = head0;
    entry->previous = tail;
    head0->previous = entry;
  }
}

void printQueue(){
  fprintf(stderr, "--Scheduling Queue--\n");
  if(!head0) return;
  queue_item_t *iterator = head0;
  while(iterator->next != head0){
    fprintf(stderr, "\tPID: %d\n", iterator->pcb->pid);
    iterator = iterator->next;
  }
  fprintf(stderr, "\tPID: %d\n", iterator->pcb->pid);
}



void removeQueueItem(pcb_t *pcb){
  if(!head0) return;  //if queue is empty; return
  if((head0->previous == head0 && head0->next == head0) && head0->pcb == pcb) head0 = NULL; //only 1 item in queue and it has pcb; set head pointer to NULL
  else{  //traverse queue looking for pcb
    queue_item_t *iterator = head0;
    while(iterator->pcb != pcb){  //break when iterator contain pcb or iterator is at last element
      if((iterator = iterator->next) == head0) break;
    }
    if(iterator->pcb == pcb){  //pcb found; remove this item
      iterator->previous->next = iterator->next;
      iterator->next->previous = iterator->previous;
      if(iterator == head0) head0 = iterator->next;
    }
  }
}


void schedule(pcb_t *pcb, const unsigned int size){
  queue_item_t *entry;
  unsigned int iterator = 0;
  
  if(!head0){  //queue0 is empty
    entry = malloc(sizeof(queue_item_t));
    entry->pcb = &pcb[iterator];
    entry->next = NULL;
  } else{  //queue0 is not empty; add to tail
    
  }
}

void dispatch(){
  
}
