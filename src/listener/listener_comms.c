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
#include <arpa/inet.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "base.h"
#include "listener_comms.h"

// Global variables
// Socket Poll Structure
extern implant_poll* poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
// Send Queue
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;

void *receive_from_agent(void *vargp){
	Fragment fragment;
	char* buffer;
	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
    for (;;){
		int poll_count = poll(pfds, *fd_count, 1000);
		
		for (int i = 0; i < *fd_count; i++){
			
			// check if ready to read
			if (pfds[i].revents & POLLIN){
				debug_print("POLLIN  %d\n", pfds[i].fd);
				int nbytes = recv(pfds[i].fd, (void*)&fragment, sizeof(Fragment), 0);
				
				if (nbytes == 0){
					// close file descriptor and remove from poll array
					printf("%s\n", "connection closed");
					poll_delete(i);
				}
				else if (nbytes == -1){
					printf("ERROR recv error\n");
					exit(-1);
				}
				else{
					debug_print("received fragment: type: %d, index: %d, size: %d\n", ntohl(fragment.type), ntohl(fragment.index), nbytes);		
					
					Queue_Message* message = malloc(sizeof(message));
					message->fragment = malloc(nbytes);
					message->id = pfds[i].fd;
					message->fragment_size = nbytes;
					memset(message->fragment, 0, nbytes);
					memcpy(message->fragment, &fragment, nbytes);
					
					debug_print("adding message to queue: id: %d, fragment_size: %d\n", message->id, message->fragment_size);
					enqueue(listener_receive_queue, &listener_receive_queue_lock, message);
				}
			}
		}
	}
	
    return 0;
}

void *send_to_agent(void *vargp){
	Queue_Message* message;
	
	for(;;){
		sleep(1);
		if (!(message = dequeue(listener_send_queue, &listener_send_queue_lock))) continue;
		
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