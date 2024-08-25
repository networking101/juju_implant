#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "agent_comms.h"

// Global variables
// Receive Queue
extern struct Queue* agent_receive_queue;
extern pthread_mutex_t agent_receive_queue_lock;
// Send Queue
extern struct Queue* agent_send_queue;
extern pthread_mutex_t agent_send_queue_lock;

void *agent_receive(void *vargp){
	int *sockfd = (int*)vargp;
	Fragment fragment;
	char* buffer;
	
	for (;;){
		sleep(1);
		uint message_size = 0;
		uint bytes_received = 0;
		
		// get first chunk which contains total message size
		int nbytes = recv(*sockfd, (void*)&fragment, sizeof(Fragment), 0);
		if (nbytes <= 0){
			if (nbytes == 0){
				debug_print("%s\n", "connection closed");
			}
			else {
				printf("recv error\n");
			}
			exit(-1);
		}
		
		debug_print("received fragment: type: %d, index: %d, size: %d\n", ntohl(fragment.type), ntohl(fragment.index), nbytes);
		
		Queue_Message* message = malloc(sizeof(message));
		message->fragment = malloc(nbytes);
		message->id = -1;
		message->size = nbytes;
		memset(message->fragment, 0, nbytes);
		memcpy(message->fragment, &fragment, nbytes);
		
		enqueue(agent_receive_queue, &agent_receive_queue_lock, message);
	}
	return 0;	
}

void *agent_send(void *vargp){
	int *sockfd = (int*)vargp;
	Queue_Message* message;
	
	for(;;){
		if (!(message = dequeue(agent_send_queue, &agent_send_queue_lock))) continue;
		
		debug_print("sending fragment: type: %d, index: %d, size: %d\n", ntohl(message->fragment->type), ntohl(message->fragment->index), ntohl(message->fragment->first_payload.total_size));
		
		int nbytes = send(*sockfd, message->fragment, message->size, 0);
		if (nbytes != message->size){
			printf("Send error\n");
			exit(-1);
		}
		
		free(message->fragment);
		free(message);
	}
	return 0;	
}


