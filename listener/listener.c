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

#include "agent_transmit.h"
#include "console.h"

#define FDSIZE			5
#define LISTEN_BACKLOG	3
#define BUFFER_SIZE		256
#define AGENT_TIMEOUT	60

int start_sockets(struct agent_receive *ARS, int port){
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
        
        if (*(ARS->fd_count) == FDSIZE){
			printf("too many connection\n");
			close(new_socket);
			continue;
		}
        
        ARS->pfds[*(ARS->fd_count)].fd = new_socket;
        ARS->pfds[*(ARS->fd_count)].events = POLLIN;
        *(ARS->fd_count) = *(ARS->fd_count) + 1;
    }
    
    close(sockfd);
    return 0;
}
int main(int argc, char *argv[]){
    int opt;
    int port = 55555;
    pthread_t agent_receive_tid, agent_send_tid, console_tid;
    struct pollfd *pfds;
    int fd_count = 0;
    int fd_size = FDSIZE;
    struct agent_receive *ARS;

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
    
    ARS->fd_count = &fd_count;
    ARS->pfds = pfds;

    // Start agent receive thread
    pthread_create(&agent_receive_tid, NULL, agent_receive, ARS);
    // Start agent send thread
    pthread_create(&agent_send_tid, NULL, agent_send, ARS);
    // Start console
    pthread_create(&console_tid, NULL, console, ARS);

    start_sockets(ARS, port);
    return 0;
}