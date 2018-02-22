/*
 * $Author: o1-mccune $
 * $Date: 2017/10/25 03:47:26 $
 * $Revision: 1.2 $
 * $Log: bitarray.c,v $
 * Revision 1.2  2017/10/25 03:47:26  o1-mccune
 * Milestone 2. All queues are working.
 *
 * Revision 1.1  2017/10/17 02:48:16  o1-mccune
 * Initial revision
 *
 */

/* Created with help from 'The Bit Twiddler' http://bits.stephan-brumme.com/basics.html */

#include "bitarray.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

static unsigned char *bitVector = NULL;
static double bytes = 0;
static unsigned int slots;

void clearAll(){
  int i;
  for(i = 0; i < bytes; i++) bitVector[i] = MIN_VALUE;
}

void setAll(){
  int i;
  for(i = 0; i < bytes; i++) bitVector[i] = MAX_VALUE;
}

void printByte(unsigned char byte){
  int i;
  for(i = 0; i < CHAR_BIT; i++) fprintf(stderr, "%d", getBit(i));
}

int initBitVector(unsigned int size, unsigned char initialValue){
  if(size <= 0) return -1;
  slots = size;
  bytes = ceil((size * 1.)/(CHAR_BIT * 1.));
  if((bitVector = malloc(bytes)) == NULL) return -1;
  if(initialValue == MIN_VALUE) clearAll();
  if(initialValue == MAX_VALUE) setAll();
  return 0;
}

int isFull(){
  int i;
  for(i = 0; i < slots; i++){
    if(!getBit(i)) return 0;
  }
  return 1;
}

int isEmpty(){
  int i;
  for(i = 0; i < slots; i++){
    if(getBit(i)) return 0;
  }
  return 1;
}

void freeBitVector(){
  if(bitVector) free(bitVector);
}

int getBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned int byte = floor(index / CHAR_BIT);
  unsigned char x = bitVector[byte];
  x >>= (index % CHAR_BIT);
  return (x & 1) != 0;
}

int setBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1 << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] |= mask;
  return 0;  
}

int flipBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] ^= mask;
  return 0;
}

int clearBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] &= ~mask;
  return 0;
}

void printBitVector(){
  int i;
  for(i = 0; i < (CHAR_BIT * bytes); i++){
    if(i % CHAR_BIT == 0) fprintf(stderr, " ");
    fprintf(stderr, "%d", getBit(i));
  }
  fprintf(stderr, "\n");
}


/*
int main(){
  initVector(3, MIN_VALUE);
  printArray();
  if(setBit(4) == -1) fprintf(stderr, "Setting bit 4 failed\n");
  if(setBit(0) == -1) fprintf(stderr, "Setting bit 0 failed\n");
  if(setBit(14) == -1) fprintf(stderr, "Setting bit 14 failed\n");
  printArray();
  return 0;
}
*/
