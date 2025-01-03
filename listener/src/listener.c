/*
listener.c

This is the main file for the listener. It will do the following:
* spawn threads
* create the listening socket
* listen for new agents to connect

*/

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "listener_utility.h"
#include "listener_comms.h"
#include "console.h"
#include "listener_handler.h"

#define LISTEN_BACKLOG	3
#define DEFAULT_PORT	55555

// Globals
// Receive Queue
Queue* listener_receive_queue;
pthread_mutex_t listener_receive_queue_lock;
// Send Queue
Queue* listener_send_queue;
pthread_mutex_t listener_send_queue_lock;
// Alarm flags
volatile sig_atomic_t listener_alive_flag = false;
// SIGINT flags
volatile sig_atomic_t listener_exit_flag = false;
volatile sig_atomic_t shell_sigint = false;
volatile sig_atomic_t shell_flag = false;

void sigint_handler(int sig){
    (shell_flag) ? (shell_sigint = true) : exit(RET_OK);
}

void listener_alive_alarm(int sig){
	listener_alive_flag = true;
}

void *check_alive_thread(void *vargp){
    int retval;
    Connected_Agents* CA = vargp;
    struct sigaction sa = {0};
    struct timespec tv;

    // set signal hander for SIGALRM
    sa.sa_handler = &listener_alive_alarm;
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1){
        printf("ERROR sigaction SIGALRM\n");
        listener_exit_flag = true;
    }

    alarm(ALIVE_FREQUENCY);
	while (!listener_exit_flag){
        tv.tv_sec = 0;
		tv.tv_nsec = TIMEOUT_CONST * 100000000;	// .1 seconds
		if (listener_alive_flag){
            pthread_mutex_lock(&CA->lock);
			// for (int i = 0; i < FDSIZE; i++){
            for (int i = 0; i < 10; i++){
				if (CA->agents[i].alive && CA->agents[i].alive + AGENT_TIMEOUT < time(NULL)){
					printf("Agent %d timeout, removing from poll\n", i);
					agent_delete(CA, i);
				}
			}
            pthread_mutex_unlock(&CA->lock);
			
			listener_alive_flag = false;
            alarm(ALIVE_FREQUENCY);
		}
        retval = nanosleep(&tv, &tv);
        if (retval == -1 && errno != EINTR){
            printf("ERROR sigaction SIGALRM\n");
            listener_exit_flag = true;
        }
	}
    debug_print("%s\n", "check_alive_thread done");
	return 0;
}

int start_sockets(Connected_Agents* CA, int port){
    int listener_socket, agent_socket;
    int opt = 1;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    struct pollfd listener_pfd[1];
    int poll_count;

    if ((listener_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket failed\n");
        listener_exit_flag = true;
        return RET_FATAL_ERROR;
    }

    if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        printf("setsockopt failed\n");
        listener_exit_flag = true;
        return RET_FATAL_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listener_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("bind failed\n");
        listener_exit_flag = true;
        return RET_FATAL_ERROR;
    }

    if (listen(listener_socket, LISTEN_BACKLOG) < 0){
        printf("listen failed\n");
        listener_exit_flag = true;
        return RET_FATAL_ERROR;
    }

    listener_pfd[0].fd = listener_socket;
    listener_pfd[0].events = POLLIN;

    while (!listener_exit_flag){
        poll_count = poll(listener_pfd, 1, TIMEOUT_CONST * 100);     // .1 seconds
        if (poll_count == -1 && errno != EINTR){
            printf("ERROR poll\n");
            listener_exit_flag = true;
            return RET_FATAL_ERROR;
	    }

        if (poll_count > 0 && listener_pfd[0].revents & POLLIN){
            if ((agent_socket = accept(listener_socket, (struct sockaddr*)&addr, &addr_len)) < 0){
                printf("accept failed\n");
                listener_exit_flag = true;
            }
            else if (agent_socket >= FDSIZE){
                print_out("%s\n", "Agent connected but too many connections");
                close(agent_socket);
            }
            else {
                // add agent to global struct
                print_out("Agent connected %d\n", agent_socket);
                pthread_mutex_lock(&CA->lock);
                agent_add(CA, agent_socket);
                pthread_mutex_unlock(&CA->lock);
            }
        }
    }

    close(listener_socket);
    debug_print("%s\n", "listening socket done");
    return RET_OK;
}

#ifndef UNIT_TESTING
int main(int argc, char *argv[]){
    int opt;
    int port = DEFAULT_PORT;
    pthread_t listener_receive_tid, listener_send_tid, console_tid, listener_handler_tid, check_alive_tid;
    struct sigaction sa = {0};
    Connected_Agents *CA;
    
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
                return RET_FATAL_ERROR;
            case ':':
                printf("Missing argument %c\n", optopt);
                return RET_FATAL_ERROR;
        }
    }
    
    // set signal hander for SIGINT
    sa.sa_handler = &sigint_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1){
        printf("ERROR sigaction SIGINT\n");
        return RET_FATAL_ERROR;
    }

    // Setup Connected_Agents struct
    if ((CA = calloc(1, sizeof(Connected_Agents))) == NULL) return RET_FATAL_ERROR;
    CA->nfds = FDSIZE;
    if ((CA->pfds = calloc(FDSIZE, sizeof(struct pollfd))) == NULL) return RET_FATAL_ERROR;
    if ((CA->agents = calloc(FDSIZE, sizeof(Agent))) == NULL) return RET_FATAL_ERROR;
    // create CA mutex
    pthread_mutex_init(&CA->lock, NULL);

    // Create receive queue
    listener_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_receive_queue_lock, NULL);
    // Create send queue
    listener_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_send_queue_lock, NULL);

    // Start agent receive thread
    pthread_create(&listener_receive_tid, NULL, listener_receive_thread, CA);
    // Start agent send thread
    pthread_create(&listener_send_tid, NULL, listener_send_thread, CA);
    // Start console thread
    pthread_create(&console_tid, NULL, console_thread, CA);
    // Start message handler thread
    pthread_create(&listener_handler_tid, NULL, listener_handler_thread, CA);
    // Start keep alive thread
    pthread_create(&check_alive_tid, NULL, check_alive_thread, CA);

    start_sockets(CA, port);

    // Clean up
    pthread_join(listener_receive_tid, NULL);
    pthread_join(listener_send_tid, NULL);
    pthread_join(console_tid, NULL);
    pthread_join(listener_handler_tid, NULL);
    pthread_join(check_alive_tid, NULL);

    pthread_mutex_destroy(&CA->lock);
    pthread_mutex_destroy(&listener_receive_queue_lock);
    pthread_mutex_destroy(&listener_send_queue_lock);

    destroyQueue(listener_receive_queue);
    destroyQueue(listener_send_queue);

    free(CA->pfds);
    free(CA->agents);
    free(CA);

    debug_print("%s\n", "closed cleanly");
    return RET_OK;
}
#endif /* UNIT_TESTING */


