#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>

#include "listener_comms.h"
#include "globals.h"
#include "queue.h"
#include "implant.h"

#define BUFFERSIZE 256

// Global variables
extern struct implant_poll* poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
// Send Queue
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

void *agent_receive(void *vargp){
	int nbytes;
	char buf[BUFFERSIZE];
	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
    for (;;){
		int poll_count = poll(pfds, *fd_count, 1000);
		
		for (int i = 0; i < *fd_count; i++){
			
			// check if ready to read
			if (pfds[i].revents & POLLIN){
//				printf("DEBUG POLLIN  %d\n", pfds[i].fd);
				nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
				
				// close file descriptor and remove from poll array
				if (nbytes <= 0){
					(nbytes == 0) ? printf("connection closed\n") : printf("recv error\n");
					poll_delete(i);
				} else{
//					printf("%s\n", buf);
					
					// Set up Message structure
					struct Message* message = malloc(sizeof(message));
					char* buffer = malloc(BUFFERSIZE);
					memcpy(buffer, buf, BUFFERSIZE);
					message->sockfd = pfds[i].fd;
					message->buffer = buffer;
					
					pthread_mutex_lock(&receive_queue_lock);
					enqueue(receive_queue, message);
					pthread_mutex_unlock(&receive_queue_lock);
				}
			}
		}
	}
	
    return 0;
}

void *agent_send(void *vargp){
	int nbytes;
	char* buf;
	struct Message* message;
	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
	for(;;){
		pthread_mutex_lock(&send_queue_lock);
		if (message = dequeue(send_queue)){
			if ((nbytes = send(message->sockfd, message->buffer, BUFFERSIZE, 0)) < 0){
				printf("send failed\n");
			}
			free(message->buffer);
			free(message);
		}
		pthread_mutex_unlock(&send_queue_lock);
	}
    return 0;
}