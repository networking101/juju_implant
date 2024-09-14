#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "utility.h"
#include "queue.h"

Queue* createQueue(unsigned capacity){
	Queue* queue = (Queue*)malloc(sizeof(Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (void**)malloc(queue->capacity * sizeof(void**));
	return queue;
}

void destroyQueue(Queue* queue){
	free(queue->array);
	free(queue);
}

int isFull(Queue* queue){
	return (queue->size == queue->capacity);
}

int isEmpty(Queue* queue){
	return(queue->size == 0);
}

void enqueue(Queue* queue, pthread_mutex_t* mutex, void* item){
	pthread_mutex_lock(mutex);
	if (isFull(queue)){
		pthread_mutex_unlock(mutex);
		return;
	}
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
	pthread_mutex_unlock(mutex);
//	debug_print("%p enqueued to queue\n", item);
}

void* dequeue(Queue* queue, pthread_mutex_t* mutex){
	pthread_mutex_lock(mutex);
	if (isEmpty(queue)){
		pthread_mutex_unlock(mutex);
		return NULL;
	}
	void* item = queue->array[queue->front];
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	pthread_mutex_unlock(mutex);
//	debug_print("%p dequeued from queue\n", item);
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


