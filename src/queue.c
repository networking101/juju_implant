#include <stdlib.h>
#include <stdio.h>

#include "queue.h"

struct Queue* createQueue(unsigned capacity){
	struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (void**)malloc(queue->capacity * sizeof(void**));
	return queue;
}

int isFull(struct Queue* queue){
	return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue){
	return(queue->size == 0);
}

void enqueue(struct Queue* queue, void* item){
	if (isFull(queue))
		return;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
//	printf("DEBUG %p enqueued to queue\n", item);
}

void* dequeue(struct Queue* queue){
	if (isEmpty(queue))
		return NULL;
	void* item = queue->array[queue->front];
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

void* front(struct Queue* queue){
	if(isEmpty(queue))
		return NULL;
	return queue->array[queue->front];
}

void* rear(struct Queue* queue){
	if (isEmpty(queue))
		return NULL;
	return queue->array[queue->rear];
}


