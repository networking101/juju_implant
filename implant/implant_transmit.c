#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>

#include "implant_transmit.h"

void *implant_receive(void *vargp){
	return 0;	
}

void *implant_send(void *vargp){
	char *alive = "ALIVE\n";
	int *sockfd = (int*)vargp;
	
	for (;;){
		send(*sockfd, alive, strlen(alive), 0);
		printf("Sent message\n");
		sleep(1);
	}
	return 0;	
}