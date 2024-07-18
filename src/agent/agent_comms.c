#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>

#include "agent_comms.h"

#define BUFFSIZE 256

void *agent_receive(void *vargp){
	int *sockfd = (int*)vargp;
	char buf[BUFFSIZE];
	
	for (;;){
		int nbytes = recv(*sockfd, buf, sizeof(buf), 0);
		if (nbytes <= 0){
			(nbytes == 0) ? printf("connection closed\n") : printf("recv error\n");
			exit(0);
		}
		printf("DEBUG Recieved: %s", buf);
	}
	return 0;	
}

void *agent_send(void *vargp){
	char *alive = "ALIVE\n";
	int *sockfd = (int*)vargp;
	
	for (;;){
		send(*sockfd, alive, strlen(alive), 0);
		printf("Sent message\n");
		sleep(1);
	}
	return 0;	
}