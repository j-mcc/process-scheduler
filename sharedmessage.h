/*
 * $Author: o1-mccune $
 * $Date: 2017/10/23 01:23:03 $
 * $Revision: 1.2 $
 * $Log: sharedmessage.h,v $
 * Revision 1.2  2017/10/23 01:23:03  o1-mccune
 * Milestone 1
 *
 * Revision 1.1  2017/10/18 03:51:45  o1-mccune
 * Initial revision
 *
 */

#ifndef SHAREDMESSAGE_H
#define SHAREDMESSAGE_H

typedef struct{
 int number;
}shared_message_t;

void resetMessage(shared_message_t *message);

int messageEmpty(shared_message_t *message);

void setMessage(shared_message_t *message, int pid);

#endif
