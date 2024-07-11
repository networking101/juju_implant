#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IP_SIZE 16
#define LISTEN_BACKLOG 3
#define BUFFER_SIZE 256

int connect_from_implant(int port){
    int sockfd, new_socket;
    int opt = 1;
    struct sockaddr_in addr;
    char* buffer;
    char* message = "Listener Alive\n";
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

    if ((new_socket = accept(sockfd, (struct sockaddr*)&addr, &addr_len)) < 0){
        printf("accept failed\n");
        return -1;
    }

    buffer = malloc(BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);

    while(read(new_socket, buffer, BUFFER_SIZE - 1)){
        printf("%s", buffer);
        send(new_socket, message, strlen(message), 0);
        sleep(1);
    }

    free(buffer);
    close(new_socket);
    close(sockfd);
    return 0;
}
int main(int argc, char *argv[]){
    int opt;
    int port = 8080;

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

    connect_from_implant(port);
    return 0;
}