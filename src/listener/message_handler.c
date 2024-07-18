#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "queue.h"
#include "implant.h"

extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

void handle_message(void*){
	struct Message* message;
	
	for (;;){
		if (isEmpty(receive_queue))
			continue;
		
		pthread_mutex_lock(&receive_queue_lock);
		message = dequeue(receive_queue);
		pthread_mutex_unlock(&receive_queue_lock);
		
		//printf("DEBUG RECEIVED MESSAGE: %d, %s\n", message->sockfd, message->buffer);
		free(message->buffer);
		free(message);
	}
	return;
}

int send_message(){
	return 0;
}