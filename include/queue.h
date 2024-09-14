#include "implant.h"

#ifndef _QUEUE_H
#define _QUEUE_H

#define QUEUE_SIZE		10000

#ifndef QUEUE_STRUCTURE
#define QUEUE_STRUCTURE

typedef struct Queue{
	int front, rear, size;
	unsigned capacity;
	void** array;
} Queue;

#endif /* QUEUE_STRUCTURE */

#ifndef QUEUE_MESSAGE_STRUCTURE
#define QUEUE_MESSAGE_STRUCTURE

typedef struct Queue_Message{
	int32_t id;
	int32_t size;
	union{
		Fragment* fragment;
		char* buffer;
	};
} Queue_Message;

#endif /* QUEUE_MESSAGE_STRUCTURE */

Queue* createQueue(unsigned);

void destroyQueue(Queue*);

int isFull(Queue*);

int isEmpty(Queue*);

void enqueue(Queue*, pthread_mutex_t*, void*);

void* dequeue(Queue*, pthread_mutex_t*);

void* front(Queue*);

void* rear(Queue*);

#endif /* _QUEUE_H */

