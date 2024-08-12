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

#include <sys/socket.h>

#include "queue.h"
#include "implant.h"
#include "base.h"
#include "listener_comms.h"

// Global variables
// Socket Poll Structure
extern struct implant_poll* poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
// Send Queue
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

bool check_alive(){
	return true;
}

void *agent_receive(void *vargp){
	Fragment fragment;
	char* buffer;
	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
    for (;;){
		int poll_count = poll(pfds, *fd_count, 1000);
		
		for (int i = 0; i < *fd_count; i++){
			
			// check if ready to read
			if (pfds[i].revents & POLLIN){
//				printf("DEBUG POLLIN  %d\n", pfds[i].fd);
				int nbytes = recv(pfds[i].fd, (void*)&fragment, sizeof(Fragment), 0);
				
				printf("DEBUG received fragment:\ttype: %d, index: %d, size: %d\n", fragment.type, fragment.index, nbytes);
				
				// close file descriptor and remove from poll array
				if (nbytes == 0){
					printf("INFO connection closed\n");
					poll_delete(i);
				}
				else if (nbytes == -1){
					printf("ERROR recv error\n");
					exit(-1);
				}
				else{
					Queue_Message* message = malloc(sizeof(message));
					message->fragment = malloc(nbytes);
					message->id = pfds[i].fd;
					message->fragment_size = nbytes;
					memset(message->fragment, 0, nbytes);
					memcpy(message->fragment, &fragment, nbytes);
					
					pthread_mutex_lock(&receive_queue_lock);
					printf("DEBUG adding message to queue:\tid: %d, fragment_size: %d\n", message->id, message->fragment_size);
					enqueue(receive_queue, message);
					pthread_mutex_unlock(&receive_queue_lock);
				}
			}
		}
	}
	
    return 0;
}

void *agent_send(void *vargp){
	Queue_Message* message;
	
//	int *fd_count = poll_struct->fd_count;
//	struct pollfd *pfds = poll_struct->pfds;
	
	for(;;){
		if (isEmpty(receive_queue))
			continue;
		
		pthread_mutex_lock(&send_queue_lock);
		message = dequeue(send_queue);
		pthread_mutex_unlock(&send_queue_lock);
		
		int nbytes = send(message->id, message->fragment, message->fragment_size, 0);
		if (nbytes != message->fragment_size){
			printf("Send error\n");
			exit(-1);
		}
		
		free(message->fragment);
		free(message);
	}
    return 0;
}