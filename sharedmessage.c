/*
 * $Author: o1-mccune $
 * $Date: 2017/10/23 01:22:48 $
 * $Revision: 1.2 $
 * $Log: sharedmessage.c,v $
 * Revision 1.2  2017/10/23 01:22:48  o1-mccune
 * Milestone 1
 *
 * Revision 1.1  2017/10/18 03:46:35  o1-mccune
 * Initial revision
 *
 */

#include "sharedmessage.h"

void resetMessage(shared_message_t *message){
  message->number = -1;
}

int messageEmpty(shared_message_t *message){
  if(message->number >= 0) return 1;
  return 0;
}

void setMessage(shared_message_t *message, int pid){
  message->number = pid;
}
