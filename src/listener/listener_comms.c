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
// Agents poll and states
extern Connected_Agents *connected_agents;
extern pthread_mutex_t agents_lock;
// Receive Queue
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
// Send Queue
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;
// SIGINT flag
extern volatile sig_atomic_t all_sigint;

STATIC int listener_receive(){
	Fragment fragment;
	char* buffer;
	
	int *fd_count = connected_agents->fd_count;
	struct pollfd *pfds = connected_agents->pfds;
	
	pthread_mutex_lock(&agents_lock);
	int poll_count = poll(pfds, *fd_count, POLL_TIMEOUT);
	
	for (int i = 0; i < *fd_count; i++){
		// check if ready to read
		if (pfds[i].revents & POLLIN){
			debug_print("POLLIN  %d\n", pfds[i].fd);
			int nbytes = recv(pfds[i].fd, (void*)&fragment, sizeof(Fragment), 0);
			
			if (nbytes == 0){
				// close file descriptor and remove from poll array
				printf("%s\n", "connection closed");
				agent_delete(i);
			}
			else if (nbytes == -1){
				printf("ERROR recv error\n");
				pthread_mutex_unlock(&agents_lock);
				return RET_ERROR;
			}
			else{
				debug_print("received fragment: type: %d, index: %d, size: %d\n", ntohl(fragment.type), ntohl(fragment.index), nbytes);		
				
				Queue_Message* message = malloc(sizeof(message));
				message->fragment = malloc(nbytes);
				message->id = pfds[i].fd;
				message->size = nbytes;
				memset(message->fragment, 0, nbytes);
				memcpy(message->fragment, &fragment, nbytes);
				
				debug_print("adding message to queue: id: %d, size: %d\n", message->id, message->size);
				enqueue(listener_receive_queue, &listener_receive_queue_lock, message);
			}
		}
	}
	pthread_mutex_unlock(&agents_lock);
	
    return RET_OK;
}

STATIC int listener_send(){
	Queue_Message* message;

	if (!(message = dequeue(listener_send_queue, &listener_send_queue_lock))) return RET_OK;
	
	int nbytes = send(message->id, message->fragment, message->size, 0);
	if (nbytes != message->size){
		printf("Send error\n");
		return RET_ERROR;
	}
	
	free(message->fragment);
	free(message);

	return RET_OK;
}


void *listener_receive_thread(void *vargp){
	while(!all_sigint){
		if (listener_receive() != RET_OK){
			all_sigint = true;
			printf("listener_receive_thread error\n");
		}
	}
	printf("listener_receive_thread done\n");
	return 0;
}

void *listener_send_thread(void *vargp){
	while(!all_sigint){
		if (listener_send() != RET_OK){
			all_sigint = true;
			printf("listener_send_thread error\n");
		}
	}
	printf("listener_send_thread done\n");
	return 0;
}