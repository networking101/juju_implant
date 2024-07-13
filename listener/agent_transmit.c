#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include <sys/socket.h>

#include "agent_transmit.h"

#define BUFFERSIZE 256

void *agent_receive(void *vargp){
	int nbytes;
	char buf[BUFFERSIZE];
	
	struct agent_receive *ARS = (struct agent_receive *)vargp;
	int *fd_count = ARS->fd_count;
	struct pollfd *pfds = ARS->pfds;
	
    for (;;){
//		printf("Size of fd_count %d\n", *fd_count);
		int poll_count = poll(pfds, *fd_count, -1);
		printf("poll_count %d\n", poll_count);
		
		if (poll_count == 1){
			printf("poll failed\n");
			exit(-1);
		}
		
		for (int i = 0; i < *fd_count; i++){
			
			// check if ready to read
			if (pfds[i].revents & POLLIN){
				printf("%d\n", pfds[i].fd);
				nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
				
				if (nbytes <= 0){
					printf("recv error\n");
					if (nbytes == 0){
						printf("connection closed\n");
					}
					close(pfds[i].fd);
				} else{
					printf("%s\n", buf);
				}
			}
		}
		
		
		sleep(.5);
	}
	
    return 0;
}

void *agent_send(void *vargp){
	int nbytes;
	char buf[64];
	
	struct agent_receive *ARS = (struct agent_receive *)vargp;
	int *fd_count = ARS->fd_count;
	struct pollfd *pfds = ARS->pfds;
	
	for(;;){
//		printf("Size of fd_count %d\n", *fd_count);
//		sleep(1);
	}
    return 0;
}