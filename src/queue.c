#include <stdlib.h>
#include <stdio.h>

#include "queue.h"

Queue* createQueue(unsigned capacity){
	Queue* queue = (Queue*)malloc(sizeof(Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (void**)malloc(queue->capacity * sizeof(void**));
	return queue;
}

int isFull(Queue* queue){
	return (queue->size == queue->capacity);
}

int isEmpty(Queue* queue){
	return(queue->size == 0);
}

void enqueue(Queue* queue, void* item){
	if (isFull(queue))
		return;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
//	printf("DEBUG %p enqueued to queue\n", item);
}

void* dequeue(Queue* queue){
	if (isEmpty(queue))
		return NULL;
	void* item = queue->array[queue->front];
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

void* front(Queue* queue){
	if(isEmpty(queue))
		return NULL;
	return queue->array[queue->front];
}

void* rear(Queue* queue){
	if (isEmpty(queue))
		return NULL;
	return queue->array[queue->rear];
}


