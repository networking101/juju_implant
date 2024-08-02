#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>

#include "implant.h"
#include "agent_comms.h"

void *agent_receive(void *vargp){
	int *sockfd = (int*)vargp;
	char buf[BUFFERSIZE];
	char* buffer;
	
	for (;;){
		uint message_size = 0;
		uint bytes_received = 0;
		
		// get first chunk which contains total message size
		int nbytes = recv(*sockfd, buf, BUFFERSIZE, 0);
		if (nbytes <= 0){
			(nbytes == 0) ? printf("connection closed\n") : printf("recv error\n");
			exit(-1);
		}
		
		bytes_received += nbytes - 4;
		
		// get total size of message
		message_size += (buf[0] && 0xFF) << 24;
		message_size += (buf[1] && 0xFF) << 16;
		message_size += (buf[2] && 0xFF) << 8;
		message_size += buf[3] && 0xFF;
		
		printf("DEBUG message_size: %d\n", message_size);
		
		buffer = malloc(message_size);
		memcpy(buffer, buf+4, nbytes);
		
		// get rest of message
		while (bytes_received < message_size){
			nbytes = recv(*sockfd, buffer + bytes_received, BUFFERSIZE, 0);
			if (nbytes <= 0){
				(nbytes == 0) ? printf("connection closed\n") : printf("recv error\n");
				exit(-1);
			}
			
			bytes_received += nbytes;
		}
		
		printf("DEBUG Recieved: %s", buffer);
	}
	return 0;	
}

void *agent_send(void *vargp){
	char *alive = "ALIVE\n";
	int *sockfd = (int*)vargp;
	uint message_size = 0;
	char buf[BUFFERSIZE];
	
	for (;;){
		// set first 4 bytes of message to message size
		buf[0] = (strlen(alive) >> 24) & 0xFF;
		buf[1] = (strlen(alive) >> 16) & 0xFF;
		buf[2] = (strlen(alive) >> 8) & 0xFF;
		buf[3] = strlen(alive) & 0xFF;
		
		memcpy(buf+4, alive, strlen(alive));
		
		send(*sockfd, buf, strlen(alive) + 4, 0);
		printf("Sent message\n");
		sleep(1);
	}
	return 0;	
}