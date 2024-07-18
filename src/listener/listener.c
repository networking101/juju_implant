#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <poll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "globals.h"
#include "listener_comms.h"
#include "console.h"
#include "queue.h"
#include "message_handler.h"

#define FDSIZE			10
#define LISTEN_BACKLOG	3
#define BUFFER_SIZE		256
#define AGENT_TIMEOUT	60

// Globals
// Socket Poll Structure
struct implant_poll *poll_struct;
pthread_mutex_t poll_lock;
// Receive Queue
struct Queue* receive_queue;
pthread_mutex_t receive_queue_lock;
// Send Queue
struct Queue* send_queue;
pthread_mutex_t send_queue_lock;

int start_sockets(int port){
    int sockfd, new_socket;
    int opt = 1;
    struct sockaddr_in addr;
    char *buffer;
    socklen_t addr_len = sizeof(addr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket failed\n");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        printf("setsockopt failed\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        printf("bind failed\n");
        return -1;
    }

    if (listen(sockfd, LISTEN_BACKLOG) < 0){
        printf("listen failed\n");
        return -1;
    }

    while(1){
        if ((new_socket = accept(sockfd, (struct sockaddr*)&addr, &addr_len)) < 0){
            printf("accept failed\n");
            return -1;
        }
        printf("DEBUG Agent connected %d\n", new_socket);
        
        if (new_socket >= FDSIZE){
			printf("too many connections\n");
			close(new_socket);
			continue;
		}
        
        poll_add(new_socket);
    }
    
    close(sockfd);
    return 0;
}
int main(int argc, char *argv[]){
    int opt;
    int port = 55555;
    pthread_t agent_receive_tid, agent_send_tid, console_tid, message_handler_tid;
    struct pollfd *pfds;
    int fd_count = 0;
    int fd_size = FDSIZE;

    poll_struct = malloc(sizeof(poll_struct));
    pfds = malloc(sizeof *pfds * fd_size);
    
    while((opt = getopt(argc, argv, "p:h")) != -1){
        switch(opt){
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("Help\n");
                return 0;
            case '?':
                printf("Unknown argument %c\n", optopt);
                return -1;
            case ':':
                printf("Missing argument %c\n", optopt);
                return -1;
        }
    }
    
    poll_struct->fd_count = &fd_count;
    poll_struct->pfds = pfds;

    // Start agent receive thread
    pthread_create(&agent_receive_tid, NULL, agent_receive, NULL);
    // Start agent send thread
    pthread_create(&agent_send_tid, NULL, agent_send, NULL);
    // Start console thread
    pthread_create(&console_tid, NULL, console, NULL);
    // Start message handler thread
    pthread_create(&message_handler_tid, NULL, handle_message, NULL);
    
    // Create receive queue
    receive_queue = createQueue(1000);
    pthread_mutex_init(&receive_queue_lock, NULL);
    // Create send queue
    send_queue = createQueue(1000);
    pthread_mutex_init(&send_queue_lock, NULL);

    start_sockets(port);
    return 0;
}