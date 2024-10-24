#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

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
// SIGINT flag
extern volatile sig_atomic_t agent_close_flag;
// Connected flag
extern volatile sig_atomic_t agent_disconnect_flag;

static int agent_receive(int *sockfd, int *next_read_size){
	int retval = RET_OK;
	Queue_Message* message;
	Fragment fragment;
	
	// get first chunk which contains total message size
	int nbytes = recv(*sockfd, (void*)&fragment, HEADER_SIZE + *next_read_size, 0);
	if (nbytes == -1){
		if (errno != EAGAIN){				// EAGAIN means recv timed out
			printf("ERROR recv");
			retval = RET_FATAL_ERROR;
		}
	}
	else if(nbytes == 0){
		debug_print("%s\n", "connection closed");
		agent_disconnect_flag = true;
	}
	else{
		// Convert each value of header to host order
		// this only works if each header value is 32 bits long
		for (int32_t* value = (int32_t*)&fragment; value < (int32_t*)&fragment.buffer; value++){
			*value = ntohl(*value);
		}
		
		debug_print("received fragment: type: %d, index: %d, total_size: %d, next_size: %d\n", \
			fragment.header.type, \
			fragment.header.index, \
			fragment.header.total_size, \
			fragment.header.next_size);

		*next_read_size = fragment.header.next_size;

		if ((message = malloc(sizeof(Queue_Message))) == NULL) return RET_FATAL_ERROR;
		if ((message->fragment = calloc(1, nbytes + 1)) == NULL) return RET_FATAL_ERROR;
		message->id = -1;
		message->size = nbytes;
		memcpy(message->fragment, &fragment, nbytes);
		
		pthread_mutex_lock(&agent_receive_queue_lock);
		enqueue(agent_receive_queue, message);
		pthread_mutex_unlock(&agent_receive_queue_lock);

		memset(&fragment, 0, sizeof(fragment));
	}

	return retval;	
}

int agent_send(int *sockfd){
	Queue_Message* message;
	Fragment* fragment;

	pthread_mutex_lock(&agent_send_queue_lock);
	message = dequeue(agent_send_queue);
	pthread_mutex_unlock(&agent_send_queue_lock);
	if (!message) return RET_OK;

	fragment = message->fragment;
	
	debug_print("sending fragment: type: %d, index: %d, total_size: %d, next_size: %d\n", \
		fragment->header.type, \
		fragment->header.index, \
		fragment->header.total_size, \
		fragment->header.next_size);

	// Convert each value of header to network order
	// this only works if each header value is 32 bits long
	for (int32_t* value = (int32_t*)fragment; value < (int32_t*)&fragment->buffer; value++){
		*value = htonl(*value);
	}
	
	if (sendall(*sockfd, (void*)message->fragment, message->size) == RET_FATAL_ERROR){
		printf("Send error\n");
		return RET_FATAL_ERROR;
	}
	
	free(message->fragment);
	free(message);

	return 0;	
}

void *agent_receive_thread(void *vargp){
	int *sockfd = (int*)vargp;
	int next_read_size = 0;

	debug_print("%s\n", "starting agent receive thread");

	while(!agent_close_flag && !agent_disconnect_flag){
		if (agent_receive(sockfd, &next_read_size) != RET_OK){
			agent_close_flag = true;
		}
	}
	debug_print("%s\n", "agent_receive_thread done");
	return 0;
}

void *agent_send_thread(void *vargp){
	int *sockfd = (int*)vargp;

	debug_print("%s\n", "starting agent send thread");

	while(!agent_close_flag && !agent_disconnect_flag){
		if (agent_send(sockfd) != RET_OK){
			agent_close_flag = true;
		}
	}
	debug_print("%s\n", "agent_send_thread done");
	return 0;
}