#include "implant.h"

#define QUEUE_SIZE		10000

#ifndef QUEUE_STRUCTURE
#define QUEUE_STRUCTURE

typedef struct Queue{
	int front, rear, size;
	unsigned capacity;
	void** array;
} Queue;

#endif

#ifndef QUEUE_MESSAGE_STRUCTURE
#define QUEUE_MESSAGE_STRUCTURE

typedef struct Queue_Message{
	int32_t id;
	int32_t fragment_size;
	Fragment* fragment;
} Queue_Message;

#endif

struct Queue* createQueue(unsigned);

int isFull(struct Queue*);

int isEmpty(struct Queue*);

void enqueue(struct Queue*, void*);

void* dequeue(struct Queue*);

void* front(struct Queue*);

void* rear(struct Queue*);