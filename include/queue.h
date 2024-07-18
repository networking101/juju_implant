
#ifndef QUEUE_STRUCTURE
#define QUEUE_STRUCTURE

struct Queue{
	int front, rear, size;
	unsigned capacity;
	void** array;
};

#endif

struct Queue* createQueue(unsigned);

int isFull(struct Queue*);

int isEmpty(struct Queue*);

void enqueue(struct Queue*, void*);

void* dequeue(struct Queue*);

void* front(struct Queue*);

void* rear(struct Queue*);